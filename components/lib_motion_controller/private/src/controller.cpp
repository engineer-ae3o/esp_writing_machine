#include "controller.hpp"
#include "config.hpp"
#include "utils.hpp"

#include <cstring>


namespace gcode {

    controller_t::~controller_t() {
        cleanup();
    }

    controller_t& controller_t::get_instance(const config_t& config) {
        static controller_t instance(config);
        return instance;
    }

    void controller_t::create_session(const session_t& session) {
        // Check to see if we are not initialized or there is a currently active session
        ASSERT(m_is_initialized && !m_session_active);

        m_session = session;

        // Check the args
        ASSERT(m_session.session_done_cb);

        // Push event and wake the planner task
        event_t event{event_t::SESSION_CREATED};
        xQueueSend(m_event_queue, &event, portMAX_DELAY);

        m_session_active = true;
    }
    
    void controller_t::init(const config_t& config) {
        if (m_is_initialized) return;

        m_config = config;

        ASSERT(m_config.servo_set_angle);
        ASSERT(m_config.send_steps);
        ASSERT(m_config.queue_size > 0);
        ASSERT(m_config.task_priority > 0);
        ASSERT(m_config.task_stack_size > 0);

        m_event_queue = xQueueCreate(m_config.queue_size, sizeof(event_t));
        ASSERT(m_event_queue);

        BaseType_t ret = xTaskCreate(planner_task, "PlannerTask", m_config.task_stack_size,
                                     this, m_config.task_priority, nullptr);
        ASSERT(ret == pdPASS);

        m_is_initialized = true;
    }
    
    void controller_t::shutdown() {
        if (m_is_initialized) cleanup();
    }

    void controller_t::send_event(event_t event) {
        ASSERT(m_is_initialized);
        // The only way to create a session is through the `start_session()` function
        if (event == event_t::SESSION_CREATED) return;
        xQueueSend(m_event_queue, &event, portMAX_DELAY);
    }

    // Private functions
    controller_t::controller_t(const config_t& config) {
        init(config);
    }

    void controller_t::cleanup() {
        m_shutdown_requested = true;
        m_is_initialized = false;
    }

    void controller_t::control_motors(const parser::line_t& line) {
        // TODO: Control motors
    }

    error_t controller_t::get_error_from_parser_error(parser::error_t error) {
        switch (error) {
            case parser::error_t::INVALID_COMMAND:   return error_t::INVALID_COMMAND;
            case parser::error_t::INVALID_SYNTAX:    return error_t::INVALID_SYNTAX;
            case parser::error_t::MISSING_PARAMETER: return error_t::MISSING_PARAMETER;
            default:                                 return error_t::UNKNOWN;
        }
    }

    void controller_t::planner_task(void* arg) {

        utils::log<config::log_level_t::INFO>("Started planner task");

        auto driver{static_cast<controller_t*>(arg)};

        event_t event{event_t::NO_EVENT};
        session_done_t session_done{};
        size_t parse_error_count{};
        size_t line_count{};

        FILE* file_handle{};
        char gcode_line[config::MAX_GCODE_LINE_LENGTH]{};

        driver->m_state = {state_t::SLEEPING};
        driver->m_session = {};

        while (!driver->m_shutdown_requested) {
            switch (driver->m_state) {
                case state_t::SLEEPING:
                    // Sleep till a session has been created. All other events
                    // received in this state are ignored and have no effect
                    xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                    if (event != event_t::SESSION_CREATED) {
                        utils::log<config::log_level_t::WARN>("Invalid event for the current state (SLEEPING)");
                        break;
                    }

                    // Open the file and transition to the paused state on success
                    file_handle = fopen(driver->m_session.file_path, "r");
                    if (!file_handle) {
                        session_done.error = error_t::FILE_NOT_FOUND;
                        utils::log<config::log_level_t::ERROR>("Failed to open file. Failed to start session");
                        driver->m_state = state_t::STOPPED;
                    } else {
                        utils::log<config::log_level_t::INFO>("Session created. Waiting for the START_SESSION event");
                        driver->m_state = state_t::PAUSED;
                    }
                    
                    break;

                case state_t::RUNNING:
                    // Check for events periodically. The only events that change the
                    // state are the PAUSE_SESSION and STOP_SESSION events
                    if (xQueueReceive(driver->m_event_queue, &event, 0) == pdTRUE) {
                        if (event == event_t::PAUSE_SESSION) driver->m_state = state_t::PAUSED;
                        else if (event == event_t::STOP_SESSION) {
                            session_done.error = error_t::OPERATION_STOPPED_BEFORE_COMPLETION;
                            driver->m_state = state_t::STOPPED;
                        } else {
                            utils::log<config::log_level_t::WARN>("Invalid event for the current state (RUNNING)");
                        }
                        break;
                    }
                    
                    // Read file and check for errors
                    if (!fgets(gcode_line, sizeof(gcode_line), file_handle)) {
                        // End of file: this session is complete
                        if (feof(file_handle)) {
                            session_done.error = error_t::NONE;
                            utils::log<config::log_level_t::INFO>("Session complete");
                        }
                        // A file IO error occurred
                        else if (ferror(file_handle)) {
                            session_done.error = error_t::FILE_READ_ERROR;
                            utils::log<config::log_level_t::ERROR>("File IO error");
                        }
                        // Most likely won't get here, but incase
                        else {
                            session_done.error = error_t::UNKNOWN;
                            utils::log<config::log_level_t::ERROR>("Unknown error while reading file");
                        }

                        driver->m_state = state_t::STOPPED;
                        break;
                    }

                    // Increment count on every successful line read
                    line_count++;

                    auto ret = parser::parse_line(gcode_line);
                    if (!ret) {
                        parse_error_count++;

                        // Only end the session when we get past MAX_LINE_PARSE_ERROR errors
                        if (parse_error_count >= config::MAX_LINE_PARSE_ERROR) {
                            parse_error_count = 0;
                            utils::log<config::log_level_t::ERROR>("Too many parsing errors. Exiting session");

                            // Set error values
                            session_done.error = driver->get_error_from_parser_error(ret.error());
                            session_done.error_line.emplace();
                            strlcpy(session_done.error_line->data(), gcode_line, session_done.error_line->size());
                            session_done.line_num = line_count;

                            driver->m_state = state_t::STOPPED;
                        }
                        // Just log the parse error and move on
                        else {
                            utils::log<config::log_level_t::ERROR>("Failed to parse line %u: (%s)", line_count, gcode_line);
                        }
                        break;
                    }

                    // If we get here, that means the line has
                    // been read and parsed without any errors. 
                    driver->control_motors(ret.value());
                    break;

                case state_t::PAUSED:
                    // Sleep till we get a START_SESSION or RESUME_SESSION event.
                    // All other events are ignored
                    xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                    if ((event == event_t::START_SESSION) || (event == event_t::RESUME_SESSION)) driver->m_state = state_t::RUNNING;
                    else if (event == event_t::STOP_SESSION) {
                        session_done.error = error_t::OPERATION_STOPPED_BEFORE_COMPLETION;
                        driver->m_state = state_t::STOPPED;
                    } else {
                        utils::log<config::log_level_t::WARN>("Invalid event for the current state (PAUSED)");
                    }
                    break;

                case state_t::STOPPED:
                    // Call the user session done callback
                    driver->m_session.session_done_cb(session_done);

                    // Zero out variables in preparation for the next session (if any)
                    driver->m_session = {};
                    driver->m_session_active = false;
                    event = event_t::NO_EVENT;
                    parse_error_count = 0;
                    line_count = 0;
                    session_done = {};
                    memset(gcode_line, 0, sizeof(gcode_line));
                    fclose(file_handle);
                    file_handle = {};

                    utils::log<config::log_level_t::INFO>("Session (if any) stopped");

                    driver->m_state = state_t::SLEEPING;
                    break;

                default:
                    driver->m_state = state_t::SLEEPING;
                    break;
            }
        }

        driver->m_is_initialized = false;

        vQueueDelete(driver->m_event_queue);
        vTaskDelete(nullptr);
    }

} // namespace gcode
