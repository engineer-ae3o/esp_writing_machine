#ifndef _SERVO_HPP_
#define _SERVO_HPP_


#include "driver/gpio.h"
#include "driver/ledc.h"

#include <optional>
#include <utility>


namespace servo {

    struct config_t {
        ledc_channel_t channel{};
        ledc_timer_bit_t resolution{};
        ledc_timer_t timer{};
        ledc_mode_t mode{};
        gpio_num_t pin{};
        float frequency{};
    };

    class servo_t {
    private:
        bool m_is_initialized{};

        config_t m_config{};
        float m_max_duty_cycle{};
        float m_max_pulse_width_us{};

        // The constructor is made private to prevent situations
        // where the object has been created but has not yet been
        // initialized. To create an object, call the `servo_t::create()`
        // function. It constructs an object, initializes it and
        // moves it into the caller.
        servo_t(const config_t& config);

        esp_err_t cleanup();

    public:
        // Remove the default constructor
        servo_t() = delete;

        // Deinits the servo_t class when it goes out of scope
        ~servo_t();

        // Non copyable
        servo_t(const servo_t&) = delete;
        servo_t& operator=(const servo_t&) = delete;

        // Only moveable
        servo_t(servo_t&& other);
        servo_t& operator=(servo_t&& other);

        /**
         * @brief Constructs a `servo_t` object and moves it
         *        into the calling function. This is done to
         *        ensure that creation of an `servo_t` object
         *        and initialization occurs in a single function
         *        call.
         * 
         * @param config Config struct holding details regarding the
         *               servo instance to be created
         * 
         * @return An `std::pair` containing the created object and an
         *        error code on success. An `std::nullopt` object and the
         *        associated error code
         */
        static std::pair<std::optional<servo_t>, esp_err_t> create(const config_t& config);

        /**
         * @brief Initializes a servo motor on given gpio pin
         *        with the given configuration settings
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t init();

        /**
         * @brief Deinitializes the servo gpio pin
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t deinit();

        /**
         * @brief Sets the servo motor to the specified angle
         * 
         * @param angle Angle to move the servo motor to
         *              from the current position
         * 
         * @return true on success, false on invalid arg
         */
        bool set_angle(uint32_t angle);
    };

} // namespace servo


#endif // _SERVO_HPP_