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

    constexpr inline log_level_t LOG_LEVEL               = log_level_t::INFO;
    constexpr inline bool ASSERTS_ENABLED                = true;
    
    constexpr inline const char TAG[]                    = "Gcode-Planner";

    constexpr inline std::size_t MAX_FILE_NAME_LENGTH    = 64;
    constexpr inline std::size_t MAX_GCODE_LINE_LENGTH   = 128;

    constexpr inline std::size_t MAX_LINE_PARSE_ERROR    = 5;
    
} // namespace gcode::config


#endif // CONFIG_HPP_