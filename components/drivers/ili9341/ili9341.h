#ifndef _ILI9341_H_
#define _ILI9341_H_

#ifdef __cplusplus
extern "C" {
#endif


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#include <stdint.h>
#include <stdbool.h>


#define ILI9341_MAX_WIDTH  240
#define ILI9341_MAX_HEIGHT 320

// Number of instances of the display you want to use
#define ILI9341_MAX_INSTANCES 1

// Configuration and default settings
#define ILI9341_TIMEOUT_MS                    50U
#define ILI9341_DEFAULT_MAX_RETRIES           4U
#define ILI9341_DEFAULT_QUEUE_SIZE            10U
#define ILI9341_DEFAULT_TASK_PRIORITY         8U
#define ILI9341_DEFAULT_TASK_CORE             1U
#define ILI9341_DEFAULT_TASK_STACK_SIZE       4096U


typedef struct {

    // SPI configuration
    spi_host_device_t spi_host;
    uint32_t spi_clock_speed_hz;

    // GPIO pins
    gpio_num_t pin_mosi;
    gpio_num_t pin_sclk;
    gpio_num_t pin_cs;
    gpio_num_t pin_dc;
    gpio_num_t pin_rst;

    // Display parameters
    uint16_t width;
    uint16_t height;
    uint8_t rotation;               // 0-3 for different orientations

    // Error handling
    uint8_t max_retries;            // Number of retry attempts on SPI failure (default: ILI9341_DEFAULT_MAX_RETRIES)

    // Task configuration
    uint8_t queue_size;             // Flush request queue size (default: ILI9341_DEFAULT_QUEUE_SIZE)
    uint8_t task_priority;          // FreeRTOS task priority (default: ILI9341_DEFAULT_TASK_PRIORITY)
    uint8_t task_core;              // CPU core to pin task to (default: ILI9341_DEFAULT_TASK_CORE)
    uint16_t task_stack_size;       // Task stack size in bytes (default: ILI9341_DEFAULT_TASK_STACK_SIZE)

} ili9341_config_t;


// Callback invoked when flush operation completes
typedef void (*ili9341_flush_cb_t)(void* user_data, esp_err_t result);

// Handle by which the current driver instance can be referenced
typedef struct ili9341_driver_t ili9341_driver_t;
typedef ili9341_driver_t* ili9341_handle_t;


/**
 * @brief Initialize the ili9341 driver
 *
 * @param[in] config Pointer to struct containing driver configuration
 * @param[out] handle Pointer to the handle of the current driver instance
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ili9341_init(const ili9341_config_t* config, ili9341_handle_t* handle);
 
/**
 * @brief Deinitialize ili9341 driver and free resources
 * 
 * @param[out] handle Pointer to the handle of the current driver instance
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ili9341_deinit(ili9341_handle_t* handle);
 
/**
 * @brief Async flush pixel data to display.
 * Non-blocking: returns immediately, callback invoked when transfer completes
 *
 * @param x1, y1 Top-left corner of update region
 * @param x2, y2 Bottom-right corner of update region
 * @param pixel_data RGB565 pixel buffer
 * @param pixel_count Number of pixels
 * @param callback Function to call when flush completes (receives result code of operation)
 * @param user_data Passed to callback
 * @param[in] handle Pointer to the handle of the current driver instance
 *
 * @return ESP_OK if data queued successfully, error code otherwise
 */
esp_err_t ili9341_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                        const uint16_t* pixel_data, size_t pixel_count,
                        ili9341_flush_cb_t callback, void* user_data, ili9341_handle_t handle);

/**
 * @brief Sets full screen to specified color in RGB565 format
 * 
 * @param color Color to be displayed
 * @param callback Function to call when flush completes (receives result code)
 * @param user_data Passed to callback
 * @param[in] handle Pointer to the handle of the current driver instance
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ili9341_set_screen(uint16_t color, ili9341_flush_cb_t callback, void* user_data, ili9341_handle_t handle);
 
/**
 * @brief Check if driver is ready for new flush operation
 * 
 * @param[in] handle Pointer to the handle of the current driver instance
 *
 * @return true if idle, false if busy
 */
bool ili9341_is_ready(ili9341_handle_t handle);


#ifdef __cplusplus
}
#endif


#endif // _ILI9341_H_