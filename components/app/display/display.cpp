#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "vhorde_logo.hpp"
#include "display.hpp"
#include "colors.hpp"
#include "config.hpp"
#include "utils.hpp"

#include "esp_timer.h"
#include "esp_err.h"

#include <array>


namespace display {

    // Tag
    static constexpr const char TAG[]                       = "Display";
    
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

    // Popup timers
    static esp_timer_handle_t s_modal_close_timer           = nullptr;
    static bool s_is_modal_popup_active                     = false;
    static esp_timer_handle_t s_toast_close_timer           = nullptr;
    static bool s_is_toast_popup_active                     = false;
    static constexpr uint32_t POPUP_TIMEOUT_US              = 2'000'000;
    
    // Bootup screen
    static lv_obj_t* s_bootup_scr                           = nullptr;

    // Forward declarations
    static void disp_flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map);
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

        // Modal popup close timer
        constexpr esp_timer_create_args_t modal_timer_args = {
            .callback = [](void* arg) {
                if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;
                if (s_is_modal_popup_active) {
                    dismiss_modal();
                    s_is_modal_popup_active = false;
                }
                xSemaphoreGive(s_display_mutex);
            },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "s_modal_close_timer",
            .skip_unhandled_events = false
        };
        
        ret = esp_timer_create(&modal_timer_args, &s_modal_close_timer);
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create modal popup close timer: %s", esp_err_to_name(ret));
            return ret;
        }

        // Toast popup close timer
        constexpr esp_timer_create_args_t toast_timer_args = {
            .callback = [](void* arg) {
                if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;
                if (s_is_toast_popup_active) {
                    dismiss_toast();
                    s_is_toast_popup_active = false;
                }
                xSemaphoreGive(s_display_mutex);
            },
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "s_toast_close_timer",
            .skip_unhandled_events = false
        };
        
        ret = esp_timer_create(&toast_timer_args, &s_toast_close_timer);
        if (ret != ESP_OK) {
            utils::log<utils::log_level_t::ERROR>(TAG, "Failed to create toast popup close timer: %s", esp_err_to_name(ret));
            return ret;
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
        
        if (s_modal_close_timer) {
            esp_timer_stop(s_modal_close_timer);
            esp_timer_delete(s_modal_close_timer);
            s_modal_close_timer = nullptr;
        }
        
        if (s_toast_close_timer) {
            esp_timer_stop(s_toast_close_timer);
            esp_timer_delete(s_toast_close_timer);
            s_toast_close_timer = nullptr;
        }

        if (s_bootup_scr) {
            lv_obj_del(s_bootup_scr);
            s_bootup_scr = nullptr;
        }

        if (s_display) {
            lv_display_delete(s_display);
            s_display = nullptr;
        }

        xSemaphoreGive(s_display_mutex);
        
        vSemaphoreDelete(s_display_mutex);
        s_display_mutex = nullptr;
        
        utils::log<utils::log_level_t::INFO>(TAG, "Display interface deinitialized");

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
        
        xSemaphoreGive(s_display_mutex);

        create_animated_loading_bar(s_bootup_scr, 180, 35, DISP_BOOTUP_SCREEN_TIME_MS);

        utils::log<utils::log_level_t::INFO>(TAG, "Done loading bootup screen");
    }
   
    void create_ui() {

        if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;

        utils::log<utils::log_level_t::INFO>(TAG, "Creating UI");

        // First screen
        create_main_menu_screen();

        // Second screen
        constexpr wifi_data_t wifi_data = {
            .is_on = false,
            .ssid = config::WIFI_SSID_NAME,
            .ip = config::IP_ADDRESS // Stable. Can be hard coded
        };
        create_wifi_screen(wifi_data);

        // Third screen
        create_scale_screen();

        // Fourth screen
        create_speed_screen();

        // Fifth screen
        constexpr about_data_t about_data = {
            .firmware_version = "v1.0.0", // What am I even doing
            .steps_per_mm = 0.0f,
            .effective_feed_rate = 0,
            .scale = 1.0f,
            .speed = 1.0f,
            .file_size_bytes = config::MAX_FILE_SIZE_BYTES,
            .fs_used_bytes = 0,
            .fs_total_bytes = 0,
            .ssid = config::WIFI_SSID_NAME
        };
        create_about_screen(about_data);

        // Sixth screen
        constexpr motion_data_t motion_data = {
            .state = gcode::types::state_t::SLEEPING,
            .progress_pct = 0,
            .current_line = 0,
            .total_lines = 0,
            .scale = 1.0f,
            .speed = 1.0f
        };
        create_motion_screen(motion_data);
        
        // Cleanup bootup screen resources
        // The children get auto deleted when
        // the parent screen is deleted
        if (s_bootup_scr) {
            lv_obj_del(s_bootup_scr);
            s_bootup_scr = nullptr;
        }

        load_screen(screen_t::MAIN_MENU);

        xSemaphoreGive(s_display_mutex);

        utils::log<utils::log_level_t::INFO>(TAG, "UI created");
    }

    void send_event(const event_t& event) {

        if (xSemaphoreTake(s_display_mutex, pdMS_TO_TICKS(config::TIMEOUT_MS)) != pdTRUE) return;

        bool is_event_toast_popup = true;

        switch (event.event) {
            case event_type_t::PLOTTING_PAUSED:
                show_toast_plotting_paused();
                break;

            case event_type_t::PLOTTING_RESUMED:
                show_toast_plotting_resumed(event.from_line);
                break;

            case event_type_t::PLOTTING_STOPPED:
                show_modal_session_stopped(event.completed_lines, event.total_lines, event.on_ok.value_or(nullptr));
                is_event_toast_popup = false;
                break;

            case event_type_t::PLOTTING_COMPLETE:
                show_toast_plotting_complete();
                break;

            case event_type_t::FILE_NOT_FOUND:
                show_modal_file_not_found(event.on_ok.value_or(nullptr));
                is_event_toast_popup = false;
                break;

            case event_type_t::PARSER_ERROR:
                show_modal_parse_error(event.line_num, event.line_str, event.on_ok.value_or(nullptr));
                is_event_toast_popup = false;
                break;

            case event_type_t::FILE_READ_ERROR:
                show_modal_file_read_error(event.on_ok.value_or(nullptr));
                is_event_toast_popup = false;
                break;

            case event_type_t::WIFI_ENABLED:
                show_toast_wifi_enabled();
                break;

            case event_type_t::WIFI_DISABLED:
                show_toast_wifi_disabled();
                break;

            case event_type_t::FILE_RECEIVED:
                show_toast_file_received();
                break;

            case event_type_t::CLEAR_ALL_POPUPS:
                if (s_is_toast_popup_active) {
                    dismiss_toast();
                    esp_timer_stop(s_toast_close_timer);
                    s_is_toast_popup_active = false;
                }
                if (s_is_modal_popup_active) {
                    dismiss_modal();
                    esp_timer_stop(s_modal_close_timer);
                    s_is_modal_popup_active = false;
                }
                xSemaphoreGive(s_display_mutex);
                return;

            default:
                utils::log<utils::log_level_t::WARN>(TAG, "Invalid event");
                xSemaphoreGive(s_display_mutex);
                return;
        }

        if (is_event_toast_popup) {
            esp_timer_stop(s_toast_close_timer);
            esp_timer_start_once(s_toast_close_timer, POPUP_TIMEOUT_US);
            s_is_toast_popup_active = true;

        } else {
            esp_timer_stop(s_modal_close_timer);
            esp_timer_start_once(s_modal_close_timer, POPUP_TIMEOUT_US);
            s_is_modal_popup_active = true;
        }

        xSemaphoreGive(s_display_mutex);
    }

    // Helper functions
    static void disp_flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {

        const uint16_t width = area->x2 - area->x1 + 1;
        const uint16_t height = area->y2 - area->y1 + 1;
        const size_t pixel_count = width * height;

        const auto px_data = reinterpret_cast<uint16_t*>(px_map);

        esp_err_t ret = ili9341_flush(area->x1, area->y1, area->x2, area->y2, px_data, pixel_count, 
            [](void* user_data, esp_err_t ret) {
                auto display = static_cast<lv_display_t*>(user_data);
                lv_display_flush_ready(display);

                if (ret != ESP_OK) {
                    utils::log<utils::log_level_t::WARN>(TAG, "Flush completed with error: %s", esp_err_to_name(ret));
                }
            },
        display, s_display_handle);
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

        lv_screen_load(parent);

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
