#ifndef _ENCODER_HPP_
#define _ENCODER_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_err.h"

#include <optional>
#include <utility>
#include <memory>


namespace enc {

    // Defines how many encoder steps have to occcur in a certain
    // direction for an event to be passed into the queue
    constexpr inline uint8_t LIMIT = 2;
    // Defines the minimum time between valid pulses
    constexpr inline uint8_t RATE_LIMIT_MS = 50;
    constexpr inline uint8_t EVENT_QUEUE_SIZE = 10;
    
    enum class event_t : uint8_t {
        NO_EVENT = 0,
        CLOCKWISE,
        COUNTERCLOCKWISE
    };
    
    class encoder_t {
    private:
        bool m_is_initialized{false};

        gpio_num_t m_pin_a{GPIO_NUM_NC};
        gpio_num_t m_pin_b{GPIO_NUM_NC};

        int32_t m_counter{};

        pcnt_unit_handle_t m_unit_handle{nullptr};
        pcnt_channel_handle_t m_chan_handle{nullptr};
        
        QueueHandle_t m_event_queue{nullptr};

        // Used for debouncing
        int64_t m_last_time_us{};

        // The constructor is made private to prevent situations
        // where the object has been created but has not yet been
        // initialized. To create an object, call the `encoder_t::create()`
        // function. It constructs an object, initializes it and
        // moves it into the caller.
        encoder_t(gpio_num_t pin_a, gpio_num_t pin_b);

        /**
         * @brief Called when an the PCNT reaches any of the
         *        pre-configured watchpoints
         * 
         * @return True if a higher priority task was woken,
         *         false otherwise
         */
        static bool event_cb(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t* edata, void* user_ctx);

        esp_err_t cleanup();

    public:
        // Remove the default constructor
        encoder_t() = delete;
        
        // Deinits the encoder class when it goes out of scope
        ~encoder_t();

        // Non copyable
        encoder_t(const encoder_t&) = delete;
        encoder_t& operator=(const encoder_t&) = delete;

        // Only moveable
        encoder_t(encoder_t&& other);
        encoder_t& operator=(encoder_t&& other);

        /**
         * @brief Constructs an `encoder_t` object and moves it
         *        into the calling function. This is done to
         *        ensure that creation of an `encoder_t` object
         *        and initialization occurs with a single function
         *        call.
         * 
         * @param pin_a The A pin of the encoder
         * @param pin_b The B pin of the encoder
         * 
         * @return An `std::pair` containing the created object and an
         *        error code on success. It returns `nullptr` and the
         *        associated error code on failure
         */
        static std::pair<std::unique_ptr<encoder_t>, esp_err_t> create(gpio_num_t pin_a, gpio_num_t pin_b);

        /**
         * @brief Initializes the encoder with the pins specified
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t init();

        /**
         * @brief Deinitializes the rotary encoder interface
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t deinit();

        /**
         * @brief Start the PCNT's counting
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t start();

        /**
         * @brief Stop the PCNT's counting
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t stop();

        /**
         * @brief Getter for encoder_t event queue
         * 
         * @return Handle to the queue in which encoder_t 
         *         events are passed into
         */
        [[nodiscard]] QueueHandle_t get_event_queue();
        
    };

} // namespace enc


#endif // _ENCODER_HPP_