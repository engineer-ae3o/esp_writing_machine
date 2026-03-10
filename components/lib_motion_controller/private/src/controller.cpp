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
        ASSERT(m_session.file_path);
        ASSERT(m_session.session_done_cb);

        // Push event and wake the planner task
        event_t event{event_t::SESSION_CREATED};
        if (m_event_queue) xQueueSend(m_event_queue, &event, portMAX_DELAY);

        m_session_active = true;
    }
    
    void controller_t::init() {
        if (m_is_initialized) return;

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
    controller_t::controller_t(const config_t& config) : m_config(config) {
        init();
    }

    void controller_t::cleanup() {
        m_shutdown_requested = true;
        m_is_initialized = false;
    }

    void controller_t::planner_task(void* arg) {

        auto driver{static_cast<controller_t*>(arg)};

        event_t event{event_t::NO_EVENT};
        parser::error_t error{parser::error_t::NONE};
        parser::line_t line{};

        FILE* file_handle{};
        char gcode_line[128]{};

        driver->m_state = {state_t::SLEEPING};
        driver->m_session = {};

        while (!driver->m_shutdown_requested) {
            switch (driver->m_state) {
                case state_t::SLEEPING:
                    // Sleep till a session has been created
                    xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                    if (event == event_t::SESSION_CREATED) {
                        driver->m_state = state_t::PAUSED;
                        file_handle = fopen(driver->m_session.file_path, "r");
                        ASSERT(file_handle);
                    }
                    break;

                case state_t::RUNNING:
                    if (xQueueReceive(driver->m_event_queue, &event, 0) == pdTRUE) {
                        if (event == event_t::PAUSE_SESSION) driver->m_state = state_t::PAUSED;
                        else if (event == event_t::STOP_SESSION) driver->m_state = state_t::STOPPED;
                        break;
                    }

                    // TODO: Finish implementation
                    auto ret = fgets(gcode_line, 0, file_handle);

                    break;

                case state_t::PAUSED:
                    xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                    if ((event == event_t::START_SESSION) || (event == event_t::RESUME_SESSION)) driver->m_state = state_t::RUNNING;
                    else if (event == event_t::STOP_SESSION) driver->m_state = state_t::STOPPED;
                    break;

                case state_t::STOPPED:
                    driver->m_session.session_done_cb(error);
                    driver->m_session = {};
                    driver->m_session_active = false;
                    driver->m_state = state_t::SLEEPING;
                    break;

                default:
                    driver->m_state = state_t::SLEEPING;
                    break;
            }
        }

        vQueueDelete(driver->m_event_queue);
        vTaskDelete(nullptr);
    }

} // namespace gcode
