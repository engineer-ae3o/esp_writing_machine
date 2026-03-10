#include "buttons.hpp"

#include "driver/ledc.h"
#include "esp_log.h"

#include <utility>


namespace btn {

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    static constexpr log_level_t LOG_LEVEL = log_level_t::INFO;
    static constexpr const char* TAG = "Button_Handler";

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

    // Button event queue
    static QueueHandle_t s_event_queue = nullptr;

    // Button debounce timer handles
    static TimerHandle_t s_rst_btn_deb_timer_handle = nullptr;
    static TimerHandle_t s_enc_btn_deb_timer_handle = nullptr;

    // Timer handles for staged led dimming
    static esp_timer_handle_t s_led_to_50_percent_timer = nullptr;
    static esp_timer_handle_t s_led_to_25_percent_timer = nullptr;
    static esp_timer_handle_t s_led_to_0_percent_timer = nullptr;

    // Timer to turn off buzzer for the `beep()` function
    static esp_timer_handle_t s_buzzer_timer = nullptr;

    // To track the led's brightness
    static bool s_screen_at_100_percent = false;

    // To sample button press hold times
    static int64_t s_rst_us = 0;
    static int64_t s_enc_us = 0;

    // Copy of button struct
    static gpio_pins_t s_pins{};

    // Conversions
    static constexpr uint32_t TIME_TO_LED_50_PERCENT_US = TIME_TO_LED_50_PERCENT_S * 1'000'000;
    static constexpr uint32_t TIME_TO_LED_25_PERCENT_US = TIME_TO_LED_25_PERCENT_S * 1'000'000;
    static constexpr uint32_t TIME_TO_LED_0_PERCENT_US = TIME_TO_LED_0_PERCENT_S * 1'000'000;
    static constexpr uint32_t BUTTON_LONG_PRESS_US = BUTTON_LONG_PRESS_S * 1'000'000;

    // Constants
    static constexpr uint8_t QUEUE_LENGTH = 10;
    static constexpr uint8_t TIMEOUT_MS = 100;

    // Get the LED full brightness values from the resolution
    constexpr uint32_t LED_FULL_BRIGHTNESS_VALUE = (1UL << static_cast<uint32_t>(LEDC_TIMER_10_BIT));
    
    // The actual max value is `(2 ^ resolution) - 1`
    constexpr uint32_t LED_100_PERCENT_VALUE = LED_FULL_BRIGHTNESS_VALUE - 1;
    constexpr uint32_t LED_50_PERCENT_VALUE = (LED_FULL_BRIGHTNESS_VALUE / 2);
    constexpr uint32_t LED_25_PERCENT_VALUE = (LED_FULL_BRIGHTNESS_VALUE / 4);


    // Forward declaration
    static inline esp_err_t delete_freertos_timer(TimerHandle_t& timer);
    static inline void delete_esp_timer(esp_timer_handle_t& timer);
    static inline void update_display_led_and_timers();
    static inline esp_err_t cleanup();
    static void rst_btn_deb_timer_cb(TimerHandle_t xTimer);
    static void enc_btn_deb_timer_cb(TimerHandle_t xTimer);


    // Public API
    [[nodiscard]] esp_err_t init(const gpio_pins_t& gpio_pins, esp_timer_handle_t& led_timer_handle) {

        log<log_level_t::INFO>("Initializing button handler");

        s_pins = gpio_pins;

        // Initialize buzzer pin
        const gpio_config_t buzzer_config = {
            .pin_bit_mask = (1ULL << s_pins.buzzer_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };

        esp_err_t ret = gpio_config(&buzzer_config);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to configure gpio buzzer pins: %s", esp_err_to_name(ret));
            return ret;
        }

        constexpr esp_timer_create_args_t s_buzzer_timer_args = {
            .callback = [](void* arg) {
                // Turn buzzer off
                gpio_set_level(s_pins.buzzer_pin, 0);
            },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "buzzer_timer",
            .skip_unhandled_events = false
        };

        ret = esp_timer_create(&s_buzzer_timer_args, &s_buzzer_timer);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to create buzzer_timer");
            cleanup();
            return ret;
        }

        // Turn the buzzer off initially
        gpio_set_level(s_pins.buzzer_pin, 0);

        // Configure button pins
        const gpio_config_t button_config = {
            .pin_bit_mask = (1ULL << s_pins.reset_btn_pin) | (1ULL << s_pins.enc_btn_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_ANYEDGE
        };

        log<log_level_t::INFO>("Reset: %d, Enc: %d", s_pins.reset_btn_pin, s_pins.enc_btn_pin);

        ret = gpio_config(&button_config);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to configure gpio button pins: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_EDGE);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to install isr service: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        ret = gpio_isr_handler_add(s_pins.reset_btn_pin,
            [](void* arg) {
                BaseType_t higher_priority_task_woken = pdFALSE;
                xTimerStartFromISR(s_rst_btn_deb_timer_handle, &higher_priority_task_woken);
                gpio_intr_disable(s_pins.reset_btn_pin);
                if (higher_priority_task_woken) portYIELD_FROM_ISR();
            },
            nullptr);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to add isr for reset button gpio: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        ret = gpio_isr_handler_add(s_pins.enc_btn_pin, 
            [](void* arg) {
                BaseType_t higher_priority_task_woken = pdFALSE;
                xTimerStartFromISR(s_enc_btn_deb_timer_handle, &higher_priority_task_woken);
                gpio_intr_disable(s_pins.enc_btn_pin);
                if (higher_priority_task_woken) portYIELD_FROM_ISR();
            },
            nullptr);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to add isr for encoder gpio pin: %s", esp_err_to_name(ret));
            cleanup();
            return ret;
        }

        // Initialize ledc timer for led pin
        constexpr ledc_timer_config_t display_led_timer_config = {
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .timer_num = LEDC_TIMER_1,
            .freq_hz = 20 * 1000,
            .clk_cfg = LEDC_AUTO_CLK,
            .deconfigure = false
        };

        ret = ledc_timer_config(&display_led_timer_config);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to initialize ledc timer");
            cleanup();
            return ret;
        }

        // Initiaize ledc channel for led pin
        const ledc_channel_config_t display_led_channel_config = {
            .gpio_num = static_cast<int>(s_pins.led_pin),
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel = LEDC_CHANNEL_1,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_1,
            .duty = LED_100_PERCENT_VALUE,
            .hpoint = 0,
            .flags = { .output_invert = 0 }
        };

        ret = ledc_channel_config(&display_led_channel_config);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to initialize ledc channel");
            cleanup();
            return ret;
        }

        // Create timers for staged led dimming
        constexpr esp_timer_create_args_t led_to_50_percent_timer = {
            .callback = [](void* arg) {
                ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LED_50_PERCENT_VALUE);
                ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                esp_timer_start_once(s_led_to_25_percent_timer, TIME_TO_LED_25_PERCENT_US);
                s_screen_at_100_percent = false;
            },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "led_to_50_percent_timer",
            .skip_unhandled_events = false
        };

        ret = esp_timer_create(&led_to_50_percent_timer, &s_led_to_50_percent_timer);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to create led_to_50_percent_timer");
            cleanup();
            return ret;
        }

        constexpr esp_timer_create_args_t led_to_25_percent_timer = {
            .callback = [](void* arg) {
                ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LED_25_PERCENT_VALUE);
                ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                esp_timer_start_once(s_led_to_0_percent_timer, TIME_TO_LED_0_PERCENT_US);
                s_screen_at_100_percent = false;
            },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "led_to_25_percent_timer",
            .skip_unhandled_events = false
        };

        ret = esp_timer_create(&led_to_25_percent_timer, &s_led_to_25_percent_timer);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to create led_to_25_percent_timer");
            cleanup();
            return ret;
        }

        constexpr esp_timer_create_args_t led_to_0_percent_timer = {
            .callback = [](void* arg) {
                ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
                ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
                s_screen_at_100_percent = false;
            },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "led_to_0_percent_timer",
            .skip_unhandled_events = false
        };

        ret = esp_timer_create(&led_to_0_percent_timer, &s_led_to_0_percent_timer);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to create led_to_0_percent_timer");
            cleanup();
            return ret;
        }

        s_event_queue = xQueueCreate(QUEUE_LENGTH, sizeof(event_t));
        if (!s_event_queue) {
            log<log_level_t::ERROR>("Failed to create s_event_queue");
            cleanup();
            return ESP_FAIL;
        }

        s_rst_btn_deb_timer_handle = xTimerCreate("RstButtonDebounceTimer", pdMS_TO_TICKS(TIMEOUT_MS),
                                                   pdFALSE, nullptr, rst_btn_deb_timer_cb);
        if(!s_rst_btn_deb_timer_handle) {
            log<log_level_t::ERROR>("Failed to create RstButtonDebounceTimer");
            cleanup();
            return ESP_FAIL;
        }

        s_enc_btn_deb_timer_handle = xTimerCreate("EncButtonDebounceTimer", pdMS_TO_TICKS(TIMEOUT_MS),
                                                   pdFALSE, nullptr, enc_btn_deb_timer_cb);
        if(!s_enc_btn_deb_timer_handle) {
            log<log_level_t::ERROR>("Failed to create EncButtonDebounceTimer");
            cleanup();
            return ESP_FAIL;
        }

        // Set the led to full brightness initially
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LED_100_PERCENT_VALUE);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);

        s_screen_at_100_percent = true;
        led_timer_handle = s_led_to_50_percent_timer;

        log<log_level_t::INFO>("Initialization complete");

        return ESP_OK;
    }

    [[nodiscard]] esp_err_t deinit() {

        log<log_level_t::INFO>("Deinitializing button interface");

        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to deinitialize button interface: %s", esp_err_to_name(ret));
            return ret;
        }
        
        log<log_level_t::INFO>("Deinitializing button interface complete");
        
        return ESP_OK;
    }

    [[nodiscard]] QueueHandle_t get_event_queue() {
        return s_event_queue;
    }

    void set_led_to_max() {
        update_display_led_and_timers();
    }

    void beep(uint32_t ms) {
        // Stop any previously activated beeps
        esp_timer_stop(s_buzzer_timer);
        gpio_set_level(s_pins.buzzer_pin, 1);
        esp_timer_start_once(s_buzzer_timer, (ms * 1'000));
    }

    // Static helpers
    static inline esp_err_t delete_freertos_timer(TimerHandle_t& timer) {
        if (timer) {
            BaseType_t ret = xTimerStop(timer, 0);
            if (ret != pdPASS) return ESP_FAIL;
            ret = xTimerDelete(timer, 0);
            if (ret != pdPASS) return ESP_FAIL;
            timer = nullptr;
        }
        return ESP_OK;
    }

    static inline void delete_esp_timer(esp_timer_handle_t& timer) {
        if (timer) {
            esp_timer_stop(timer);
            esp_timer_delete(timer);
            timer = nullptr;
        }
    }

    static inline void update_display_led_and_timers() {
        // Set the screen's brightness back to the max value
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, LED_100_PERCENT_VALUE);
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
        s_screen_at_100_percent = true;
        // Stop all timers immediately if active
        esp_timer_stop(s_led_to_50_percent_timer);
        esp_timer_stop(s_led_to_25_percent_timer);
        esp_timer_stop(s_led_to_0_percent_timer);
        // Start the s_led_to_50_percent_timer
        esp_timer_start_once(s_led_to_50_percent_timer, TIME_TO_LED_50_PERCENT_US);
    }
    
    static inline esp_err_t cleanup() {

        gpio_isr_handler_remove(s_pins.reset_btn_pin);
        gpio_isr_handler_remove(s_pins.enc_btn_pin);

        gpio_uninstall_isr_service();

        gpio_reset_pin(s_pins.reset_btn_pin);
        gpio_reset_pin(s_pins.enc_btn_pin);
        gpio_reset_pin(s_pins.buzzer_pin);
        gpio_reset_pin(s_pins.led_pin);
        
        esp_err_t ret = ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
        if (ret != ESP_OK) return ret;

        ledc_timer_config_t led_timer_deconfig = {};

        led_timer_deconfig.speed_mode = LEDC_HIGH_SPEED_MODE;
        led_timer_deconfig.timer_num = LEDC_TIMER_1;
        led_timer_deconfig.deconfigure = true;

        ret = ledc_timer_config(&led_timer_deconfig);
        if (ret != ESP_OK) return ret;

        s_screen_at_100_percent = false;
        s_rst_us = 0;
        s_enc_us = 0;
        s_pins = {};

        delete_esp_timer(s_led_to_50_percent_timer);
        delete_esp_timer(s_led_to_25_percent_timer);
        delete_esp_timer(s_led_to_0_percent_timer);
        delete_esp_timer(s_buzzer_timer);

        if (s_event_queue) {
            vQueueDelete(s_event_queue);
            s_event_queue = nullptr;
        }

        ret = delete_freertos_timer(s_rst_btn_deb_timer_handle);
        if (ret != ESP_OK) return ret;
        ret = delete_freertos_timer(s_enc_btn_deb_timer_handle);
        return ret;
    }

    static void rst_btn_deb_timer_cb(TimerHandle_t xTimer) {
        // Re-enable interrupts
        gpio_intr_enable(s_pins.reset_btn_pin);

        event_t event = event_t::NO_EVENT;

        if (gpio_get_level(s_pins.reset_btn_pin) == 0) {
            s_rst_us = esp_timer_get_time();
            return;

        } else if (gpio_get_level(s_pins.reset_btn_pin) == 1) {
            if ((esp_timer_get_time() - s_rst_us) >= BUTTON_LONG_PRESS_US) {
                event = event_t::RESET_LONG_PRESSED;
            } else {
                event = event_t::RESET_PRESSED;
            }
            s_rst_us = 0;
        }

        // Only send button updates if screen is at 100% brightness, else, just set screen to full brightness
        if (s_screen_at_100_percent) xQueueSend(s_event_queue, &event, 0);

        update_display_led_and_timers();
    }

    static void enc_btn_deb_timer_cb(TimerHandle_t xTimer) {
        // Re-enable interrupts
        gpio_intr_enable(s_pins.enc_btn_pin);

        event_t event = event_t::NO_EVENT;

        if (gpio_get_level(s_pins.enc_btn_pin) == 0) {
            s_enc_us = esp_timer_get_time();
            return;

        } else if (gpio_get_level(s_pins.enc_btn_pin) == 1) {
            if ((esp_timer_get_time() - s_enc_us) >= BUTTON_LONG_PRESS_US) {
                event = event_t::ENC_LONG_PRESSED;
            } else {
                event = event_t::ENC_PRESSED;
            }
            s_enc_us = 0;
        }

        // Only send button updates if screen is at 100% brightness, else, just set screen to full brightness
        if (s_screen_at_100_percent) xQueueSend(s_event_queue, &event, 0);

        update_display_led_and_timers();
    }
    
} // namespace btn
