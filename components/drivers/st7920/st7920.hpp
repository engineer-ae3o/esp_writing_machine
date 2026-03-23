#ifndef ST7920_HPP_
#define ST7920_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_err.h"

#include <expected>
#include <utility>
#include <memory>


namespace st7920 {

    struct config_t {
        // SPI configuration
        spi_host_device_t spi_host{};
        uint32_t spi_clock_speed_hz{};

        // GPIO pins
        gpio_num_t pin_mosi{GPIO_NUM_NC};
        gpio_num_t pin_sclk{GPIO_NUM_NC};
        gpio_num_t pin_cs{GPIO_NUM_NC};

        // Display parameters
        uint16_t width{};                 // Width of display in pixels
        uint16_t height{};                // Height of display in pixels

        // Error handling
        uint8_t max_retries{};            // Number of retry attempts on SPI failure

        // Task configuration
        uint8_t queue_size{};             // Flush request queue size
        uint8_t task_priority{};          // FreeRTOS task priority
        uint8_t task_core{};              // CPU core to pin task to
        uint16_t task_stack_size{};       // Task stack size in bytes
    };

    // Type of callback invoked when a transfer has been completed
    using flush_done_cb_t = void(*)(void* user_data, esp_err_t result);

    class st7920_t {
    private:
        enum class state_t : uint8_t {
            IDLE = 0,
            BUSY
        };
        
        config_t m_config{};
        spi_device_handle_t m_spi_handle{};
        volatile state_t m_state{};
        
        SemaphoreHandle_t m_spi_done_sem{};     // Signaled by ISR when SPI completes
        QueueHandle_t m_flush_queue{};          // Queue of pending flush requests
        TaskHandle_t m_task_handle{};           // Background processing task
        SemaphoreHandle_t m_handle_mutex{};     // Mutex for thread safety

        bool m_is_initialized{};
        bool m_shutdown_requested{};
        TaskHandle_t m_deinit_task_handle{};

        uint8_t* m_pixels_buf{};                // Pointer to DMA buffer for current instance
        size_t m_size_of_pixel_buf_bytes{};
        SemaphoreHandle_t m_dma_semphr{};       // Semaphore to ensure safe access of dma buffer
        
        // This is due to the ST7920's weird pixel reception format. For more details,
        // refer to the datasheet. The buffer is owned by the class, but is used only
        // by `pixel_trans_task`, so it doesn't need protection from concurrent access
        uint8_t* m_wire_buf{};                  // Actual buffer used by DMA
        size_t m_wire_buf_len{};                // This is not the length of m_wire_buf, but how many
                                                // in it are relevant

        struct flush_req_t {
            uint16_t x1{}, y1{}, x2{}, y2{};
            size_t pixel_count{};
            flush_done_cb_t callback{};
            void* user_data{};
        };

        // The constructor is made private to prevent situations
        // where the object has been created but has not yet been
        // initialized. To create an object, call the `st7920_t::create()`
        // function. It constructs an object, initializes it and
        // moves it into the caller.
        st7920_t(const config_t& config);
        
    public:
        // Remove the default constructor
        st7920_t() = delete;
        
        // Deinits the encoder class when it goes out of scope
        ~st7920_t();

        // Non copyable
        st7920_t(const st7920_t&) = delete;
        st7920_t& operator=(const st7920_t&) = delete;

        // Only moveable
        st7920_t(st7920_t&& other);
        st7920_t& operator=(st7920_t&& other);

        /**
         * @brief Constructs an `st7920_t` object and moves it
         *        into the calling function. This is done to
         *        ensure that creation of an `st7920_t` object
         *        and initialization occurs with a single function
         *        call.
         * 
         * @param config The configuration struct containing the
         *               data to initialize the driver with
         * 
         * @return An `std::expected` containing the created object and an
         *        error code on success. It returns `nullptr` and the
         *        associated error code on failure
         */
        static std::expected<std::unique_ptr<st7920_t>, esp_err_t> create(const config_t& config);

        /**
         * @brief Initialize the ST7920 driver
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t init();

        /**
         * @brief Deinitialize the ST7920 driver and free resources
         *
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t deinit();

        /**
         * @brief Async flush pixel data to display.
         * Non-blocking: returns immediately, callback invoked when transfer completes
         *
         * @param x1, y1 Top-left corner of update region
         * @param x2, y2 Bottom-right corner of update region
         * @param pixel_data Monochrome pixel buffer
         * @param pixel_count Number of pixels
         * @param callback Function to call when flush completes (receives result code of operation)
         * @param user_data Passed to callback
         *
         * @return ESP_OK if data queued successfully, error code otherwise
         */
        [[nodiscard]] esp_err_t flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                      const uint8_t* pixel_data, size_t pixel_count,
                                      flush_done_cb_t callback, void* user_data);
        
        /**
         * @brief Sets full screen to specified either black or white as specified
         * 
         * @param color Color to set screen to
         *                  - `true` for to set screen
         *                  - `false` to clear screen
         * @param callback Function to call when flush completes (receives result code)
         * @param user_data Passed to callback
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t set_screen(bool color, flush_done_cb_t callback, void* user_data);

    private:
        // Cleanup resources
        esp_err_t cleanup();

        // Data and commands
        esp_err_t send_cmd(uint8_t cmd);
        
        // Display driver control and pixel updates
        esp_err_t init_sequence();
        esp_err_t send_pixels_dma(const uint8_t* pixels, size_t count);
        esp_err_t build_wire_buffer(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
        
        // These are passed to C functions, so they need to be declared static
        static void pixel_trans_task(void* arg);
        static void IRAM_ATTR spi_post_transfer_callback(spi_transaction_t* trans);

    };

} // namespace st7920


#endif // ST7920_HPP_