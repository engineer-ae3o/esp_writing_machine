#ifndef TYPES_HPP_
#define TYPES_HPP_


#include <optional>
#include <cstdint>
#include <array>

#include "config.hpp"


namespace gcode::types {

    enum class direction_t : uint8_t {
        FORWARD = 0,
        BACKWARD
    };

    struct config_t {
        // Set the servo motor's angle for up and down motion of the pen
        void(*servo_set_angle)(uint32_t angle){};

        // Send steps. Straightforward. Acceleration should
        // be handled internally by the hardware drivers
        void(*send_steps_x)(uint32_t steps, direction_t dir, uint32_t speed){};
        void(*send_steps_y)(uint32_t steps, direction_t dir, uint32_t speed){};

        // An optional callback that sends all steps x and y synchronously
        // without having to rely on Brasenham's line interpolation algorithm
        // It will be used if available over the above two when there is a
        // need to send x and y steps simultaneously. Note that the send x and
        // y steps functions still have be present even if this one is present
        std::optional<void(*)(uint32_t steps_x, direction_t dir_x, uint32_t speed_x,
                              uint32_t steps_y, direction_t dir_y, uint32_t speed_y)> send_steps_sync{std::nullopt};

        size_t queue_size{config::DEFAULT_QUEUE_SIZE};
        uint8_t task_priority{config::DEFAULT_TASK_PRIORITY};
        uint16_t task_stack_size_bytes{config::DEFAULT_TASK_STACK_SIZE_BYTES};
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
        std::array<char, config::MAX_FILE_NAME_LENGTH> file_path{};

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

} // namespace gcode::types


#endif // TYPES_HPP_