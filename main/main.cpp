#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "stepper.hpp"
#include "config.hpp"
#include "utils.hpp"

#include <cstdio>


extern "C" {

    void app_main() {

        constexpr const char TAG[] = "MAIN";

        constexpr a4988::config_t config = {

            .step_pin = config::X_STEP_PIN,
            .dir_pin = config::X_DIR_PIN,
            .ena_pin = config::X_ENA_PIN,

            .rmt_frequency = 1'000'000,
            .dma_buf_size = 1'024,
            
            .queue_depth = 25,
            
            .microstep_1 = config::MICROSTEP_1_PIN,
            .microstep_2 = config::MICROSTEP_2_PIN,
            .microstep_3 = config::MICROSTEP_3_PIN,

            .is_active_low = true
        };

        auto stepper = a4988::driver_t<a4988::microstep_t::ONE_SIXTEENTH>::create(config);
        if (!stepper) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to initialize the stepper motor driver: %s", esp_err_to_name(stepper.error()));
            return;
        }
        
        ESP_ERROR_CHECK(stepper.value()->enable());

        constexpr uint32_t SLOW_SPEED   = 1'000;
        constexpr uint32_t MID_SPEED    = 5'000;
        constexpr uint32_t FAST_SPEED   = 10'000;

        constexpr uint32_t ACCELERATION = 100;

        while (1) {
            ESP_ERROR_CHECK(stepper.value()->send_steps(1000, a4988::dir_t::CLOCKWISE, SLOW_SPEED));
            vTaskDelay(pdMS_TO_TICKS(5000));

            ESP_ERROR_CHECK(stepper.value()->send_steps_async(1000, a4988::dir_t::CLOCKWISE, MID_SPEED));
            vTaskDelay(pdMS_TO_TICKS(5000));

            ESP_ERROR_CHECK(stepper.value()->send_steps_async_with_accel(2000, a4988::dir_t::ANTICLOCKWISE, FAST_SPEED, ACCELERATION));
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }

} // extern "C"
