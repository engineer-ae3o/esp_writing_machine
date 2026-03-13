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
        m_session = session;
        event_t event{event_t::SESSION_CREATED};
        if (m_event_queue) xQueueSend(m_event_queue, &event, portMAX_DELAY);
    }
    
    void controller_t::shutdown() {
        cleanup();
    }

    void controller_t::send_event(event_t event) {
        // The only way to create a session is through the `start_session()` function
        if (event == event_t::SESSION_CREATED) return;
        if (m_event_queue) xQueueSend(m_event_queue, &event, portMAX_DELAY);
    }

    // Private functions
    controller_t::controller_t(const config_t& config) : m_config(config) {
        m_event_queue = xQueueCreate(m_config.queue_size, sizeof(event_t));
        if (!m_event_queue) {
            utils::log<config::log_level_t::ERROR>("Failed to create the event queue");
            ASSERT(0);
        }

        BaseType_t ret = xTaskCreate(planner_task, "planner_task", m_config.task_stack_size,
                                     this, m_config.task_priority, &m_task_handle);
        if (ret != pdPASS) {
            utils::log<config::log_level_t::ERROR>("Failed to create the planner task");
            ASSERT(0);
        }
    }

    void controller_t::cleanup() {
        if (m_event_queue) {
            vQueueDelete(m_event_queue);
            m_event_queue = nullptr;
        }

        if (m_task_handle) {
            vTaskDelete(m_task_handle);
            m_task_handle = nullptr;
        }
    }

    void controller_t::planner_task(void* arg) {

        auto driver{static_cast<controller_t*>(arg)};

        event_t event{event_t::NO_EVENT};
        driver->m_state = {state_t::SLEEPING};
        driver->m_session = {};

        while (1) {

            switch (driver->m_state) {
            case state_t::SLEEPING:
                // Sleep till a session has been created
                xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                if (event == event_t::SESSION_CREATED) driver->m_state = state_t::RUNNING;
                break;

            case state_t::RUNNING:
                if (xQueueReceive(driver->m_event_queue, &event, 0) == pdTRUE) {
                    if (event == event_t::PAUSE_SESSION) driver->m_state = state_t::PAUSED;
                    else if (event == event_t::STOP_SESSION) driver->m_state = state_t::STOPPED;
                }

                // TODO: Handle motion planning

                break;

            case state_t::PAUSED:
                xQueueReceive(driver->m_event_queue, &event, portMAX_DELAY);
                if (event == event_t::RESUME_SESSION) driver->m_state = state_t::RUNNING;
                else if (event == event_t::STOP_SESSION) driver->m_state = state_t::STOPPED;
                break;

            case state_t::STOPPED:
                driver->m_session = {};
                driver->m_state = state_t::SLEEPING;
                break;

            default:
                driver->m_state = state_t::SLEEPING;
                break;
            }
        }
    }

} // namespace gcode
