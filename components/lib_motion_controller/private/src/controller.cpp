#include "controller.hpp"
#include "config.hpp"
#include "utils.hpp"


namespace gcode {

    controller_t::~controller_t() {
        cleanup();
    }

    controller_t& controller_t::get_instance(const config_t& config) {
        static controller_t instance(config);
        return instance;
    }

    void controller_t::create_session(const session_t& session) {
        // Return if we are not initialized or there is a currently active session
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

        auto driver{static_cast<controller_t*>(arg)};

        event_t event{event_t::NO_EVENT};
        error_t error{error_t::NONE};
        size_t parse_error_count{};

        FILE* file_handle{};
        char gcode_line[config::MAX_GCODE_LINE_LENGTH]{};

        driver->m_state = {state_t::SLEEPING};
        driver->m_session = {};

        while (!driver->m_shutdown_requested) {
            switch (driver->m_state) {
                case state_t::SLEEPING:
                    // Sleep till a session has been created
                    xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                    if (event == event_t::SESSION_CREATED) {
                        // Open the file and transition to the paused state on success
                        file_handle = fopen(driver->m_session.file_path, "r");
                        if (!file_handle) {
                            error = error_t::FILE_NOT_FOUND;
                            driver->m_state = state_t::STOPPED;
                        } else {
                            driver->m_state = state_t::PAUSED;
                        }
                    }
                    break;

                case state_t::RUNNING:
                    if (xQueueReceive(driver->m_event_queue, &event, 0) == pdTRUE) {
                        if (event == event_t::PAUSE_SESSION) driver->m_state = state_t::PAUSED;
                        else if (event == event_t::STOP_SESSION) {
                            error = error_t::OPERATION_STOPPED_BEFORE_COMPLETION;
                            driver->m_state = state_t::STOPPED;
                        };
                        break;
                    }
                    
                    if (!fgets(gcode_line, sizeof(gcode_line), file_handle)) {
                        if (feof(file_handle)) {
                            error = error_t::OPERATION_COMPLETED_SUCCESSFULLY;
                        } else if (ferror(file_handle)) {
                            error = error_t::FILE_READ_ERROR;
                        } else {
                            error = error_t::UNKNOWN;
                        }
                        driver->m_state = state_t::STOPPED;
                        break;
                    }

                    auto ret = parser::parse_line(gcode_line);
                    if (!ret) {
                        parse_error_count++;
                        if (parse_error_count >= config::MAX_LINE_PARSE_ERROR) {
                            parse_error_count = 0;
                            error = driver->get_error_from_parser_error(ret.error());
                            driver->m_state = state_t::STOPPED;
                        }
                        break;
                    }

                    // If we get here, that means the line has
                    // been read and parsed without any errors. 
                    driver->control_motors(ret.value());
                    break;

                case state_t::PAUSED:
                    xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                    if ((event == event_t::START_SESSION) || (event == event_t::RESUME_SESSION)) driver->m_state = state_t::RUNNING;
                    else if (event == event_t::STOP_SESSION) {
                        error = error_t::OPERATION_STOPPED_BEFORE_COMPLETION;
                        driver->m_state = state_t::STOPPED;
                    }
                    break;

                case state_t::STOPPED:
                    driver->m_session.session_done_cb(error);
                    driver->m_session = {};
                    driver->m_session_active = false;

                    error = error_t::NONE;
                    file_handle = {};

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
