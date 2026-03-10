#ifndef _BUTTONS_HPP_
#define _BUTTONS_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_err.h"


namespace btn {

    // Time for staged led dimming. Configurable
    constexpr inline uint8_t TIME_TO_LED_50_PERCENT_S{30};
    constexpr inline uint8_t TIME_TO_LED_25_PERCENT_S{20};
    constexpr inline uint8_t TIME_TO_LED_0_PERCENT_S{10};

    // Button long press hold time
    constexpr inline float BUTTON_LONG_PRESS_S{2};

    struct gpio_pins_t {
        gpio_num_t reset_btn_pin{GPIO_NUM_NC};
        gpio_num_t enc_btn_pin{GPIO_NUM_NC};
        gpio_num_t led_pin{GPIO_NUM_NC};
        gpio_num_t buzzer_pin{GPIO_NUM_NC};
    };

    /**
     * @brief Initializes the button interface and isr handlers as
     *        well as ledc pwm for the led pin and the buzzer pin
     * 
     * @param[in] gpio_pins Structure containing gpio pins for the buttons and led
     * @param[out] led_timer_handle Handle to timer which starts led dimming sequence
     * 
     * @return ESP_OK on success, error code otherwise
     */
    [[nodiscard]] esp_err_t init(const gpio_pins_t& gpio_pins, esp_timer_handle_t& led_timer_handle);

    /**
     * @brief Deinitializes the button interface and isr handlers
     * 
     * @return ESP_OK on success, error code otherwise
     */
    [[nodiscard]] esp_err_t deinit();

    enum class event_t : uint8_t {
        NO_EVENT= 0,
        RESET_PRESSED,
        RESET_LONG_PRESSED,
        ENC_PRESSED,
        ENC_LONG_PRESSED
    };

    /**
     * @brief Getter for button event queue
     * 
     * @return Handle to queue in which all button events are passed into
     */
    [[nodiscard]] QueueHandle_t get_event_queue();

    /**
     * @brief Beeps a buzzer for the specified number of miliseconds.
     *        This function is asynchronous. It returns immediately and
     *        defers the turning off of the buzzer to an `esp_timer`
     * 
     * @param ms Time for the beep in miliseconds
     */
    void beep(uint32_t ms);

    /**
     * @brief Sets the led to max brightness
     *        and resets the timers
     */
    void set_led_to_max();

} // namespace btn


#endif // _BUTTONS_HPP_