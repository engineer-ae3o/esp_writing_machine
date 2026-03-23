#ifndef APP_CONFIG_HPP_
#define APP_CONFIG_HPP_


#include "driver/gpio.h"
#include <cstdint>


namespace config {

    // Filesystem settings
    constexpr inline const char FILE_PARTITION[]             = "storage";
    constexpr inline const char FILE_BASE_PATH[]             = "/device";
    constexpr inline const char INDEX_HTML_FILE_PATH[]       = "/device/index.html";
    constexpr inline const char GCODE_FILE_PATH[]            = "/device/user/file.gcode";

    constexpr inline const char TEST_GCODE_FILE_1_PATH[]     = "/device/test/test_file_1.gcode";
    constexpr inline const char TEST_GCODE_FILE_2_PATH[]     = "/device/test/test_file_2.gcode";
    constexpr inline const char TEST_GCODE_FILE_3_PATH[]     = "/device/test/test_file_3.gcode";

    // WiFi settings
    constexpr inline uint8_t MAX_WIFI_CONNECTIONS            = 4;
    constexpr inline uint32_t MAX_FILE_SIZE_BYTES            = 2'000'000;
    constexpr inline const char WIFI_SSID_NAME[]             = "ESP-Pen-Plotter";
    constexpr inline const char IP_ADDRESS[]                 = "192.168.4.1";

    // GPIO pin connections
    constexpr inline gpio_num_t MOSI_PIN                     = GPIO_NUM_25;
    constexpr inline gpio_num_t SCLK_PIN                     = GPIO_NUM_27;
    constexpr inline gpio_num_t CS_PIN                       = GPIO_NUM_26;
    constexpr inline gpio_num_t RST_PIN                      = GPIO_NUM_1;
    constexpr inline gpio_num_t DC_PIN                       = GPIO_NUM_1;
    
    constexpr inline gpio_num_t ENC_A_PIN                    = GPIO_NUM_35;
    constexpr inline gpio_num_t ENC_B_PIN                    = GPIO_NUM_32;
    constexpr inline gpio_num_t ENC_BTN_PIN                  = GPIO_NUM_33;
    constexpr inline gpio_num_t RST_BTN_PIN                  = GPIO_NUM_15;
    constexpr inline gpio_num_t BEEPER_PIN                   = GPIO_NUM_2;
    
    constexpr inline gpio_num_t SERVO_PIN                    = GPIO_NUM_4;
    
    constexpr inline gpio_num_t X_STEP_PIN                   = GPIO_NUM_1;
    constexpr inline gpio_num_t X_DIR_PIN                    = GPIO_NUM_1;
    constexpr inline gpio_num_t X_ENA_PIN                    = GPIO_NUM_1;

    constexpr inline gpio_num_t Y_STEP_PIN                   = GPIO_NUM_1;
    constexpr inline gpio_num_t Y_DIR_PIN                    = GPIO_NUM_1;
    constexpr inline gpio_num_t Y_ENA_PIN                    = GPIO_NUM_1;
    
    constexpr inline gpio_num_t MICROSTEP_1_PIN              = GPIO_NUM_1;
    constexpr inline gpio_num_t MICROSTEP_2_PIN              = GPIO_NUM_1;
    constexpr inline gpio_num_t MICROSTEP_3_PIN              = GPIO_NUM_1;

    // Display settings
    constexpr inline uint32_t LCD_WIDTH                      = 320;
    constexpr inline uint32_t LCD_HEIGHT                     = 240;
    constexpr inline uint32_t LCD_ORIENTATION                = 2;

    // Timeout
    constexpr inline uint32_t TIMEOUT_MS                     = 150;
    constexpr inline uint32_t LVGL_TIMER_PERIOD_MS           = 10;

}


#endif // APP_CONFIG_HPP_