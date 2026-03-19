#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "server.hpp"
#include "utils.hpp"
#include "config.hpp"

#include "esp_http_server.h"
#include "esp_littlefs.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <cstdio>


namespace http {

    static constexpr const char* TAG = "HTTP_Server";

    static bool s_is_initialized{false};
    static bool s_is_http_started{false};

    static httpd_handle_t s_handle{nullptr};

    static SemaphoreHandle_t s_mutex{};
    static SemaphoreHandle_t s_file_event_semphr{};
    static constexpr uint8_t TIMEOUT_MS{100};

    static constexpr size_t FILE_CHUNK_SIZE_BYTES{512};


    // Forward declarations
    static esp_err_t cleanup();
    static esp_err_t get_handler(httpd_req_t* request);
    static esp_err_t post_handler(httpd_req_t* request);


    // Public API
    esp_err_t init(SemaphoreHandle_t& event_semphr) {

        if (s_is_initialized) {
            utils::log<utils::log_level_t::WARN>(TAG, "HTTP server interface already initialized");
            return ESP_ERR_INVALID_STATE;
        }

        utils::log<utils::log_level_t::INFO>(TAG, "Initializing http server interface");

        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create http server mutex");
            return ESP_ERR_NO_MEM;
        }

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to take the server mutex");
            cleanup();
            return ESP_ERR_TIMEOUT;
        }

        s_file_event_semphr = xSemaphoreCreateBinary();
        if (!s_file_event_semphr) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to file notification semaphore");
            return ESP_ERR_NO_MEM;
        }

        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ASSERT((nvs_flash_erase() == ESP_OK));
            ret = nvs_flash_init();
        }
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to initialize nvs flash: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        ret = esp_event_loop_create_default();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        ret = esp_netif_init();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to initialize the ESP32's network interface: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        esp_netif_t* netif = esp_netif_create_default_wifi_ap();
        if (!netif) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create default wifi AP");
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        constexpr esp_vfs_littlefs_conf_t config = {
            .base_path = config::FILE_BASE_PATH,
            .partition_label = config::FILE_PARTITION,
            .partition = nullptr,
            .sdcard = nullptr,
            .format_if_mount_failed = 1,
            .read_only = 0,
            .dont_mount = 0,
            .grow_on_mount = 1
        };

        ret = esp_vfs_littlefs_register(&config);
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to register littlefs partition: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        size_t total_bytes{};
        size_t used_bytes{};

        ret = esp_littlefs_info(config::FILE_PARTITION, &total_bytes, &used_bytes);
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to get littlefs partition info: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        utils::log<utils::log_level_t::INFO>(TAG, "Size of filesystem: %luKB. Used bytes: %luKB",
                                                  (total_bytes / 1024), (used_bytes / 1024));
        
        wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
        ret = esp_wifi_init(&wifi_init_config);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to initialize wifi: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        ret = esp_wifi_set_mode(WIFI_MODE_AP);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to set wifi mode: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        wifi_config_t wifi_config{};
        snprintf(reinterpret_cast<char*>(wifi_config.ap.ssid), sizeof(wifi_config.ap.ssid), config::WIFI_SSID_NAME);
        wifi_config.ap.ssid_len = strlen(config::WIFI_SSID_NAME);
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.ssid_hidden = 0;
        wifi_config.ap.max_connection = config::MAX_WIFI_CONNECTIONS;

        ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to initialize wifi: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            cleanup();
            return ret;
        }

        event_semphr = s_file_event_semphr;

        s_is_initialized = true;
        xSemaphoreGive(s_mutex);
        
        utils::log<utils::log_level_t::INFO>(TAG, "Initialization complete");

        return ESP_OK;
    }

    esp_err_t deinit() {

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to take the server mutex");
            return ESP_ERR_TIMEOUT;
        }

        if (!s_is_initialized) {
            utils::log<utils::log_level_t::WARN>(TAG, "HTTP server interface already in an uninitialized state");
            xSemaphoreGive(s_mutex);
            return ESP_ERR_INVALID_STATE;
        }

        utils::log<utils::log_level_t::INFO>(TAG, "Deinitializing the HTTP server");

        xSemaphoreGive(s_mutex);
        esp_err_t ret = cleanup();

        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to cleanup resources: %s", esp_err_to_name(ret));
            return ret;
        }

        s_is_initialized = false;
        
        utils::log<utils::log_level_t::INFO>(TAG, "HTTP server deinitialized");
        
        return ESP_OK;
    }

    esp_err_t start() {

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to take the server mutex");
            return ESP_ERR_TIMEOUT;
        }

        if (s_is_http_started) {
            utils::log<utils::log_level_t::WARN>(TAG, "HTTP server already started");
            xSemaphoreGive(s_mutex);
            return ESP_ERR_INVALID_STATE;
        }

        utils::log<utils::log_level_t::INFO>(TAG, "Starting HTTP server");

        esp_err_t ret = esp_wifi_start();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to start wifi access point: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            return ret;
        }
        
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        constexpr httpd_uri_t uri_post_file = {
            .uri = "/upload",
            .method = HTTP_POST,
            .handler = post_handler,
            .user_ctx = nullptr
        };

        constexpr httpd_uri_t uri_get_index = {
            .uri     = "/",
            .method  = HTTP_GET,
            .handler = get_handler,
            .user_ctx = nullptr
        };
        
        // Start the http server
        if (httpd_start(&s_handle, &config) == ESP_OK) {
            // Register URI handlers
            ASSERT(httpd_register_uri_handler(s_handle, &uri_post_file) == ESP_OK);
            ASSERT(httpd_register_uri_handler(s_handle, &uri_get_index) == ESP_OK);
        } else {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to start http webserver: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            return ret;
        }
        
        s_is_http_started = true;
        xSemaphoreGive(s_mutex);
        
        utils::log<utils::log_level_t::INFO>(TAG, "HTTP server running");

        return ESP_OK;
    }
    
    esp_err_t stop() {

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to take the server mutex");
            return ESP_ERR_TIMEOUT;
        }

        if (!s_is_http_started) {
            utils::log<utils::log_level_t::WARN>(TAG, "HTTP server already stopped");
            xSemaphoreGive(s_mutex);
            return ESP_ERR_INVALID_STATE;
        }

        utils::log<utils::log_level_t::INFO>(TAG, "Stopping HTTP server");
        
        esp_err_t ret = httpd_stop(s_handle);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to stop http webserver: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            return ret;
        }

        s_handle = nullptr;

        ret = esp_wifi_stop();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to stop wifi access point: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            return ret;
        }

        s_is_http_started = false;
        xSemaphoreGive(s_mutex);

        utils::log<utils::log_level_t::WARN>(TAG, "HTTP server stopped");

        return ESP_OK;
    }

    // Private functions
    static esp_err_t cleanup() {

        esp_err_t ret{ESP_OK};

        if (s_handle) {
            ret = httpd_stop(s_handle);
            if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                utils::log<utils::log_level_t::ERROR>(TAG, "Failed to stop http webserver: %s", esp_err_to_name(ret));
                xSemaphoreGive(s_mutex);
                return ret;
            }
            s_handle = nullptr;
        }

        ret = esp_wifi_stop();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to stop wifi access point: %s", esp_err_to_name(ret));
            xSemaphoreGive(s_mutex);
            return ret;
        }
        
        ret = esp_wifi_deinit();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to deinitialize wifi: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = esp_vfs_littlefs_unregister(config::FILE_PARTITION);
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to unmount littlefs partition: %s", esp_err_to_name(ret));
            return ret;
        }

        // Wea also check for `ESP_ERR_NOT_SUPPORTED` because deinitialization is not
        // supported yet
        ret = esp_netif_deinit();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE) && (ret != ESP_ERR_NOT_SUPPORTED )) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to deinitialize the ESP32's network interface: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = esp_event_loop_delete_default();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to delete the event loop: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = nvs_flash_deinit();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to deinitialize the nvs flash: %s", esp_err_to_name(ret));
            return ret;
        }

        if (s_mutex) {
            vSemaphoreDelete(s_mutex);
            s_mutex = nullptr;
        }

        if (s_file_event_semphr) {
            vSemaphoreDelete(s_file_event_semphr);
            s_file_event_semphr = nullptr;
        }

        return ESP_OK;
    }

    static esp_err_t get_handler(httpd_req_t* request) {

        utils::log<utils::log_level_t::INFO>(TAG, "HTTP GET API handler triggered. Serving index.html");

        FILE* file_handle = fopen(config::INDEX_HTML_FILE_PATH, "r");
        if (!file_handle) {
            httpd_resp_send_err(request, HTTPD_404_NOT_FOUND, "index.html not found on filesystem");
            utils::log<utils::log_level_t::ERROR>(TAG, "index.html not found on filesystem");
            return ESP_FAIL;
        }

        httpd_resp_set_type(request, "text/html");

        char chunk[FILE_CHUNK_SIZE_BYTES]{};
        size_t read_bytes{};

        do {
            read_bytes = fread(chunk, 1, sizeof(chunk), file_handle);
            if (read_bytes > 0) {
                if (httpd_resp_send_chunk(request, chunk, static_cast<ssize_t>(read_bytes)) != ESP_OK) {
                    utils::log<utils::log_level_t::ERROR>(TAG, "Failed to send chunk: client likely disconnected");
                    fclose(file_handle);
                    httpd_resp_sendstr_chunk(request, nullptr); // Terminate chunked response
                    return ESP_FAIL;
                }
            }
        } while (read_bytes == sizeof(chunk));

        fclose(file_handle);
        httpd_resp_sendstr_chunk(request, nullptr); // Signal end of chunked transfer

        utils::log<utils::log_level_t::INFO>(TAG, "index.html sent successfully");

        return ESP_OK;
    }

    static esp_err_t post_handler(httpd_req_t* request) {

        utils::log<utils::log_level_t::INFO>(TAG, "HTTP POST API handler triggered: Reading file from client");
        
        // No need to delete the previous file (if any) as opening a file
        // with the `"w"` access specifier overwrites the file if it exists
        // since it has the same name. The previous file gets zeroed out.
        // This is the intended behaviour Managing multiple files both at the
        // backend (here) and at the user level would add unnecessary complexity
        // for almost zero benefit. Also, a user of this file would have to make
        // sure that no two threads access file at the same time. `server.cpp` is
        // not aware of any other threads or TUs that may want to access the same
        // file so the user of this file must add synchronization themselves. This
        // allows for a simpler and easy to use API in `server.hpp` and `server.cpp`
        FILE* file_handle = fopen(config::GCODE_FILE_PATH, "w");
        if (!file_handle) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
            return ESP_FAIL;
        }

        int remaining_bytes = request->content_len;
        if (remaining_bytes > config::MAX_FILE_SIZE_BYTES) {
            httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "File too large");
            fclose(file_handle);
            return ESP_FAIL;
        }
        
        char file_chunk_buf[FILE_CHUNK_SIZE_BYTES]{};

        while (remaining_bytes > 0) {

            int received_bytes = httpd_req_recv(request, file_chunk_buf, sizeof(file_chunk_buf));

            if (received_bytes == HTTPD_SOCK_ERR_TIMEOUT) {
                utils::log<utils::log_level_t::ERROR>(TAG, "Timeout while waiting for socket recv()");
                fclose(file_handle);
                return ESP_ERR_WIFI_TIMEOUT;
            } else if (received_bytes == HTTPD_SOCK_ERR_FAIL) {
                utils::log<utils::log_level_t::ERROR>(TAG, "Unrecoverable error while calling recv()");
                fclose(file_handle);
                return ESP_FAIL;
            } else if (received_bytes <= 0) {
                utils::log<utils::log_level_t::ERROR>(TAG, "HTTP server failure: Peer likely closed the connection");       
                fclose(file_handle);
                return ESP_FAIL;
            }
            
            ASSERT(fwrite(file_chunk_buf, received_bytes, 1, file_handle) == 1);

            remaining_bytes -= received_bytes;
        }
        
        fclose(file_handle);

        httpd_resp_send(request, "File received successfully", HTTPD_RESP_USE_STRLEN);

        // Give semaphore to indicate that the file can now be used
        xSemaphoreGive(s_file_event_semphr);
        
        utils::log<utils::log_level_t::INFO>(TAG, "File received successfully");

        return ESP_OK;
    }

} // namespace http
