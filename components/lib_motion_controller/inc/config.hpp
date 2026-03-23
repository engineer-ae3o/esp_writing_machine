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
    constexpr inline log_level_t LOG_LEVEL                   = log_level_t::INFO;

    // Turn asserts on or off
    constexpr inline bool ASSERTS_ENABLED                    = true;
    
    // Tag used for logging
    constexpr inline const char TAG[]                        = "Gcode-Planner";

    // File specific details
    constexpr inline uint32_t MAX_FILE_NAME_LENGTH           = 64;
    constexpr inline uint32_t MAX_GCODE_LINE_LENGTH          = 128;
    constexpr inline uint32_t MAX_LINE_PARSE_ERROR           = 5;

    // How many degrees the servo moves the pen for an up or down motion
    constexpr inline uint32_t SERVO_PEN_UP_ANGLE             = 30;
    constexpr inline uint32_t SERVO_PEN_DOWN_ANGLE           = 90;
    
    // How long it takes the servo motor to move the pen
    constexpr inline uint32_t PEN_DWELL_TIME_MS              = 30;

    // Hardware dependent settings. Set as per your hardware
    // Stepper rating
    constexpr inline float STEPPER_DEGREE_PER_STEP           = 1.8;
    
    // Number of steps the stepper motor needs to make a full revolution
    constexpr inline float STEPS_PER_FULL_REV                = 360.0f / STEPPER_DEGREE_PER_STEP;

    // Microstepping
    constexpr inline uint32_t MICROSTEP                      = 16;
    
    // Drive mechanism: how many milimetres make a revolution
    constexpr inline float MM_PER_REV                        = 40;
    
    // Conversion factor from milimetres to steps: How many steps needed
    // to move 1mm. Depends on the microstepping, steps per revolution and
    // milimetres per full revolution
    constexpr inline float STEPS_PER_MM                      = MICROSTEP * STEPS_PER_FULL_REV / MM_PER_REV;

    // Default feedrate. Used for rapid moves `(mm/min)`
    constexpr inline uint32_t DEFAULT_FEED_RATE              = 500;

    // Some FreeRTOS primitive defaults
    constexpr inline uint8_t DEFAULT_TASK_PRIORITY           = 10;
    constexpr inline size_t DEFAULT_QUEUE_SIZE               = 10;

    // If your implementation of FreeRTOS takes in words as
    // opposed to bytes, apply the appropriate conversions
    // as the library assumes bytes
    constexpr inline uint16_t DEFAULT_TASK_STACK_SIZE_BYTES  = 3072;
    
} // namespace gcode::config


#endif // CONFIG_HPP_