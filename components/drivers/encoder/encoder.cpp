#include "encoder.hpp"

#include "driver/pulse_cnt.h"
#include "esp_timer.h"
#include "esp_log.h"


namespace enc {

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    static constexpr log_level_t LOG_LEVEL = log_level_t::INFO;
    static constexpr const char* TAG = "Encoder";

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

    encoder_t::~encoder_t() {
        
        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to cleanup resources: %s", esp_err_to_name(ret));
        }

        log<log_level_t::INFO>("Destructed instance successfully: %s", esp_err_to_name(ret));
    }
    
    encoder_t::encoder_t(encoder_t&& other) {
        // Get all of the other class' resources
        m_is_initialized = other.m_is_initialized;
        m_pin_a = other.m_pin_a;
        m_pin_b = other.m_pin_b;
        m_counter = other.m_counter;
        m_unit_handle = other.m_unit_handle;
        m_chan_handle = other.m_chan_handle;
        m_event_queue = other.m_event_queue;
        m_last_time_us = other.m_last_time_us;

        // Invalidate the other class
        other.m_is_initialized = false;
        other.m_pin_a = GPIO_NUM_NC;
        other.m_pin_b = GPIO_NUM_NC;
        other.m_counter = {};
        other.m_unit_handle = nullptr;
        other.m_chan_handle = nullptr;
        other.m_event_queue = nullptr;
        other.m_last_time_us = {};
    }

    encoder_t& encoder_t::operator=(encoder_t&& other) {
        if (this != &other) {
            // Destroy the current class' instance
            cleanup();

            // Get all of the other class' resources
            m_is_initialized = other.m_is_initialized;
            m_pin_a = other.m_pin_a;
            m_pin_b = other.m_pin_b;
            m_counter = other.m_counter;
            m_unit_handle = other.m_unit_handle;
            m_chan_handle = other.m_chan_handle;
            m_event_queue = other.m_event_queue;
            m_last_time_us = other.m_last_time_us;

            // Invalidate the other class
            other.m_is_initialized = false;
            other.m_pin_a = GPIO_NUM_NC;
            other.m_pin_b = GPIO_NUM_NC;
            other.m_counter = {};
            other.m_unit_handle = nullptr;
            other.m_chan_handle = nullptr;
            other.m_event_queue = nullptr;
            other.m_last_time_us = 0LL;
        }
        
        return *this;
    }

    std::expected<std::unique_ptr<encoder_t>, esp_err_t> encoder_t::create(gpio_num_t pin_a, gpio_num_t pin_b) {
        // Object has to be heap allocated for it to be returned
        auto encoder = std::unique_ptr<encoder_t>(new encoder_t(pin_a, pin_b));

        esp_err_t ret = encoder->init();
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Initialization failed: %s", esp_err_to_name(ret));
            encoder->cleanup();
            return std::unexpected(ret);
        }
        
        return encoder;
    }

    [[nodiscard]] esp_err_t encoder_t::init() {

        if (m_is_initialized) {
            log<log_level_t::WARN>("Encoder already initialized");
            return ESP_ERR_INVALID_STATE;
        }

        log<log_level_t::INFO>("Starting initialization of encoder unit");

        constexpr pcnt_unit_config_t unit_config = {
            // Arbitrary values are used here
            // as they're irrelevant
            .low_limit = -5,
            .high_limit = 5,
            .intr_priority = 3,
            .flags = {
                .accum_count = 0
            }
        };

        esp_err_t ret = pcnt_new_unit(&unit_config, &m_unit_handle);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to create new pcnt unit: %s", esp_err_to_name(ret));
            return ret;
        }

        const pcnt_chan_config_t chan_config = {
            .edge_gpio_num = m_pin_a,
            .level_gpio_num = m_pin_b,
            .flags = {
                .invert_edge_input = 0,
                .invert_level_input = 0,
                .virt_edge_io_level = 0,
                .virt_level_io_level = 0,
                .io_loop_back = 0,
            }
        };

        ret = pcnt_new_channel(m_unit_handle, &chan_config, &m_chan_handle);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to create new pcnt channel: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Set a glitch filter ignore value of 1us
        constexpr pcnt_glitch_filter_config_t glitch_config = {
            .max_glitch_ns = 1'000
        };

        ret = pcnt_unit_set_glitch_filter(m_unit_handle, &glitch_config);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to set glitch filter: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        constexpr pcnt_event_callbacks_t cb_config = {
            .on_reach = event_cb
        };

        ret = pcnt_unit_register_event_callbacks(m_unit_handle, &cb_config, this);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to register event callback: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Set actions for both gpio pins. This is what enables the
        // PCNT unit to act as a quadrature decoder
        ret = pcnt_channel_set_edge_action(m_chan_handle, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to set edge action: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        ret = pcnt_channel_set_level_action(m_chan_handle, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to set level action: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Add watchpoints
        ret = pcnt_unit_add_watch_point(m_unit_handle, 1);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to add watchpoint of 1: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        ret = pcnt_unit_add_watch_point(m_unit_handle, -1);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to add watchpoint of -1: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // It is recommended by the docs to clear the count for the updates
        // to the watchpoints to take effect
        ret = pcnt_unit_clear_count(m_unit_handle);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to clear pcnt unit counter: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Enable the unit
        ret = pcnt_unit_enable(m_unit_handle);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to enable pcnt unit: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Create event queue
        m_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(event_t));
        if (!m_event_queue) {
            log<log_level_t::ERROR>("Failed to create event queue");
            cleanup();
            return ESP_ERR_NO_MEM;
        }
        
        log<log_level_t::INFO>("Finished initialization of encoder unit");
        m_is_initialized = true;

        return ESP_OK;
    }

    [[nodiscard]] esp_err_t encoder_t::deinit() {

        if (!m_is_initialized) {
            log<log_level_t::WARN>("Encoder already in an uninitialized state");
            return ESP_ERR_INVALID_STATE;
        }
        
        log<log_level_t::INFO>("Starting deinitialization of encoder unit");

        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to deinitialize encoder instance: %s", esp_err_to_name(ret));
            return ret;
        }

        log<log_level_t::INFO>("Finished deinitialization of encoder unit");

        return ESP_OK;
    }

    [[nodiscard]] esp_err_t encoder_t::start() {
        return pcnt_unit_start(m_unit_handle);
    }

    [[nodiscard]] esp_err_t encoder_t::stop() {
        return pcnt_unit_stop(m_unit_handle);
    }

    [[nodiscard]] QueueHandle_t encoder_t::get_event_queue() {
        return m_event_queue;
    }

    // Private methods
    encoder_t::encoder_t(gpio_num_t pin_a, gpio_num_t pin_b) : m_pin_a(pin_a), m_pin_b(pin_b) {}

    bool IRAM_ATTR encoder_t::event_cb(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t* edata, void* user_ctx) {
        
        auto encoder = static_cast<encoder_t*>(user_ctx);
        BaseType_t higher_priority_task_woken{false};

        int64_t now = esp_timer_get_time();

        // If the difference between now and the last detected pulse is
        // less than `RATE_LIMIT_MS`, we simply return
        if ((now - encoder->m_last_time_us) < (RATE_LIMIT_MS * 1'000LL)) return false;

        encoder->m_last_time_us = now;
        
        enc::event_t event{event_t::NO_EVENT};
        switch (edata->watch_point_value) {
        case 1:
            encoder->m_counter++;
            if (encoder->m_counter == LIMIT) event = event_t::CLOCKWISE;
            break;
        case -1:
            encoder->m_counter--;
            if (encoder->m_counter == -LIMIT) event = event_t::COUNTERCLOCKWISE;
            break;
        default:
            break;
        }

        // The `v5.3.1` docs say it is safe to call this from ISR context
        pcnt_unit_clear_count(encoder->m_unit_handle);

        // Reset counters when we reach the high and low limits.
        // The reason it's like this is to combat noise on the
        // lines. Software debouncing, rate limiting and counting
        // is the best I can do for now
        if ((encoder->m_counter == LIMIT) || (encoder->m_counter == -LIMIT)) encoder->m_counter = 0;

        if (event != event_t::NO_EVENT) xQueueSendFromISR(encoder->m_event_queue, &event, &higher_priority_task_woken);

        return higher_priority_task_woken == pdTRUE;
    }

    esp_err_t encoder_t::cleanup() {

        esp_err_t ret{ESP_OK};

        if (m_unit_handle) {
            // Remove all watchpoints
            // The return values can be ignored
            // as failure is not detrimental to
            // the encoder deinitialization
            pcnt_unit_remove_watch_point(m_unit_handle, 1);
            pcnt_unit_remove_watch_point(m_unit_handle, -1);

            ret = pcnt_unit_stop(m_unit_handle);
            if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                log<log_level_t::ERROR>("Failed to stop pcnt counting: %s", esp_err_to_name(ret));
                return ret;
            }

            ret = pcnt_unit_disable(m_unit_handle);
            if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                log<log_level_t::ERROR>("Failed to disable pcnt unit: %s", esp_err_to_name(ret));
                return ret;
            }
        }
        
        if (m_chan_handle) {
            ret = pcnt_del_channel(m_chan_handle);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to delete pcnt channel: %s", esp_err_to_name(ret));
                return ret;
            }
            m_chan_handle = nullptr;
        }
        
        if (m_unit_handle) {
            ret = pcnt_del_unit(m_unit_handle);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to delete pcnt unit: %s", esp_err_to_name(ret));
                return ret;
            }
            m_unit_handle = nullptr;
        }

        if (m_event_queue) {
            vQueueDelete(m_event_queue);
            m_event_queue = nullptr;
        }
        
        m_counter = 0;
        m_is_initialized = false;

        return ESP_OK;
    }

} // namespace enc
