#ifndef CONTROLLER_HPP_
#define CONTROLLER_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <cstdint>
#include <cstdio>


namespace gcode {
    
    struct config_t {
        void(*servo_set_angle)(uint32_t angle);
        void(*send_steps)(uint32_t steps, bool dir, uint32_t speed);
        void(*send_steps_accel)(uint32_t steps, bool dir, uint32_t speed, uint32_t accel);

        uint8_t queue_size{};
        uint8_t task_priority{};
        uint8_t task_stack_size{};
    };

    struct session_t {
        char* file_path{};
    };

    enum class event_t : uint8_t {
        NO_EVENT = 0,
        SESSION_CREATED,
        PAUSE_SESSION,
        RESUME_SESSION,
        STOP_SESSION,
        ERROR
    };

    class controller_t {
    private:
        config_t m_config{};
        session_t m_session{};

        TaskHandle_t m_task_handle{};
        QueueHandle_t m_event_queue{};

        enum class state_t : uint8_t {
            SLEEPING = 0,
            RUNNING,
            PAUSED,
            STOPPED
        };

        state_t m_state{state_t::SLEEPING};
        
    public:
        ~controller_t();
        
        // Remove the default constructor
        controller_t() = delete;

        // Non copyable
        controller_t(const controller_t&) = delete;
        controller_t& operator=(const controller_t&) = delete;

        // Non moveable
        controller_t(controller_t&&) = delete;
        controller_t& operator=(controller_t&&) = delete;

        // Useable functions
        static controller_t& get_instance(const config_t& config);
        void create_session(const session_t& session);
        void send_event(event_t event);
        void shutdown();

    private:
        controller_t(const config_t&);
        void cleanup();
        static void planner_task(void* arg);
    };

} // namespace gcode


#endif // CONTROLLER_HPP_