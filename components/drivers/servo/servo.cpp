#include "servo.hpp"

#include "esp_err.h"
#include "esp_log.h"


namespace servo {

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    static constexpr log_level_t LOG_LEVEL = log_level_t::INFO;
    static constexpr const char* TAG = "Servo";

    template <log_level_t level, typename... Args>
    void log(const char* fmt, Args&&... args) {
        if constexpr (level <= LOG_LEVEL) {
            // This is a work around. The ESP_LOGx macros
            // expect a string literal
            char msg[256]{};
            snprintf(msg, sizeof(msg), fmt, args...);
            if constexpr (level == log_level_t::ERROR)     ESP_LOGE(TAG, "%s", msg);
            else if constexpr (level == log_level_t::WARN) ESP_LOGW(TAG, "%s", msg);
            else if constexpr (level == log_level_t::INFO) ESP_LOGI(TAG, "%s", msg);
        }
    }

    static constexpr float MAX_ANGLE               = 180.0f;
    static constexpr float MIN_ANGLE               = 0.0f;
    static constexpr float MAX_PULSE_WIDTH_US      = 2500.0f;
    static constexpr float MIN_PULSE_WIDTH_US      = 500.0f;

    servo_t::servo_t(const config_t& config) : m_config(config) {}

    servo_t::~servo_t() {
        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to properly destruct servo instance: %s", esp_err_to_name(ret));
        }
    }

    servo_t::servo_t(servo_t&& other) {
        m_is_initialized = other.m_is_initialized;
        m_config = other.m_config;
        m_max_duty_cycle = other.m_max_duty_cycle;
        m_max_pulse_width_us = other.m_max_pulse_width_us;

        other.m_is_initialized = false;
        other.m_config = {};
        other.m_max_duty_cycle = 0.0f;
        other.m_max_pulse_width_us = 0.0f;
    }

    servo_t& servo_t::operator=(servo_t&& other) {
        if (this != &other) {
            // Free previously used resources if any
            cleanup();

            m_is_initialized = other.m_is_initialized;
            m_config = other.m_config;
            m_max_duty_cycle = other.m_max_duty_cycle;
            m_max_pulse_width_us = other.m_max_pulse_width_us;

            other.m_is_initialized = false;
            other.m_config = {};
            other.m_max_duty_cycle = 0.0f;
            other.m_max_pulse_width_us = 0.0f;
        }
        
        return *this;
    }

    std::pair<std::optional<servo_t>, esp_err_t> servo_t::create(const config_t& config) {
        servo_t servo(config);

        esp_err_t ret = servo.init();
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to initialize servo instance");
            servo.cleanup();
            return {std::nullopt, ret};
        }

        return {std::move(servo), ESP_OK};
    }

    [[nodiscard]] esp_err_t servo_t::init() {

        if (m_is_initialized) {
            log<log_level_t::WARN>("Servo instance already initialized");
            return ESP_ERR_INVALID_STATE;
        }

        log<log_level_t::INFO>("Initializing servo motor instance");

        // Initialize servo motor gpio timer
        const ledc_timer_config_t servo_timer_config_args = {
            .speed_mode = m_config.mode,
            .duty_resolution = m_config.resolution,
            .timer_num = m_config.timer,
            .freq_hz = static_cast<uint32_t>(m_config.frequency),
            .clk_cfg = LEDC_AUTO_CLK,
            .deconfigure = false
        };

        esp_err_t ret = ledc_timer_config(&servo_timer_config_args);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to initialize servo timer: %s", esp_err_to_name(ret));
            return ret;
        }

        // Initialize servo motor gpio channel
        const ledc_channel_config_t servo_channel_config_args = {
            .gpio_num = static_cast<int>(m_config.pin),
            .speed_mode = m_config.mode,
            .channel = m_config.channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = m_config.timer,
            .duty = 0,
            .hpoint = 0,
            .flags = { .output_invert = 0 }
        };

        ret = ledc_channel_config(&servo_channel_config_args);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to initialize servo channel: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Find max duty cycle from resolution
        m_max_duty_cycle = (1 << static_cast<uint32_t>(m_config.resolution)) - 1;
        // Find max pulse width from frequency
        m_max_pulse_width_us = 1'000'000 / m_config.frequency;

        // Apply defaults
        ledc_set_duty(m_config.mode, m_config.channel, 0);
        ledc_update_duty(m_config.mode, m_config.channel);

        m_is_initialized = true;

        log<log_level_t::INFO>("Done initializing servo motor instance");

        return ESP_OK;
    }

    [[nodiscard]] esp_err_t servo_t::deinit() {

        if (!m_is_initialized) {
            log<log_level_t::WARN>("Servo instance already in an uninitialized state");
            return ESP_ERR_INVALID_STATE;
        }

        log<log_level_t::INFO>("Deinitializing servo instance");

        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to properly deinitialize servo instance");
            return ret;
        }

        log<log_level_t::INFO>("Servo instance fully deinitialized");

        return ESP_OK;
    }

    bool servo_t::set_angle(uint32_t angle) {

        if (angle > static_cast<uint32_t>(MAX_ANGLE)) return false;

        // Find pulse width first from angle and pulse ranges
        const float fraction = (angle - MIN_ANGLE) / (MAX_ANGLE - MIN_ANGLE);
        const float pulse_us = ((MAX_PULSE_WIDTH_US - MIN_PULSE_WIDTH_US) * fraction) + MIN_PULSE_WIDTH_US;

        // Find duty cycle from pulse width
        const uint32_t duty_cycle = pulse_us * m_max_duty_cycle / m_max_pulse_width_us;

        // Apply duty cycle to servo
        ledc_set_duty(m_config.mode, m_config.channel, duty_cycle);
        ledc_update_duty(m_config.mode, m_config.channel);

        return true;
    }

    // Private functions
    esp_err_t servo_t::cleanup() {

        esp_err_t ret = ledc_stop(m_config.mode, m_config.channel, 0);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to stop pwm output: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = ledc_timer_pause(m_config.mode, m_config.timer);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to stop ledc timer: %s", esp_err_to_name(ret));
            return ret;
        }

        ledc_timer_config_t servo_timer_deconfig_args{};

        servo_timer_deconfig_args.speed_mode = m_config.mode;
        servo_timer_deconfig_args.timer_num = m_config.timer;
        servo_timer_deconfig_args.deconfigure = true;
        
        ret = ledc_timer_config(&servo_timer_deconfig_args);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to deinitialize servo timer: %s", esp_err_to_name(ret));
            return ret;
        }

        m_is_initialized = false;

        return ESP_OK;
    }

} // namespace servo
