#ifndef SCREENS_HPP_
#define SCREENS_HPP_


#include "lvgl.h"

#include "controller.hpp"

#include <cstdint>
#include <cstddef>


namespace display {

    // Data passed into screens
    struct motion_data_t {
        gcode::types::state_t state{gcode::types::state_t::SLEEPING};
        uint8_t  progress_pct{0};   // 0–100
        size_t   current_line{0};
        size_t   total_lines{0};
        float    scale{1.0f};
        float    speed{1.0f};
    };

    struct about_data_t {
        const char* firmware_version{"v1.0.0"};
        float       steps_per_mm{0.0f};
        uint32_t    effective_feed_rate{0};   // default feed rate * speed multiplier
        float       scale{1.0f};
        float       speed{1.0f};
        size_t      file_size_bytes{0};
        size_t      fs_used_bytes{0};
        size_t      fs_total_bytes{0};
        const char* ssid{""};
    };

    struct wifi_data_t {
        bool        is_on{false};
        const char* ssid{""};
        const char* ip{""};
    };

    // Screen IDs
    enum class screen_t : uint8_t {
        MAIN_MENU = 0,
        WIFI,
        SCALE,
        SPEED,
        ABOUT,
        MOTION,
        COUNT
    };

    // Screen creation
    // Logs a warning and no ops if the screen already exists.
    void create_main_menu_screen(uint8_t selected_item = 0);
    void create_wifi_screen(const wifi_data_t& data);
    void create_scale_screen(float current_scale);
    void create_speed_screen(float current_speed);
    void create_about_screen(const about_data_t& data);
    void create_motion_screen(const motion_data_t& data);

    // Screen updates
    // Logs a warning and no ops if the screen does not exist.
    void update_main_menu_selection(uint8_t item);
    void update_wifi_screen(const wifi_data_t& data);
    void update_scale_screen(float scale);
    void update_speed_screen(float speed);
    void update_about_screen(const about_data_t& data);
    void update_motion_screen(const motion_data_t& data);

    // Navigation
    // Logs a warning and no ops if the screen does not exist.
    void load_screen(screen_t screen);

    // Toasts
    // Parented to lv_scr_act() at call time. A new toast replaces any existing
    // one. No auto-dismiss — caller drives the timer and calls dismiss_toast().
    void show_toast_plotting_paused();
    void show_toast_plotting_resumed(size_t from_line);
    void show_toast_plotting_complete();
    void show_toast_wifi_enabled(const char* ssid, const char* ip);
    void show_toast_wifi_disabled();
    void show_toast_file_received();

    void dismiss_toast();   // No op if no toast active

    // Modals
    // Parented to lv_scr_act() at call time with a 70% dim overlay behind them.
    // on_ok is called after the modal is dismissed. May be nullptr.
    // Input blocking while modal is active is the caller's responsibility.
    void show_modal_parse_error(size_t line_num, const char* line_str,
                                 void (*on_ok)() = nullptr);
    void show_modal_file_not_found(void (*on_ok)() = nullptr);
    void show_modal_file_read_error(void (*on_ok)() = nullptr);
    void show_modal_session_stopped(size_t completed_lines, size_t total_lines,
                                     void (*on_ok)() = nullptr);

    void dismiss_modal();   // No op if no modal active

} // namespace display


#endif // SCREENS_HPP_
