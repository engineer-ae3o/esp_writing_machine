#ifndef UTILS_HPP_
#define UTILS_HPP_


#include "esp_log.h"
#include "esp_system.h"

#include "config.hpp"

#include <source_location>


namespace gcode::utils {

    template <config::log_level_t level, typename... Args>
    void log(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(level) <= static_cast<uint8_t>(config::LOG_LEVEL)) {
            // This is a work around. The `ESP_LOGx()`
            // macros expect a string literal
            char msg[128]{};
            snprintf(msg, sizeof(msg), fmt, std::forward<Args>(args)...);
            
            if constexpr (level == config::log_level_t::ERROR)     ESP_LOGE(config::TAG, "%s", msg);
            else if constexpr (level == config::log_level_t::WARN) ESP_LOGW(config::TAG, "%s", msg);
            else if constexpr (level == config::log_level_t::INFO) ESP_LOGI(config::TAG, "%s", msg);
        }
    }

    [[noreturn]] inline void panic(const char* msg) {
        esp_system_abort(msg);
        while (1);
    }
    
    inline void assert_check(bool cond, const char* msg, const std::source_location& loc = std::source_location::current()) {
        if constexpr (config::ASSERTS_ENABLED) {
            if (!cond) {
                log<config::log_level_t::ERROR>("ASSERT", "Assert failed (%s). File: %s. Line: %u. Function: %s",
                                        msg, loc.file_name(), loc.line(), loc.function_name());
                panic(msg);
            }
        }
    }

} // namespace utils


#define ASSERT(cond) utils::assert_check((cond), #cond)


#endif // UTILS_HPP_