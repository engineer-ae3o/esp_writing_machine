#ifndef DISPLAY_HPP_
#define DISPLAY_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ili9341.h"

#include "esp_err.h"

#include <cstdint>


namespace display {

    /**
     * @brief Initializes LVGL and the display interface
     * 
     * @param[in] handle Handle to the current instance of the hardware driver being used
     * @param[out] disp_mutex Mutex to ensure thread safety across lvgl api calls
     * 
     * @return ESP_OK on sucess, error code otherwise
     */
    esp_err_t init(const ili9341_handle_t& handle, SemaphoreHandle_t& disp_mutex);
    
    /**
     * @brief Deinitializes the display interface
     * 
     * @return ESP_OK on sucess, error code otherwise
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
     * @brief Updates the currently active screen with data that has changed
     */
    void update_ui();

    enum class event_type_t : uint8_t {
        NONE = 0,

        // Plotter events
        PLOTTING_PAUSED,
        PLOTTING_RESUMED,
        PLOTTING_STOPPED,
        PLOTTING_COMPLETE,

        // File errors
        FILE_NOT_FOUND,
        PARSER_ERROR,
        FILE_READ_ERROR,

        // WiFi events
        WIFI_ENABLED,
        WIFI_DISABLED,
        FILE_RECEIVED,

        CLEAR_ALL_POPUPS
    };

    struct event_t {
        event_type_t event{event_type_t::NONE};

        // Required for `PLOTTING_RESUMED` event
        size_t from_line{};
        
        // Required for `PARSER_ERROR` event
        size_t line_num{};
        const char* line_str{};

        // Required for `PLOTTING_STOPPED` event
        size_t completed_lines{};
        size_t total_lines{};
    };

    void send_event(const event_t& event);

} // namespace display


#endif // DISPLAY_HPP_