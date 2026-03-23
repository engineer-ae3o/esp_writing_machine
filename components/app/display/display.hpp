#ifndef DISPLAY_HPP_
#define DISPLAY_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ili9341.h"

#include "esp_err.h"


namespace display {

    /**
     * @brief Initializes LVGL and the display interface
     * 
     * @param[in] handle Handle to the current instance of the driver being used
     * @param[out] disp_mutex Mutex to ensure thread safety across lvgl api calls
     * 
     * @return ESP_OK on sucess, error code otherwise
     */
    esp_err_t init(const ili9341_handle_t& handle, SemaphoreHandle_t& disp_mutex);
    
    /**
     * @brief Deinitializes the display interface
     */
    esp_err_t deinit();

    /**
     * @brief Displays the bootup screen
     */
    void bootup_screen();
    
    /**
     * @brief This creates all UI screens and displays first screen
     */
    void create_ui();
    
    /**
     * @brief Switches to next screen
     */
    void next_screen();

    /**
     * @brief Switches to previous screen
     */
    void prev_screen();

} // namespace display


#endif // DISPLAY_HPP_