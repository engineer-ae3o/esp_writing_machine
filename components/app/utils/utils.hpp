#ifndef UTILS_HPP_
#define UTILS_HPP_


#include "esp_log.h"
#include "esp_system.h"

#include <source_location>


namespace utils {

    constexpr inline bool ASSERTS_ENABLED{true};

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    constexpr inline log_level_t LOG_LEVEL = log_level_t::INFO;

    template <log_level_t level, typename... Args>
    void log(const char* tag, const char* fmt, Args&&... args) {
        if constexpr (level <= LOG_LEVEL) {
            // This is a work around. The `ESP_LOGx`
            // macros expect a string literal
            char msg[128]{};
            snprintf(msg, sizeof(msg), fmt, args...);
            if constexpr (level == log_level_t::ERROR)     ESP_LOGE(tag, "%s", msg);
            else if constexpr (level == log_level_t::WARN) ESP_LOGW(tag, "%s", msg);
            else if constexpr (level == log_level_t::INFO) ESP_LOGI(tag, "%s", msg);
        }
    }

    [[noreturn]] inline void panic(const char* msg) {
        esp_system_abort(msg);
        while (1);
    }
    
    inline void assert_check(bool cond, const char* msg, const std::source_location& loc = std::source_location::current()) {
        if constexpr (ASSERTS_ENABLED) {
            if (!cond) {
                log<log_level_t::ERROR>("ASSERT", "Assert failed (%s). File: %s. Line: %u. Function: %s",
                                        msg, loc.file_name(), loc.line(), loc.function_name());
                panic(msg);
            }
        }
    }

} // namespace utils


#define ASSERT(cond) utils::assert_check((cond), #cond)


#endif // UTILS_HPP_