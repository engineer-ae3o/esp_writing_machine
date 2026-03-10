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

    constexpr inline log_level_t LOG_LEVEL = log_level_t::INFO;

}


#endif // CONFIG_HPP_