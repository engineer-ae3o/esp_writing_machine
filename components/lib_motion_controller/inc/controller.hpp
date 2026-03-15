#ifndef CONTROLLER_HPP_
#define CONTROLLER_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "parser.hpp"
#include "config.hpp"

#include <cstdint>
#include <cstdio>
#include <array>


namespace gcode::controller {
    
    struct config_t {
        // Set the servo motor's angle for up and down motion of the pen
        void(*servo_set_angle)(uint32_t angle){};

        // Send steps. Straightforward. `true` represents the forward direction
        void(*send_steps)(uint32_t steps, bool dir, uint32_t speed, uint32_t accel){};

        uint8_t queue_size{};
        uint8_t task_priority{};
        uint16_t task_stack_size{};
    };

    enum class error_t : uint8_t {
        NONE = 0,
        UNKNOWN,
        FILE_NOT_FOUND,
        FILE_READ_ERROR,
        OPERATION_STOPPED_BEFORE_COMPLETION,
        // Parser errors
        INVALID_COMMAND, 
        INVALID_SYNTAX,
        MISSING_PARAMETER
    };

    struct session_done_t {
        error_t error{error_t::NONE};
        // These are only valid in the case of a line parsing error
        std::optional<std::array<char, config::MAX_GCODE_LINE_LENGTH>> error_line{std::nullopt};
        std::optional<size_t> line_num{std::nullopt};
    };

    struct session_t {
        // Path of the file to be opened, parsed and plan the motion
        char file_path[config::MAX_FILE_NAME_LENGTH]{};

        // This is called when a session is completed or hits an error.
        // The error, if any, is passed to the callback
        void (*session_done_cb)(const session_done_t&);
    };

    enum class event_t : uint8_t {
        NO_EVENT = 0,
        SESSION_CREATED,                /** This creates a session, but doesn't start it. Puts
                                            the device in the `PAUSED` state */
        PAUSE_SESSION,                  /** To pause a currently running session. It is ignored
                                            if the state is already paused or sleeping. Puts the
                                            in the `PAUSED` state */
        START_SESSION,                  /** Starts a previously created session. Puts the device
                                            in the `RUNNING` state */ 
        RESUME_SESSION = START_SESSION, /** Same semantics as `START_SESSION`, but used to indicate
                                            we were paused before */
        STOP_SESSION                    /** Completely stops the currently active session. Puts the
                                            device in the `STOPPED` state, then the`SLEEPING` state */
    };

    enum class state_t : uint8_t {
        SLEEPING = 0,
        RUNNING,
        PAUSED,
        STOPPED
    };

    class controller_t {
    private:
        config_t m_config{};
        session_t m_session{};

        bool m_is_initialized{};
        bool m_session_active{};
        bool m_shutdown_requested{};

        QueueHandle_t m_event_queue{};

        state_t m_state{state_t::SLEEPING};
        
    public:
        // Remove the default constructor
        controller_t() = delete;

        // Will never run as it's a single static instance, but good practice
        ~controller_t();

        // Non copyable
        controller_t(const controller_t&) = delete;
        controller_t& operator=(const controller_t&) = delete;

        // Non moveable
        controller_t(controller_t&&) = delete;
        controller_t& operator=(controller_t&&) = delete;

        // Useable functions
        static controller_t& get_instance(const config_t& config);
        void init(const config_t& config);
        void create_session(const session_t& session);
        void send_event(event_t event);
        state_t get_state();
        void shutdown();
        constexpr const char* event_to_string(event_t event);
        constexpr const char* controller_state_to_string(state_t state);

    private:
        controller_t(const config_t&);
        void cleanup();
        static void planner_task(void* arg);
        error_t get_error_from_parser_error(parser::error_t error);
    };

} // namespace gcode::controller


#endif // CONTROLLER_HPP_