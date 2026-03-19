#ifndef CONFIG_HPP_
#define CONFIG_HPP_


#include <cstdint>


namespace gcode::config {

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    // Control the logging level and output
    constexpr inline log_level_t LOG_LEVEL               = log_level_t::INFO;

    // Turn asserts on or off
    constexpr inline bool ASSERTS_ENABLED                = true;
    
    // Tag used for logging
    constexpr inline const char TAG[]                    = "Gcode-Planner";

    // File specific details
    constexpr inline uint32_t MAX_FILE_NAME_LENGTH       = 64;
    constexpr inline uint32_t MAX_GCODE_LINE_LENGTH      = 128;
    constexpr inline uint32_t MAX_LINE_PARSE_ERROR       = 5;

    // How many degrees the servo moves the pen for an up or down motion
    constexpr inline uint32_t SERVO_PEN_UP_ANGLE         = 30;
    constexpr inline uint32_t SERVO_PEN_DOWN_ANGLE       = 90;
    
    // How long it takes the servo motor to move the pen
    constexpr inline uint32_t PEN_DWELL_TIME_MS          = 30;

    // Microstepping
    constexpr inline uint32_t MICROSTEP                  = 16;

    // Default feedrate. Used for rapid moves
    constexpr inline uint32_t DEFAULT_FEED_RATE          = 500;
    
} // namespace gcode::config


#endif // CONFIG_HPP_