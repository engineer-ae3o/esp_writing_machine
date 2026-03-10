#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "stepper.hpp"
#include "st7920.hpp"
#include "config.hpp"
#include "utils.hpp"

#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_log.h"

#include <cstdio>


extern "C" {
    void app_main() {

        constexpr const char TAG[] = "MAIN";

        constexpr st7920::config_t config = {

            .spi_host = SPI2_HOST,
            .spi_clock_speed_hz = 500'000,
            
            .pin_mosi = config::MOSI_PIN,
            .pin_sclk = config::SCLK_PIN,
            .pin_cs = config::CS_PIN,
            
            .width = 128,
            .height = 64,
            
            .max_retries = 4,
            
            .queue_size = 10,
            .task_priority = 7,
            .task_core = 1,
            .task_stack_size = 3072
        };

        auto [display, ret] = st7920::st7920_t::create(config);
        if (!display) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to initialize the ST7920: %s", esp_err_to_name(ret));
            return;
        }

        ESP_ERROR_CHECK(display->set_screen(true, nullptr, nullptr));

        while (1) {
            vTaskDelay(10000);
        }
    }

} // extern "C"
