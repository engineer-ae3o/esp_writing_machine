#ifndef SERVER_HPP_
#define SERVER_HPP_


#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_err.h"


namespace http {

    /**
     * @brief Initialize the wifi and http server interface
     * 
     * @param[out] event_semphr This is used by the `POST` event handler
     *                          to notify that a file has been received
     *                          successfully and is free to use. 
     * 
     * @return ESP_OK on success, error code otherwise
     * 
     * @note There are no guarantees that the file
     *       would not be accessed by the driver even after
     *       sending the semaphore so it is advised to stop
     *       the webserver after receiving this semaphore to
     *       prevent accesses to the file by the driver, and
     *       to only start the server when the file is not
     *       needed anymore and is no longer in use.
     */
    esp_err_t init(SemaphoreHandle_t& event_semphr);

    /**
     * @brief Deinitialize the wifi and http server interface
     * 
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t deinit();

    /**
     * @brief Start the wifi and http server interface
     * 
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t start();

    /**
     * @brief Stop the wifi and http server interface
     * 
     * @return ESP_OK on success, error code otherwise
     */
    esp_err_t stop();

} // namespace http


#endif // SERVER_HPP_