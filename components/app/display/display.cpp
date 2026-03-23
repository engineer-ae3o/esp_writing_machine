#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "vhorde_logo.hpp"
#include "display.hpp"
#include "screens.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "utils.hpp"

#include "esp_timer.h"
#include "esp_err.h"

#include <array>


namespace display {

    // Tag
    static constexpr const char TAG[] = "Display";

    // Screens
    static uint8_t s_current_screen_idx                     = 0;
    
    // Timeout
    static constexpr uint32_t POPUP_TIMEOUT_US              = 2'000'000;
    
    // Display buffer size for LVGL (40 lines worth of pixels)
    static constexpr size_t DISP_BUF_SIZE                   = config::LCD_WIDTH * 40;
    static constexpr uint16_t DISP_BOOTUP_SCREEN_TIME_MS    = 2'500;
    
    // LVGL buffers
    static std::array<lv_color16_t, DISP_BUF_SIZE> s_buf1{};
    static std::array<lv_color16_t, DISP_BUF_SIZE> s_buf2{};

    // General utilities
    static lv_display_t* s_display                          = nullptr;
    static esp_timer_handle_t s_lvgl_tick_timer             = nullptr;
    static ili9341_handle_t s_display_handle                = nullptr;
    static SemaphoreHandle_t s_display_mutex                = nullptr;
    static TimerHandle_t s_lvgl_timer_handler               = nullptr;
    
    // Bootup screen
    static lv_obj_t* s_bootup_scr                             = nullptr;

    // Forward declarations
    static void disp_flush_cb(lv_display_t* s_display, const lv_area_t* area, uint8_t* px_map);
    static void create_animated_loading_bar(lv_obj_t* parent, uint8_t w, uint8_t h, uint16_t time_ms);

    // Public functions
    esp_err_t init(const ili9341_handle_t& handle, SemaphoreHandle_t& disp_mutex) {

        utils::log<utils::log_level_t::INFO>(TAG, "Initializing s_display interface");

        s_display_handle = handle;

        s_display_mutex = xSemaphoreCreateMutex();
        if (!s_display_mutex) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create lvgl mutex");
            return ESP_ERR_NO_MEM;
        }

        disp_mutex = s_display_mutex;

        // Initialize LVGL
        lv_init();

        // Create the s_display
        s_display = lv_display_create(config::LCD_WIDTH, config::LCD_HEIGHT);
        lv_display_set_buffers(s_display, s_buf1.data(), s_buf2.data(), sizeof(s_buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_flush_cb(s_display, disp_flush_cb);
        
        // LVGL tick timer
        constexpr esp_timer_create_args_t timer_args = {
            .callback = [](void* arg) { lv_tick_inc(1); },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_ISR,
            .name = "lvgl_tick",
            .skip_unhandled_events = false
        };
        
        auto ret = esp_timer_create(&timer_args, &s_lvgl_tick_timer);
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create LVGL tick timer: %s", esp_err_to_name(ret));
            return ret;
        }
        
        ret = esp_timer_start_periodic(s_lvgl_tick_timer, 1000); // 1ms
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to start LVGL tick timer: %s", esp_err_to_name(ret));
            deinit();
            return ret;
        }

        // LVGL timer handler timer
        s_lvgl_timer_handler = xTimerCreate("LVGL Timer Handler", pdMS_TO_TICKS(config::LVGL_TIMER_PERIOD_MS),
                                            pdTRUE, nullptr,
                                            [](TimerHandle_t xTimer) {
                                                if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;
                                                lv_timer_handler();
                                                xSemaphoreGive(s_display_mutex);
                                            });
        if (!s_lvgl_timer_handler) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create lvgl timer handler");
            deinit();
            return ESP_ERR_NO_MEM;
        }

        auto rc = xTimerStart(s_lvgl_timer_handler, 0);
        if (rc != pdPASS) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to start lvgl timer handler");
            deinit();
            return ESP_FAIL;
        }
        
        utils::log<utils::log_level_t::INFO>(TAG, "Display interface initialized successfully");

        return ESP_OK;
    }

    esp_err_t deinit() {

        if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to take lvgl s_display mutex");
            return ESP_ERR_TIMEOUT;
        }

        utils::log<utils::log_level_t::INFO>(TAG, "Deinitializing the display interface");

        if (s_lvgl_tick_timer) {
            esp_timer_stop(s_lvgl_tick_timer);
            esp_timer_delete(s_lvgl_tick_timer);
            s_lvgl_tick_timer = nullptr;
        }

        if (s_lvgl_timer_handler) {
            xTimerStop(s_lvgl_timer_handler, pdMS_TO_TICKS(config::TIMEOUT_MS));
            xTimerDelete(s_lvgl_timer_handler, pdMS_TO_TICKS(config::TIMEOUT_MS));
            s_lvgl_timer_handler = nullptr;
        }

        if (s_bootup_scr) {
            lv_obj_del(s_bootup_scr);
            s_bootup_scr = nullptr;
        }

        for (auto& screen : screens) {
            if (screen) {
                lv_obj_delete(screen);
                screen = nullptr;
            }
        }

        if (s_display) {
            lv_display_delete(s_display);
            s_display = nullptr;
        }
        
        utils::log<utils::log_level_t::INFO>(TAG, "Display interface deinitialized");

        xSemaphoreGive(s_display_mutex);

        return ESP_OK;
    }

    void bootup_screen() {

        if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;

        utils::log<utils::log_level_t::INFO>(TAG, "Loading bootup screen");

        s_bootup_scr = lv_obj_create(nullptr);
        lv_obj_set_style_bg_color(s_bootup_scr, lv_color_hex(color::BLACK), 0);

        lv_obj_t* image = lv_image_create(s_bootup_scr);
        lv_image_set_src(image, &vhorde_logo);
        lv_obj_align(image, LV_ALIGN_TOP_MID, 0, 20);

        lv_screen_load(s_bootup_scr);

        xSemaphoreGive(s_display_mutex);

        create_animated_loading_bar(s_bootup_scr, 180, 35, DISP_BOOTUP_SCREEN_TIME_MS);

        utils::log<utils::log_level_t::INFO>(TAG, "Done loading bootup screen");
    }
    
    void create_ui() {

        if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;

        utils::log<utils::log_level_t::INFO>(TAG, "Creating UI");

        create_screen_0();
        create_screen_1();
        create_screen_2();
        create_screen_3();
        create_screen_4();
        create_screen_5();
        create_screen_6();
        create_screen_7();

        lv_scr_load(screens[0]);

        // Cleanup bootup screen resources
        // The children get auto deleted when
        // the parent screen is deleted
        if (s_bootup_scr) {
            lv_obj_del(s_bootup_scr);
            s_bootup_scr = nullptr;
        }

        xSemaphoreGive(s_display_mutex);

        utils::log<utils::log_level_t::INFO>(TAG, "UI created");
    }


    // Helper functions
    static void disp_flush_cb(lv_display_t* s_display, const lv_area_t* area, uint8_t* px_map) {

        const uint16_t width = area->x2 - area->x1 + 1;
        const uint16_t height = area->y2 - area->y1 + 1;
        const size_t pixel_count = width * height;

        const auto px_data = reinterpret_cast<uint16_t*>(px_map);

        esp_err_t ret = ili9341_flush(area->x1, area->y1, area->x2, area->y2, px_data, pixel_count, 
            [](void* user_data, esp_err_t ret) {
                auto display = static_cast<lv_display_t*>(user_data);
                lv_disp_flush_ready(display);

                if (ret != ESP_OK) {
                    utils::log<utils::log_level_t::WARN>(TAG, "Flush completed with error: %s", esp_err_to_name(ret));
                }
            },
        s_display, s_display_handle);
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Flush failed: %s", esp_err_to_name(ret));
        }
    }

    static void create_animated_loading_bar(lv_obj_t* parent, uint8_t w, uint8_t h, uint16_t time_ms) {

        if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;

        lv_obj_t* loading_bar = lv_bar_create(parent);

        lv_obj_set_size(loading_bar, w, h);
        lv_bar_set_range(loading_bar, 0, 100);
        lv_obj_align(loading_bar, LV_ALIGN_TOP_MID, 0, 270);
        lv_bar_set_value(loading_bar, 0, LV_ANIM_ON);

        lv_scr_load(parent);

        lv_anim_t bar_anim{};

        lv_anim_init(&bar_anim);
        lv_anim_set_var(&bar_anim, loading_bar);
        lv_anim_set_duration(&bar_anim, time_ms);
        lv_anim_set_values(&bar_anim, 0, 100);
        lv_anim_set_repeat_count(&bar_anim, 0);
        lv_anim_set_exec_cb(&bar_anim, 
            [](void* user_data, int32_t value) {
                lv_bar_set_value(static_cast<lv_obj_t*>(user_data), value, LV_ANIM_ON);
            }
        );
        lv_anim_set_path_cb(&bar_anim, lv_anim_path_ease_in);

        lv_anim_start(&bar_anim);

        xSemaphoreGive(s_display_mutex);

        vTaskDelay(pdMS_TO_TICKS(time_ms));
    }

} // namespace s_display
