#include "st7920.hpp"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <cstring>


namespace st7920 {

    enum class log_level_t : uint8_t {
        NONE = 0,
        ERROR,
        WARN,
        INFO
    };

    static constexpr log_level_t LOG_LEVEL = log_level_t::INFO;
    static constexpr const char* TAG = "ST7920";

    template <log_level_t level, typename... Args>
    void log(const char* fmt, Args&&... args) {
        if constexpr (level <= LOG_LEVEL) {
            // This is a work around. The ESP_LOGx macros
            // expect a string literal
            char msg[256]{};
            snprintf(msg, sizeof(msg), fmt, args...);
            if constexpr (level == log_level_t::ERROR)     ESP_LOGE(TAG, "%s", msg);
            else if constexpr (level == log_level_t::WARN) ESP_LOGW(TAG, "%s", msg);
            else if constexpr (level == log_level_t::INFO) ESP_LOGI(TAG, "%s", msg);
        }
    }

    static constexpr uint8_t TIMEOUT_MS{100};
    static constexpr uint8_t COMMAND_SYNC_BYTE{0xF8};
    static constexpr uint8_t DATA_SYNC_BYTE{0xFA};
    static constexpr uint8_t ADDRESS_SYNC_BYTE{0x80};

    st7920_t::st7920_t(const config_t& config) : m_config(config) {}

    st7920_t::~st7920_t() {
        // Get task handle of the currently running task
        m_deinit_task_handle = xTaskGetCurrentTaskHandle();

        // Signal task to shutdown
        m_shutdown_requested = true;

        if (m_flush_queue) {
            flush_req_t dummy{};
            // Unblock task if it was in a blocked state waiting for data from the queue
            xQueueSend(m_flush_queue, &dummy, pdMS_TO_TICKS(TIMEOUT_MS));
        }

        // Give mutex back before blocking and deletion
        xSemaphoreGive(m_handle_mutex);

        if (m_task_handle) {
            // Block till we receive notification from the task to be deleted
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TIMEOUT_MS)); 
        }
        m_deinit_task_handle = nullptr;

        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to properly destruct ST7920 instance: %s", esp_err_to_name(ret));
        }
    }

    st7920_t::st7920_t(st7920_t&& other) {
        m_config = other.m_config;
        m_spi_handle = other.m_spi_handle;
        m_state = other.m_state;
        m_spi_done_sem = other.m_spi_done_sem;
        m_flush_queue = other.m_flush_queue;
        m_task_handle = other.m_task_handle;
        m_handle_mutex = other.m_handle_mutex;
        m_is_initialized = other.m_is_initialized;
        m_shutdown_requested = other.m_shutdown_requested;
        m_deinit_task_handle =other.m_deinit_task_handle;
        m_pixels_buf = other.m_pixels_buf;
        m_size_of_pixel_buf_bytes = other.m_size_of_pixel_buf_bytes;
        m_dma_semphr = other.m_dma_semphr;
        m_wire_buf = other.m_wire_buf;
        m_wire_buf_len = other.m_wire_buf_len;

        other.m_config = {};
        other.m_spi_handle = {};
        other.m_state = {};
        other.m_spi_done_sem = {};
        other.m_flush_queue = {};
        other.m_task_handle = {};
        other.m_handle_mutex = {};
        other.m_is_initialized = {};
        other.m_shutdown_requested = {};
        other.m_deinit_task_handle = {};
        other.m_pixels_buf = {};
        other.m_size_of_pixel_buf_bytes = {};
        other.m_dma_semphr = {};
        other.m_wire_buf = {};
        other.m_wire_buf_len = {};
    }

    st7920_t& st7920_t::operator=(st7920_t&& other) {
        if (this != &other) {
            cleanup();

            m_config = other.m_config;
            m_spi_handle = other.m_spi_handle;
            m_state = other.m_state;
            m_spi_done_sem = other.m_spi_done_sem;
            m_flush_queue = other.m_flush_queue;
            m_task_handle = other.m_task_handle;
            m_handle_mutex = other.m_handle_mutex;
            m_is_initialized = other.m_is_initialized;
            m_shutdown_requested = other.m_shutdown_requested;
            m_deinit_task_handle =other.m_deinit_task_handle;
            m_pixels_buf = other.m_pixels_buf;
            m_size_of_pixel_buf_bytes = other.m_size_of_pixel_buf_bytes;
            m_dma_semphr = other.m_dma_semphr;
            m_wire_buf = other.m_wire_buf;
            m_wire_buf_len = other.m_wire_buf_len;
            
            other.m_config = {};
            other.m_spi_handle = {};
            other.m_state = {};
            other.m_spi_done_sem = {};
            other.m_flush_queue = {};
            other.m_task_handle = {};
            other.m_handle_mutex = {};
            other.m_is_initialized = {};
            other.m_shutdown_requested = {};
            other.m_deinit_task_handle = {};
            other.m_pixels_buf = {};
            other.m_size_of_pixel_buf_bytes = {};
            other.m_dma_semphr = {};
            other.m_wire_buf = {};
            other.m_wire_buf_len = {};
        }

        return *this;
    }

    // Public API
    std::expected<std::unique_ptr<st7920_t>, esp_err_t> st7920_t::create(const config_t& config) {
        // Object has to be heap allocated for it to be returned/moved as we
        // pass in a `this` pointer during initialization and the original
        // would have been destructed when we leave this function
        auto display = std::unique_ptr<st7920_t>(new st7920_t(config));

        esp_err_t ret = display->init();
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Initialization failed: %s", esp_err_to_name(ret));
            display->cleanup();
            return std::unexpected(ret);
        }

        return display;
    }

    [[nodiscard]] esp_err_t st7920_t::init() {

        if (m_is_initialized) {
            log<log_level_t::WARN>("ST7920 instance already initialized");
            return ESP_OK;
        }

        log<log_level_t::INFO>("Initializing ST7920 handle");

        // We create the mutex here because we need it to ensure thread safety
        m_handle_mutex = xSemaphoreCreateMutex();
        if (!m_handle_mutex) return ESP_FAIL;

        if (xSemaphoreTake(m_handle_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            log<log_level_t::ERROR>("Failed to take mutex during initialization");
            return ESP_ERR_TIMEOUT;
        }

        // Allocate DMA buffer
        m_size_of_pixel_buf_bytes = (m_config.width * m_config.height) / 8; // 1 bit per pixel
        m_pixels_buf = static_cast<uint8_t*>(heap_caps_malloc(m_size_of_pixel_buf_bytes, MALLOC_CAP_8BIT));
        if (!m_pixels_buf) {
            log<log_level_t::ERROR>("Failed to allocate pixel buffer");
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ESP_ERR_NO_MEM;
        }

        m_wire_buf = static_cast<uint8_t*>(heap_caps_malloc(m_size_of_pixel_buf_bytes * 4, MALLOC_CAP_DMA));
        if (!m_wire_buf) {
            log<log_level_t::ERROR>("Failed to allocate dma wire buffer");
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ESP_ERR_NO_MEM;
        }

        m_state = state_t::IDLE;
        m_shutdown_requested = false;
        
        // Configure SPI bus
        spi_bus_config_t bus_cfg{};
        bus_cfg.mosi_io_num = m_config.pin_mosi;
        bus_cfg.miso_io_num = -1;
        bus_cfg.sclk_io_num = m_config.pin_sclk;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = m_size_of_pixel_buf_bytes * 4;
        bus_cfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO;

        esp_err_t ret = spi_bus_initialize(m_config.spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("SPI bus init failed: %s", esp_err_to_name(ret));
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ret;
        }

        // Configure SPI device
        spi_device_interface_config_t dev_cfg{};
        dev_cfg.mode = 3;
        dev_cfg.clock_source = SPI_CLK_SRC_DEFAULT;
        dev_cfg.clock_speed_hz = static_cast<int>(m_config.spi_clock_speed_hz);
        dev_cfg.spics_io_num = m_config.pin_cs;
        dev_cfg.flags = SPI_DEVICE_POSITIVE_CS;
        dev_cfg.queue_size = m_config.queue_size;
        dev_cfg.post_cb = spi_post_transfer_callback;

        ret = spi_bus_add_device(m_config.spi_host, &dev_cfg, &m_spi_handle);
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Failed to add device to SPI bus: %s", esp_err_to_name(ret));
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ret;
        }

        // Create FreeRTOS primitives
        m_spi_done_sem = xSemaphoreCreateBinary();
        m_flush_queue = xQueueCreate(m_config.queue_size, sizeof(flush_req_t));

        if (!m_spi_done_sem || !m_flush_queue) {
            log<log_level_t::ERROR>("Failed to create spi_done_sem and flush_queue");
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ESP_ERR_NO_MEM;
        }

        m_dma_semphr = xSemaphoreCreateBinary();
        if (!m_dma_semphr) {
            log<log_level_t::ERROR>("Failed to create dma semphr");
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ESP_ERR_NO_MEM;
        }

        // Give semaphore to indicate the DMA buffer's availability
        if (xSemaphoreGive(m_dma_semphr) != pdTRUE) {
            log<log_level_t::ERROR>("Failed to give dma semphr");
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ESP_ERR_NO_MEM;
        }

        // Create task which handles dma transfers
        BaseType_t rc = xTaskCreatePinnedToCore(pixel_trans_task, "ST7920Task", m_config.task_stack_size, this,
                                                m_config.task_priority, &m_task_handle, m_config.task_core);
        if (rc != pdPASS) {
            log<log_level_t::ERROR>("Failed to create ST7920 task");
            xSemaphoreGive(m_handle_mutex);
            cleanup();
            return ESP_ERR_NO_MEM;
        }
        
        // Send initialization sequence to ST7920
        ret = init_sequence();
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Init sequence failed: %s", esp_err_to_name(ret));
            m_shutdown_requested = true;
            m_task_handle = nullptr;
            xSemaphoreGive(m_handle_mutex); // Release mutex before deletion
            cleanup();
            return ret;
        }

        m_is_initialized = true;
        xSemaphoreGive(m_handle_mutex);

        log<log_level_t::INFO>("Initialization complete");

        return ESP_OK;
    }

    [[nodiscard]] esp_err_t st7920_t::deinit() {

        if (xSemaphoreTake(m_handle_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) return ESP_ERR_TIMEOUT;

        if (!m_is_initialized) {
            log<log_level_t::WARN>("ST7920 already in a deinitialized state");
            return ESP_ERR_INVALID_STATE;
        }

        log<log_level_t::INFO>("Deinitializing ST7920 instance");

        // Get task handle of the currently running task
        m_deinit_task_handle = xTaskGetCurrentTaskHandle();

        // Signal task to shutdown
        m_shutdown_requested = true;

        if (m_flush_queue) {
            flush_req_t dummy{};
            // Unblock task if it was in a blocked state waiting for data from the queue
            xQueueSend(m_flush_queue, &dummy, pdMS_TO_TICKS(TIMEOUT_MS));
        }

        // Give mutex back before blocking and deletion
        xSemaphoreGive(m_handle_mutex);

        if (m_task_handle) {
            // Block till we receive notification from the task to be deleted
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TIMEOUT_MS)); 
        }
        m_deinit_task_handle = nullptr;

        esp_err_t ret = cleanup();
        if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
            log<log_level_t::ERROR>("Failed to properly deinitialize ST7920 instance: %s", esp_err_to_name(ret));
            return ret;
        }

        log<log_level_t::INFO>("Done deinitializing ST7920 instance");

        return ESP_OK;
    }

    [[nodiscard]] esp_err_t st7920_t::flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                                            const uint8_t* pixel_data, size_t pixel_count,
                                            flush_done_cb_t callback, void* user_data) {
        
        if (xSemaphoreTake(m_handle_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            if (callback) callback(user_data, ESP_ERR_TIMEOUT);
            return ESP_ERR_TIMEOUT;
        }
        
        // Checking for invalid arguments
        if (!pixel_data || pixel_count == 0) {
            if (callback) callback(user_data, ESP_ERR_INVALID_ARG);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_INVALID_ARG;
        }

        // Bounds checking
        if (pixel_count > (m_size_of_pixel_buf_bytes / sizeof(m_pixels_buf[0]))) {
            if (callback) callback(user_data, ESP_ERR_INVALID_SIZE);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_INVALID_SIZE;
        }
        if (x1 >= m_config.width || x2 >= m_config.width || x1 > x2) {
            if (callback) callback(user_data, ESP_ERR_INVALID_ARG);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_INVALID_ARG;
        }
        if (y1 >= m_config.height || y2 >= m_config.height || y1 > y2) {
            if (callback) callback(user_data, ESP_ERR_INVALID_ARG);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_INVALID_ARG;
        }

        // Wait till the DMA buffer is available
        if (xSemaphoreTake(m_dma_semphr, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            log<log_level_t::ERROR>("DMA buffer in use for too long. Timing out from flush()");
            if (callback) callback(user_data, ESP_ERR_TIMEOUT);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_TIMEOUT;
        }

        // Copying the pixels data into dma capable memory
        for (size_t i = 0; i < pixel_count; i++) {
            m_pixels_buf[i] = pixel_data[i];
        }

        // Package flush request
        const flush_req_t request = {
            .x1 = x1,
            .y1 = y1,
            .x2 = x2,
            .y2 = y2,
            .pixel_count = pixel_count,
            .callback = callback,
            .user_data = user_data
        };

        // Send to queue
        if (xQueueSend(m_flush_queue, &request, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {

            log<log_level_t::WARN>("Flush queue full");
            xSemaphoreGive(m_dma_semphr);
            if (callback) callback(user_data, ESP_ERR_NO_MEM);

            xSemaphoreGive(m_handle_mutex);

            return ESP_ERR_NO_MEM;
        }

        xSemaphoreGive(m_handle_mutex);
        
        return ESP_OK;
    }

    [[nodiscard]] esp_err_t st7920_t::set_screen(bool color, flush_done_cb_t callback, void* user_data) {

        if (xSemaphoreTake(m_handle_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            log<log_level_t::ERROR>("Unable to take mutex");
            if (callback) callback(user_data, ESP_ERR_TIMEOUT);
            return ESP_ERR_TIMEOUT;
        }

        if (xSemaphoreTake(m_dma_semphr, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            log<log_level_t::ERROR>("DMA buffer in use for too long. Timing out from set_screen()");
            if (callback) callback(user_data, ESP_ERR_TIMEOUT);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_TIMEOUT;
        }
        
        // Set pixel buffer
        memset(m_pixels_buf, color ? 0xFF : 0x00, m_size_of_pixel_buf_bytes);

        // Package flush request
        const flush_req_t request = {
            .x1 = 0,
            .y1 = 0,
            .x2 = static_cast<uint16_t>(m_config.width - 1U),
            .y2 = static_cast<uint16_t>(m_config.height - 1U),
            .pixel_count = m_size_of_pixel_buf_bytes,
            .callback = callback,
            .user_data = user_data
        };

        // Send to queue
        if (xQueueSend(m_flush_queue, &request, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            log<log_level_t::WARN>("Flush queue full");
            xSemaphoreGive(m_dma_semphr);
            if (callback) callback(user_data, ESP_ERR_NO_MEM);
            xSemaphoreGive(m_handle_mutex);
            return ESP_ERR_NO_MEM;
        }

        xSemaphoreGive(m_handle_mutex);

        return ESP_OK;
    }

    // Private methods
    esp_err_t st7920_t::cleanup() {

        if (m_pixels_buf) {
            heap_caps_free(m_pixels_buf);
            m_pixels_buf = nullptr;
        }

        if (m_wire_buf) {
            heap_caps_free(m_wire_buf);
            m_wire_buf = nullptr;
        }

        // Delete FreeRTOS objects
        if (m_spi_done_sem) {
            vSemaphoreDelete(m_spi_done_sem);
            m_spi_done_sem = nullptr;
        }

        if (m_handle_mutex) {
            vSemaphoreDelete(m_handle_mutex);
            m_handle_mutex = nullptr;
        }

        if (m_flush_queue) {
            vQueueDelete(m_flush_queue);
            m_flush_queue = nullptr;
        }

        if (m_dma_semphr) {
            vSemaphoreDelete(m_dma_semphr);
            m_dma_semphr = nullptr;
        }
        
        if (m_spi_handle) {
            esp_err_t ret = spi_bus_remove_device(m_spi_handle);
            if ((ret != ESP_OK) && (ret != ESP_ERR_INVALID_STATE)) {
                log<log_level_t::ERROR>("Failed to remove device from SPI bus: %s", esp_err_to_name(ret));
                return ret;
            }
            m_spi_handle = nullptr;
        }

        return spi_bus_free(m_config.spi_host);
    }

    esp_err_t st7920_t::send_cmd(uint8_t cmd) {

        const uint8_t buf[]{ COMMAND_SYNC_BYTE, static_cast<uint8_t>(cmd & 0xF0U), static_cast<uint8_t>((cmd << 4) & 0xF0U) };

        spi_transaction_t trans{};
        trans.length = sizeof(buf) * 8;
        trans.tx_buffer = buf;
        trans.user = this;

        return spi_device_polling_transmit(m_spi_handle, &trans);
    }
    
    void st7920_t::spi_post_transfer_callback(spi_transaction_t* trans) {
        // Signal completion from ISR
        BaseType_t higher_priority_task_woken{pdFALSE};
        auto driver = static_cast<st7920_t*>(trans->user);
        xSemaphoreGiveFromISR(driver->m_spi_done_sem, &higher_priority_task_woken);
        if (higher_priority_task_woken) portYIELD_FROM_ISR();
    }
    
    esp_err_t st7920_t::init_sequence() {

        // Power ON delay
        vTaskDelay(pdMS_TO_TICKS(50));

        // Function set
        esp_err_t ret = send_cmd(0x30);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        // Needs to be sent twice
        ret = send_cmd(0x30);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        // Display OFF
        ret = send_cmd(0x08);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        // Display clear
        ret = send_cmd(0x01);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(15));

        // Entry mode
        ret = send_cmd(0x06);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        // Display ON, cursor OFF
        ret = send_cmd(0x0C);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        // Switch to extended instruction set
        ret = send_cmd(0x34);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        // Enable graphics mode
        ret = send_cmd(0x36);
        if (ret != ESP_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        return ESP_OK;
    }

    esp_err_t st7920_t::send_pixels_dma(const uint8_t* pixels, size_t count) {
        
        spi_transaction_t trans{};
        trans.flags = 0;
        trans.length = count * 8; // Number of bits
        trans.user = this;
        trans.tx_buffer = pixels;

        // Queue transaction
        esp_err_t ret = spi_device_queue_trans(m_spi_handle, &trans, pdMS_TO_TICKS(TIMEOUT_MS));
        if (ret != ESP_OK) {
            log<log_level_t::ERROR>("Pixel data queue failed: %s", esp_err_to_name(ret));
            return ret;
        }

        // Wait for DMA completion
        if (xSemaphoreTake(m_spi_done_sem, pdMS_TO_TICKS(TIMEOUT_MS)) != pdTRUE) {
            log<log_level_t::ERROR>("DMA timeout on pixel data transaction");
            return ESP_ERR_TIMEOUT;
        }

        return ESP_OK;
    }

    esp_err_t st7920_t::build_wire_buffer(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {

        const uint16_t x_word_start = x1 / 16;
        const uint16_t x_words = (x2 - x1 + 1) / 16;

        uint8_t* wire = m_wire_buf;
        size_t wire_len = 0;

        // Function lambdas to add commands and data to the wire buffer
        auto write_cmd_byte = [&](uint8_t cmd) {
            wire[wire_len++] = COMMAND_SYNC_BYTE;
            wire[wire_len++] = static_cast<uint8_t>(cmd & 0xF0U);
            wire[wire_len++] = static_cast<uint8_t>((cmd << 4) & 0xF0U);
        };

        auto write_data_byte = [&](uint8_t data) {
            wire[wire_len++] = DATA_SYNC_BYTE;
            wire[wire_len++] = static_cast<uint8_t>(data & 0xF0U);
            wire[wire_len++] = static_cast<uint8_t>((data << 4) & 0xF0U);
        };

        for (uint16_t y = y1; y <= y2; y++) {
            // Check if we are to draw into the upper or lower half
            const bool lower_half = (y >= 32);
            const uint8_t y_addr = static_cast<uint8_t>(lower_half ? y - 32 : y);
            const uint8_t x_addr = static_cast<uint8_t>(x_word_start + (lower_half ? 8 : 0));

            write_cmd_byte(ADDRESS_SYNC_BYTE | y_addr);
            write_cmd_byte(ADDRESS_SYNC_BYTE | x_addr);

            const size_t row_byte_offset = (y * 16U) + (x1 / 8U);

            for (uint16_t b = 0; b < x_words * 2; b++) {
                write_data_byte(m_pixels_buf[row_byte_offset + b]);
            }
        }

        m_wire_buf_len = wire_len;

        return ESP_OK;
    }

    void st7920_t::pixel_trans_task(void* arg) {

        log<log_level_t::INFO>("ST7920 task started");

        auto driver = static_cast<st7920_t*>(arg);
        flush_req_t request{};

        while (!driver->m_shutdown_requested) {

            // Wait for flush request from queue
            if (xQueueReceive(driver->m_flush_queue, &request, portMAX_DELAY) != pdTRUE) continue;

            // Check to see if a shutdown has been requested so as not to process dummy data
            if (driver->m_shutdown_requested) break;

            // Mark handle as busy
            if (xSemaphoreTake(driver->m_handle_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdTRUE) {
                driver->m_state = state_t::BUSY;
                xSemaphoreGive(driver->m_handle_mutex);
            } else {
                // Indicate we are done with the DMA buffer
                xSemaphoreGive(driver->m_dma_semphr);
                if (request.callback) request.callback(request.user_data, ESP_ERR_TIMEOUT); // Invoke user callback
                continue;
            }
            
            esp_err_t ret{ESP_OK};

            for (uint8_t i = 1; i <= driver->m_config.max_retries; i++) {

                ret = driver->build_wire_buffer(request.x1, request.y1, request.x2, request.y2);
                if (ret != ESP_OK) {
                    log<log_level_t::WARN>("Attempt #%u: Failed to set pixel window", i);
                    continue;
                }

                ret = driver->send_pixels_dma(driver->m_wire_buf, driver->m_wire_buf_len);
                if (ret == ESP_OK) break;

                log<log_level_t::WARN>("Attempt #%u: Failed to send pixel data", i);
            }

            if (ret != ESP_OK) {
                log<log_level_t::ERROR>("Failed to send pixels");
            }

            // Mark handle as idle
            if (xSemaphoreTake(driver->m_handle_mutex, pdMS_TO_TICKS(TIMEOUT_MS)) == pdTRUE) {
                driver->m_state = state_t::IDLE;
                xSemaphoreGive(driver->m_handle_mutex);
            }

            xSemaphoreGive(driver->m_dma_semphr);
            if (request.callback) request.callback(request.user_data, ret);

            // Clear the wire buffer
            memset(driver->m_wire_buf, 0, driver->m_size_of_pixel_buf_bytes * 4);
            driver->m_wire_buf_len = 0;
        }

        if (driver->m_deinit_task_handle) {
            xTaskNotifyGive(driver->m_deinit_task_handle);
        }

        log<log_level_t::INFO>("ST7920 task shutting down");

        vTaskDelete(nullptr);
    }

} // namespace st7920
