#ifndef STEPPER_HPP_
#define STEPPER_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "esp_err.h"
#include "esp_log.h"

#include <expected>
#include <utility>
#include <memory>
#include <cmath>


namespace a4988 {

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    constexpr inline log_level_t LOG_LEVEL = log_level_t::INFO;
    constexpr inline const char* TAG       = "Stepper";

    template <log_level_t level, typename... Args>
    void log(const char* fmt, Args&&... args) {
        if constexpr (level <= LOG_LEVEL) {
            // This is a work around. The ESP_LOGx macros
            // expect a string literal
            char msg[128]{};
            snprintf(msg, sizeof(msg), fmt, args...);
            if constexpr (level == log_level_t::ERROR)     ESP_LOGE(TAG, "%s", msg);
            else if constexpr (level == log_level_t::WARN) ESP_LOGW(TAG, "%s", msg);
            else if constexpr (level == log_level_t::INFO) ESP_LOGI(TAG, "%s", msg);
        }
    }

    // This defines the how many percent of steps is used for
    // acceleration. Note this applies to decceleration as well
    constexpr inline float ACCEL_PERCENT{15.0f};

    // Defines the number of distinct acceleration levels.
    // Thst is, how many times we increment the speed before
    // reaching cruise speed
    constexpr inline size_t ACCEL_LEVELS{10};
    
    struct config_t {
        gpio_num_t step_pin{GPIO_NUM_NC};
        gpio_num_t dir_pin{GPIO_NUM_NC};
        gpio_num_t ena_pin{GPIO_NUM_NC};

        uint32_t rmt_frequency{};
        size_t dma_buf_size{};

        // NOTE: The acceleration profile is uses trapezoidal profiles,
        // and would require a queue size of at least 21 elements. If
        // the queue size is not enough, the driver will block waiting
        // for free slots
        size_t queue_depth{};

        // Microstepping pins
        // NOTE: These are ignored if microstep
        // is set to FULL mode
        gpio_num_t microstep_1{GPIO_NUM_NC};
        gpio_num_t microstep_2{GPIO_NUM_NC};
        gpio_num_t microstep_3{GPIO_NUM_NC};

        bool is_active_low{false};
    };

    enum class dir_t : uint8_t {
        ANTICLOCKWISE = 0,
        CLOCKWISE     = 1
    };

    // The bit positions here determine the logic levels
    // of the microstepping pins in the order of ms3 to ms1
    enum class microstep_t : uint8_t {
        FULL          = 0b000,
        HALF          = 0b001,
        ONE_FOURTH    = 0b010,
        ONE_EIGHTH    = 0b011,
        ONE_SIXTEENTH = 0b111
    };

    template <microstep_t microstep>
    class driver_t {
    private:
        rmt_channel_handle_t m_chan_handle{};
        rmt_encoder_handle_t m_copy_enc_handle{};

        bool m_is_initialized{false};
        config_t m_config{};
        TaskHandle_t m_calling_task_handle{};
        
        // The constructor is made private to prevent situations
        // where the object has been created but has not yet been
        // initialized. To create an object, call the `create()`
        // function and pass in microstep option. It constructs an
        // object, initializes it and moves it into the caller.
        driver_t(const config_t& config) : m_config(config) {}

        static bool IRAM_ATTR trans_done_cb(rmt_channel_handle_t tx_chan, const rmt_tx_done_event_data_t* edata, void* user_ctx) {
            driver_t* driver = static_cast<driver_t*>(user_ctx);
            BaseType_t higher_priority_task_woken{false};
            if (driver->m_calling_task_handle) vTaskNotifyGiveFromISR(driver->m_calling_task_handle, &higher_priority_task_woken);
            return higher_priority_task_woken == pdTRUE;
        }

        esp_err_t cleanup() {

            esp_err_t ret{ESP_OK};

            // Disable motor
            if (m_chan_handle) {
                ret = disable();
                if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                    log<log_level_t::ERROR>("Failed to disable rmt channel: %s", esp_err_to_name(ret));
                    return ret;
                }
            }

            // Reset all gpio pins
            gpio_reset_pin(m_config.dir_pin);
            gpio_reset_pin(m_config.ena_pin);

            if constexpr (microstep != microstep_t::FULL) {
                gpio_reset_pin(m_config.microstep_1);
                gpio_reset_pin(m_config.microstep_2);
                gpio_reset_pin(m_config.microstep_3);
            }
            
            if (m_copy_enc_handle) {
                ret = rmt_del_encoder(m_copy_enc_handle);
                if (ret != ESP_OK) {
                    log<log_level_t::ERROR>("Failed to delete rmt encoder: %s", esp_err_to_name(ret));
                    return ret;
                }
                m_copy_enc_handle = nullptr;
            }
            
            if (m_chan_handle) {
                ret = rmt_del_channel(m_chan_handle);
                if (ret != ESP_OK) {
                    log<log_level_t::ERROR>("Failed to delete rmt channel: %s", esp_err_to_name(ret));
                    return ret;
                }
                m_chan_handle = nullptr;
            }
            
            m_config = {};
            m_is_initialized = false;

            return ESP_OK;
        }

        esp_err_t queue_accel_and_cruise_steps(uint32_t steps, uint32_t speed, uint32_t accel) {

            // Find the number of steps that would be used as acceleration
            const uint32_t accel_steps = (ACCEL_PERCENT * steps) / 100;
            // Acceleration gets the remaining steps after removing the acceleration and decceleration steps
            const uint32_t cruise_steps = steps - (2 * accel_steps);
            // Starting speed. We use `1 / ACCEL_LEVELS` of the given speed
            const float v_start = static_cast<float>(speed) / static_cast<float>(ACCEL_LEVELS);

            // Buffers to store the acceleration and decceleration
            // steps, as well as the cruise speed
            rmt_symbol_word_t accel_steps_words[ACCEL_LEVELS]{};
            rmt_symbol_word_t cruise_step_words{};

            esp_err_t ret{ESP_OK};
            
            // Send acceleration steps
            const uint32_t steps_per_accel_level = accel_steps / ACCEL_LEVELS;

            for (uint32_t i = 0; i < ACCEL_LEVELS; i++) {
                // Using `(vf ^ 2) = (vi ^ 2) + (2 * a * s)`
                const float v_next = sqrtf((v_start * v_start) + (2.0f * accel * (i * steps_per_accel_level)));
                const uint32_t half_period = static_cast<uint32_t>(m_config.rmt_frequency / (2.0f * v_next));

                accel_steps_words[i].duration0 = half_period;
                accel_steps_words[i].level0 = 1;
                accel_steps_words[i].duration1 = half_period;
                accel_steps_words[i].level1 = 0;

                const rmt_transmit_config_t accel_config = {
                    .loop_count = static_cast<int>(steps_per_accel_level),
                    .flags = {
                        .eot_level = 1,
                        .queue_nonblocking = 0
                    }
                };

                ret = rmt_transmit(m_chan_handle, m_copy_enc_handle, &accel_steps_words[i], sizeof(accel_steps_words[i]), &accel_config);
                if (ret != ESP_OK) return ret;
            }

            // Send cruise speed steps
            const uint32_t num_of_ticks_per_steps = m_config.rmt_frequency / speed;
            const uint32_t half_period = num_of_ticks_per_steps / 2;

            // Prepare steps
            cruise_step_words.duration0 = half_period;
            cruise_step_words.level0 = 1;
            cruise_step_words.duration1 = half_period;
            cruise_step_words.level1 = 0;

            const rmt_transmit_config_t cruise_speed_config = {
                .loop_count = static_cast<int>(cruise_steps),
                .flags = {
                    .eot_level = 1,
                    .queue_nonblocking = 0
                }
            };

            ret = rmt_transmit(m_chan_handle, m_copy_enc_handle, &cruise_step_words, sizeof(cruise_step_words), &cruise_speed_config);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to queue rmt tx cruise steps payload: %s", esp_err_to_name(ret));
                return ret;
            }
            
            // Send decceleration steps
            for (int i = (ACCEL_LEVELS - 1); i >= 0; i--) {
                // The acceleration steps have already been calculated,
                // so we just iterate in the reverse order and transmit
                // the steps
                const rmt_transmit_config_t accel_config = {
                    .loop_count = static_cast<int>(steps_per_accel_level),
                    .flags = {
                        .eot_level = 1,
                        .queue_nonblocking = 0
                    }
                };

                ret = rmt_transmit(m_chan_handle, m_copy_enc_handle, &accel_steps_words[i], sizeof(accel_steps_words[i]), &accel_config);
                if (ret != ESP_OK) return ret;
            }

            return ESP_OK;
        }

    public:
        // Remove default constructor
        driver_t() = delete;

        // Deinits the driver when it gets destructed
        ~driver_t() {
            esp_err_t ret = cleanup();
            if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                log<log_level_t::ERROR>("Failed to properly destruct driver instance: %s", esp_err_to_name(ret));
            }
        }

        // Non copyable
        driver_t(const driver_t&) = delete;
        driver_t& operator=(const driver_t&) = delete;

        // Only moveable
        driver_t(driver_t&& other) {
            
            m_is_initialized = other.m_is_initialized;
            m_chan_handle = other.m_chan_handle;
            m_copy_enc_handle = other.m_copy_enc_handle;
            m_config = other.m_config;
            m_calling_task_handle = other.m_calling_task_handle;
            
            other.m_is_initialized = false;
            other.m_chan_handle = nullptr;
            other.m_copy_enc_handle = nullptr;
            other.m_config = {};
            other.m_calling_task_handle = nullptr;
        }

        driver_t& operator=(driver_t&& other) {
            if (this != &other) {

                cleanup();
                
                m_chan_handle = other.m_chan_handle;
                m_copy_enc_handle = other.m_copy_enc_handle;
                m_is_initialized = other.m_is_initialized;
                m_config = other.m_config;
                m_calling_task_handle = other.m_calling_task_handle;
                
                other.m_is_initialized = false;
                other.m_chan_handle = nullptr;
                other.m_copy_enc_handle = nullptr;
                other.m_config = {};
                other.m_calling_task_handle = nullptr;
            }

            return *this;
        }

        /**
         * @brief Constructs a `driver_t` object on the heap and moves
         *        it into the calling function. This is done to
         *        ensure that creation of an `driver_t` object
         *        and initialization occurs with a single function
         *        call.
         * 
         * @param config The configuration struct containing the information
         *               to use to initialize the driver
         * 
         * @return An `std::expected` containing a `unique_ptr` to the created 
         *         object on success. It returns an `std::unexpected` and the
         *         associated error code on failure
         */
        static std::expected<std::unique_ptr<driver_t>, esp_err_t> create(const config_t& config) {
            // Object has to be heap allocated for it to be returned/moved as we
            // pass in a `this` pointer during initialization and the original
            // would have been destructed when we leave this function
            std::unique_ptr<driver_t> stepper(new driver_t(config));

            esp_err_t ret = stepper->init();
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Initialization failed: %s", esp_err_to_name(ret));
                stepper->cleanup();
                return std::unexpected(ret);
            }

            return std::move(stepper);
        }

        /**
         * @brief Initializes the stepper motor driver with the specified settings
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t init() {
            
            if (m_is_initialized) {
                log<log_level_t::WARN>("Stepper motor instance already initialized");
                return ESP_ERR_INVALID_STATE;
            }

            log<log_level_t::INFO>("Starting initialization of stepper driver");

            // Configure RMT channel
            const rmt_tx_channel_config_t config = {
                .gpio_num = m_config.step_pin,
                // All RMT channels must use the same clock source
                .clk_src = RMT_CLK_SRC_DEFAULT,
                .resolution_hz = m_config.rmt_frequency,
                .mem_block_symbols = m_config.dma_buf_size,
                .trans_queue_depth = m_config.queue_depth,
                .intr_priority = 3,

                .flags = {
                    .invert_out = 0,
                    .with_dma = 1,
                    .io_loop_back = 0,
                    .io_od_mode = 0
                }
            };

            esp_err_t ret = rmt_new_tx_channel(&config, &m_chan_handle);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to create new rmt tx channel: %s", esp_err_to_name(ret));
                return ret;
            }
            
            // Setup RMT TX copy encoder
            constexpr rmt_copy_encoder_config_t enc_config{};

            ret = rmt_new_copy_encoder(&enc_config, &m_copy_enc_handle);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to create copy encoder: %s", esp_err_to_name(ret));
                return ret;
            }
            
            // Register event callback
            constexpr rmt_tx_event_callbacks_t event_cb = {
                .on_trans_done = trans_done_cb
            };

            ret = rmt_tx_register_event_callbacks(m_chan_handle, &event_cb, this);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to register event callback on rmt tx channel: %s", esp_err_to_name(ret));
                cleanup();
                return ret;
            }

            // Initialize direction and enable gpio pins
            const gpio_config_t dir_ena_config = {
                .pin_bit_mask = (1ULL << static_cast<uint64_t>(m_config.dir_pin)) | (1ULL << static_cast<uint64_t>(m_config.ena_pin)),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE
            };

            ret = gpio_config(&dir_ena_config);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to configure dir and enable pins: %s", esp_err_to_name(ret));
                cleanup();
                return ret;
            }

            // Disable stepper motor driver by default
            gpio_set_level(m_config.ena_pin, m_config.is_active_low);
            
            if constexpr (microstep != microstep_t::FULL) {
                // Microstepping pin configuration
                const gpio_config_t microstep_config = {
                    .pin_bit_mask = (1ULL << static_cast<uint64_t>(m_config.microstep_1)) |
                                    (1ULL << static_cast<uint64_t>(m_config.microstep_2)) |
                                    (1ULL << static_cast<uint64_t>(m_config.microstep_3)),
                    .mode = GPIO_MODE_OUTPUT,
                    .pull_up_en = GPIO_PULLUP_DISABLE,
                    .pull_down_en = GPIO_PULLDOWN_DISABLE,
                    .intr_type = GPIO_INTR_DISABLE
                };

                ret = gpio_config(&microstep_config);
                if (ret != ESP_OK) {
                    log<log_level_t::ERROR>("Failed to configure microstepping pins: %s", esp_err_to_name(ret));
                    cleanup();
                    return ret;
                }

                constexpr uint8_t ms1 = ((static_cast<uint8_t>(microstep) >> 0) & 0x01U);
                constexpr uint8_t ms2 = ((static_cast<uint8_t>(microstep) >> 1) & 0x01U);
                constexpr uint8_t ms3 = ((static_cast<uint8_t>(microstep) >> 2) & 0x01U);

                // Set microstepping to configured level
                gpio_set_level(m_config.microstep_1, ms1);
                gpio_set_level(m_config.microstep_2, ms2);
                gpio_set_level(m_config.microstep_3, ms3);
            }

            m_is_initialized = true;
            log<log_level_t::INFO>("Initialization complete");

            return ESP_OK;
        }

        /**
         * @brief Deinitializes the stepper motor driver
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t deinit() {

            if (!m_is_initialized) {
                log<log_level_t::WARN>("Stepper motor instance already in an uninitialized state");
                return ESP_ERR_INVALID_STATE;
            }

            log<log_level_t::INFO>("Deinitializing stepper driver");

            esp_err_t ret = cleanup();
            if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                log<log_level_t::ERROR>("Failed to deinitialize stepper motor instance: %s", esp_err_to_name(ret));
                return ret;
            }
            
            log<log_level_t::INFO>("Deinitialization complete");

            return ESP_OK;
        }

        /**
         * @brief Enables the stepper motor driver and the RMT channel
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t enable() {
            if (!m_is_initialized) return ESP_ERR_INVALID_STATE;
            gpio_set_level(m_config.ena_pin, !m_config.is_active_low);
            return rmt_enable(m_chan_handle);
        }

        /**
         * @brief Disables the stepper motor driver and the RMT channel
         * 
         * @return ESP_OK on success, error code otherwise
         */
        [[nodiscard]] esp_err_t disable() {
            if (!m_is_initialized) return ESP_ERR_INVALID_STATE;
            gpio_set_level(m_config.ena_pin, m_config.is_active_low);
            return rmt_disable(m_chan_handle);
        }

        /**
         * @brief Sends the steps to the stepper motor driver with the
         *        given number of steps and the specified direction. This
         *        function blocks until all steps have been sent or when an
         *        error occurs.
         * 
         * @param steps Number of steps to transmit
         * @param dir   Direction the stepper is to move
         * @param speed Speed of the stepper motor
         * 
         * @return ESP_OK if all steps were sent successfully, error code otherwise
         * 
         * @note The unit of speed is in `steps/sec`
         */
        [[nodiscard]] esp_err_t send_steps(uint32_t steps, dir_t dir, uint32_t speed) {

            if (!m_is_initialized) return ESP_ERR_INVALID_STATE;

            if ((steps == 0) || (speed == 0)) return ESP_ERR_INVALID_ARG;
            
            // Set direction
            gpio_set_level(m_config.dir_pin, static_cast<uint8_t>(dir));
            
            // Find the number of ticks per step
            const uint32_t num_of_ticks_per_steps = m_config.rmt_frequency / speed;
            const uint32_t half_period = num_of_ticks_per_steps / 2;

            // Prepare steps
            const rmt_symbol_word_t words = {
                .duration0 = half_period,
                .level0 = 1,
                .duration1 = half_period,
                .level1 = 0
            };
  
            // Transmission settings
            const rmt_transmit_config_t config = {
                .loop_count = static_cast<int>(steps),
                .flags = {
                    .eot_level = 1,
                    .queue_nonblocking = 0
                }
            };

            // Get the current task handle so the ISR knows
            // which task to send the task notification to
            m_calling_task_handle = xTaskGetCurrentTaskHandle();

            esp_err_t ret = rmt_transmit(m_chan_handle, m_copy_enc_handle, &words, sizeof(words), &config);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to queue rmt tx payload: %s", esp_err_to_name(ret));
                return ret;
            }

            // Wait for completion notification from ISR
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

            // Clear `m_calling_task_handle` to avoid stale state
            m_calling_task_handle = nullptr;

            return ESP_OK;
        }

        /**
         * @brief Sends the steps to the stepper motor driver with the
         *        given number of steps and the specified direction. This
         *        function configures the RMT peripheral to transmit the
         *        steps but returns immediately. The completion is handled
         *        by a preconfigured ISR internally.
         * 
         * @param steps Total number of steps to transmit
         * @param dir   Direction the stepper is to move
         * @param speed Speed of the stepper motor
         * 
         * @return ESP_OK if all steps were queued to the RMT peripheral successfully,
         *         error code otherwise
         * 
         * @note The unit of speed is in `steps/sec`
         */
        [[nodiscard]] esp_err_t send_steps_async(uint32_t steps, dir_t dir, uint32_t speed) {

            if (!m_is_initialized) return ESP_ERR_INVALID_STATE;

            if ((steps == 0) || (speed == 0)) return ESP_ERR_INVALID_ARG;
            
            // Set direction
            gpio_set_level(m_config.dir_pin, static_cast<uint8_t>(dir));
            
            // Find the number of ticks per step
            const uint32_t num_of_ticks_per_steps = m_config.rmt_frequency / speed;
            const uint32_t half_period = num_of_ticks_per_steps / 2U;

            // Prepare steps
            const rmt_symbol_word_t words = {
                .duration0 = half_period,
                .level0 = 1,
                .duration1 = static_cast<uint32_t>(half_period),
                .level1 = 0
            };

            // Transmission settings
            const rmt_transmit_config_t config = {
                .loop_count = static_cast<int>(steps),
                .flags = {
                    .eot_level = 1,
                    .queue_nonblocking = 0
                }
            };

            // Set this to `nullptr` so that the ISR doesn't
            // send any notification as we do not need it
            m_calling_task_handle = nullptr;

            // Queue steps and return immediately
            return rmt_transmit(m_chan_handle, m_copy_enc_handle, &words, sizeof(words), &config);
        }

        /**
         * @brief Sends the steps to the stepper motor driver with the
         *        given number of steps and the specified direction. This
         *        function blocks until all steps have been sent or when an
         *        error occurs. This function takes in the max speed the
         *        stepper is to get to, as well as the acceleration to get to
         *        the speed. It uses a trapezoidal acceleration curve.
         * 
         * @param steps Total number of steps to transmit
         * @param dir   Direction the stepper is to move
         * @param speed Max speed the stepper is to reach
         * @param accel Value of the acceleration to get to
         *              the max speed
         * 
         * @return ESP_OK if all steps were sent successfully,
         *         error code otherwise
         * 
         * @note The value of the acceleration is also used as the decceleration.
         * @note The unit of speed is `steps/sec` and acceleration is `steps/(sec ^ 2)`
         */
        [[nodiscard]] esp_err_t send_steps_with_accel(uint32_t steps, dir_t dir, uint32_t speed, uint32_t accel) {
            
            if (!m_is_initialized) return ESP_ERR_INVALID_STATE;

            if ((steps == 0) || (speed == 0)) return ESP_ERR_INVALID_ARG;
            if (accel == 0) return send_steps(steps, dir, speed);

            // Set direction
            gpio_set_level(m_config.dir_pin, static_cast<uint8_t>(dir));

            // Set this to `nullptr` so that the ISR doesn't
            // send any notification as we do not need it
            m_calling_task_handle = nullptr;

            esp_err_t ret = queue_accel_and_cruise_steps(steps, speed, accel);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to queue rmt steps: %s", esp_err_to_name(ret));
                return ret;
            }

            // Wait till all transfers are done
            ret = rmt_tx_wait_all_done(m_chan_handle, -1);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Operation failed while waiting for rmt tx completion: %s", esp_err_to_name(ret));
                return ret;
            }
            
            return ESP_OK;
        }

        /**
         * @brief Sends the steps to the stepper motor driver with the
         *        given number of steps and the specified direction. This
         *        function configures the RMT peripheral to transmit the
         *        steps but returns immediately. The completion is handled
         *        by a preconfigured ISR internally. This function takes
         *        in the max speed the stepper is to get to, as well as the
         *        acceleration to get to the speed. It uses a trapezoidal
         *        acceleration curve.
         * 
         * @param steps Total number of steps to transmit
         * @param dir   Direction the stepper is to move
         * @param speed Max speed the stepper is to reach
         * @param accel Value of the acceleration to get to
         *              the max speed
         * 
         * @return ESP_OK if all steps were queued to the RMT peripheral
         *         successfully, error code otherwise
         * 
         * @note The value of the acceleration is also used as the decceleration
         * @note The unit of speed is `steps/sec` and acceleration is `steps/(sec ^ 2)`
         */
        [[nodiscard]] esp_err_t send_steps_async_with_accel(uint32_t steps, dir_t dir, uint32_t speed, uint32_t accel) {
            
            if (!m_is_initialized) return ESP_ERR_INVALID_STATE;

            if ((steps == 0) || (speed == 0)) return ESP_ERR_INVALID_ARG;
            if (accel == 0) return send_steps_async(steps, dir, speed);

            // Set direction
            gpio_set_level(m_config.dir_pin, static_cast<uint8_t>(dir));

            // Set this to `nullptr` so that the ISR doesn't
            // send any notification as we do not need it
            m_calling_task_handle = nullptr;

            esp_err_t ret = queue_accel_and_cruise_steps(steps, speed, accel);
            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to queue rmt steps: %s", esp_err_to_name(ret));
                return ret;
            }
            
            return ESP_OK;
        }

    };
    
} // namespace a4988


#endif // STEPPER_HPP_