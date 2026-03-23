#include "screens.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "colors.hpp"

#include <cstdio>
#include <cstring>


namespace display {

    static constexpr const char* TAG = "UI";

    // Layout constants
    static constexpr int32_t WIDTH       = config::LCD_WIDTH;
    static constexpr int32_t HEIGHT      = config::LCD_HEIGHT;
    static constexpr int32_t MX          = 20;          // horizontal margin
    static constexpr int32_t CW          = WIDTH - 2 * MX;  // content width: 280
    static constexpr int32_t TOP_BAR_H   = 36;
    static constexpr int32_t CONTENT_Y   = TOP_BAR_H + 1; // after separator
    static constexpr int32_t MODAL_HDR_H = 38;
    static constexpr int32_t TOAST_H     = 40;
    static constexpr int32_t TOAST_Y     = HEIGHT - TOAST_H - 8;  // 192

    static constexpr float SCALE_MIN = 0.25f;
    static constexpr float SCALE_MAX = 4.00f;
    static constexpr float SPEED_MIN = 0.25f;
    static constexpr float SPEED_MAX = 3.00f;
    static constexpr uint32_t DEFAULT_FEED_RATE = gcode::config::DEFAULT_FEED_RATE;


    // Colour palette
    // Backgrounds / surfaces
    static const lv_color_t C_BG        = lv_color_hex(color::UI_BG);          // TODO
    static const lv_color_t C_TOPBAR    = lv_color_hex(color::UI_TOPBAR);       // TODO
    static const lv_color_t C_SURFACE   = lv_color_hex(color::UI_SURFACE);      // TODO
    static const lv_color_t C_SURFACE2  = lv_color_hex(color::UI_SURFACE_2);    // TODO
    static const lv_color_t C_SURFACE3  = lv_color_hex(color::UI_SURFACE_3);    // TODO

    // Borders / separators
    static const lv_color_t C_BORDER    = lv_color_hex(color::UI_BORDER);       // TODO
    static const lv_color_t C_SEP       = lv_color_hex(color::UI_SEPARATOR);    // TODO

    // Accent (cyan)
    static const lv_color_t C_ACCENT    = lv_color_hex(color::UI_ACCENT);       // TODO
    static const lv_color_t C_ACCENT_BG = lv_color_hex(color::UI_ACCENT_BG);    // TODO

    // Green (RUNNING / success)
    static const lv_color_t C_GREEN     = lv_color_hex(color::UI_GREEN);        // TODO
    static const lv_color_t C_GREEN_BG  = lv_color_hex(color::UI_GREEN_BG);     // TODO
    static const lv_color_t C_GREEN_DK  = lv_color_hex(color::UI_GREEN_DARK);   // TODO
    static const lv_color_t C_GREEN_TXT_DK = lv_color_hex(color::UI_GREEN_TEXT_DARK); // TODO

    // Amber (PAUSED / warning)
    static const lv_color_t C_YELLOW        = lv_color_hex(color::UI_AMBER);           // TODO
    static const lv_color_t C_YELLOW_BG     = lv_color_hex(color::UI_AMBER_BG);        // TODO
    static const lv_color_t C_YELLOW_TXT_DK = lv_color_hex(color::UI_AMBER_TEXT_DARK); // TODO
    static const lv_color_t C_MUTED_YEL     = lv_color_hex(color::UI_AMBER_HEADER);    // TODO

    // Red (errors / stop)
    static const lv_color_t C_RED       = lv_color_hex(color::UI_RED);          // TODO
    static const lv_color_t C_RED_BG    = lv_color_hex(color::UI_RED_BG);       // TODO
    static const lv_color_t C_RED_HDR   = lv_color_hex(color::UI_RED_HEADER);   // TODO
    static const lv_color_t C_RED_TXT   = lv_color_hex(color::UI_RED_TEXT);     // TODO

    // Orange (speed)
    static const lv_color_t C_ORANGE    = lv_color_hex(color::UI_ORANGE);       // TODO
    static const lv_color_t C_ORANGE_BG = lv_color_hex(color::UI_ORANGE_BG);    // TODO
    static const lv_color_t C_ORANGE_DK = lv_color_hex(color::UI_ORANGE_DARK);  // TODO

    // Blue (pause button / wifi)
    static const lv_color_t C_BLUE      = lv_color_hex(color::UI_BLUE);         // TODO
    static const lv_color_t C_BLUE_BG   = lv_color_hex(color::UI_BLUE_BG);      // TODO
    static const lv_color_t C_BLUE_DK   = lv_color_hex(color::UI_BLUE_DARK);    // TODO

    // Text / greyscale
    static const lv_color_t C_TEXT      = lv_color_hex(color::UI_TEXT);         // TODO
    static const lv_color_t C_SUBTEXT   = lv_color_hex(color::GREY);            // exists: 0x7F7F7F
    static const lv_color_t C_DIM       = lv_color_hex(color::UI_DIM);          // TODO
    static const lv_color_t C_DIMMER    = lv_color_hex(color::DARK_GREY);       // exists: 0x404040
    static const lv_color_t C_FAINT     = lv_color_hex(color::UI_FAINT);        // TODO
    static const lv_color_t C_GHOST     = lv_color_hex(color::UI_GHOST);        // TODO
    

    // Font aliases
    static const auto F8  = &lv_font_montserrat_8;
    static const auto F10 = &lv_font_montserrat_10;
    static const auto F12 = &lv_font_montserrat_12;
    static const auto F14 = &lv_font_montserrat_14;
    static const auto F32 = &lv_font_montserrat_32;


    // State
    static constexpr uint8_t SCREEN_COUNT = static_cast<uint8_t>(screen_t::COUNT);
    static lv_obj_t* s_screens[SCREEN_COUNT]{};

    // Main menu
    static lv_obj_t* s_menu_items[5]{};
    static uint8_t   s_menu_selected{0};

    // WiFi screen
    static lv_obj_t* s_wifi_status_box{};
    static lv_obj_t* s_wifi_status_label{};
    static lv_obj_t* s_wifi_ssid_label{};
    static lv_obj_t* s_wifi_ip_label{};
    static lv_obj_t* s_wifi_hint_label{};

    // Scale screen
    static lv_obj_t* s_scale_value_label{};
    static lv_obj_t* s_scale_bar{};

    // Speed screen
    static lv_obj_t* s_speed_value_label{};
    static lv_obj_t* s_speed_computed_label{};
    static lv_obj_t* s_speed_bar{};

    // About screen
    static lv_obj_t* s_about_feed_rate_val{};
    static lv_obj_t* s_about_scale_val{};
    static lv_obj_t* s_about_speed_val{};
    static lv_obj_t* s_about_file_size_val{};
    static lv_obj_t* s_about_fs_val{};

    // Motion screen
    static lv_obj_t* s_motion_badge_box{};
    static lv_obj_t* s_motion_badge_label{};
    static lv_obj_t* s_motion_progress_bar{};
    static lv_obj_t* s_motion_pct_label{};
    static lv_obj_t* s_motion_line_label{};
    static lv_obj_t* s_motion_params_label{};
    static lv_obj_t* s_motion_ok_btn_box{};
    static lv_obj_t* s_motion_ok_btn_label{};

    // Popups
    static lv_obj_t*  s_toast{};
    static lv_obj_t*  s_modal_overlay{};
    static void     (*s_modal_ok_cb)(){};


    // Static helpers
    static lv_obj_t*& screen_slot(screen_t s) {
        return s_screens[static_cast<uint8_t>(s)];
    }

    static lv_obj_t* make_obj(lv_obj_t* parent,
                                int32_t x, int32_t y, int32_t w, int32_t h,
                                int32_t radius,
                                lv_color_t bg, lv_opa_t bg_opa,
                                lv_color_t border_col, int32_t border_w) {
        lv_obj_t* o = lv_obj_create(parent);
        lv_obj_remove_style_all(o);
        lv_obj_set_pos(o, x, y);
        lv_obj_set_size(o, w, h);
        lv_obj_set_style_radius(o, radius, 0);
        lv_obj_set_style_bg_color(o, bg, 0);
        lv_obj_set_style_bg_opa(o, bg_opa, 0);
        lv_obj_set_style_border_color(o, border_col, 0);
        lv_obj_set_style_border_width(o, border_w, 0);
        lv_obj_set_style_border_opa(o, border_w > 0 ? LV_OPA_COVER : LV_OPA_0, 0);
        lv_obj_set_style_pad_all(o, 0, 0);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        return o;
    }

    static lv_obj_t* make_label(lv_obj_t* parent, const char* text,
                                  lv_color_t color, const lv_font_t* font) {
        lv_obj_t* l = lv_label_create(parent);
        lv_obj_remove_style_all(l);
        lv_label_set_text(l, text);
        lv_obj_set_style_text_color(l, color, 0);
        lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_bg_opa(l, LV_OPA_TRANSP, 0);
        return l;
    }

    static lv_obj_t* make_bar(lv_obj_t* parent,
                                int32_t x, int32_t y, int32_t w, int32_t h,
                                lv_color_t indicator_color,
                                int32_t value, int32_t max_value) {
        lv_obj_t* b = lv_bar_create(parent);
        lv_obj_remove_style_all(b);
        lv_obj_set_pos(b, x, y);
        lv_obj_set_size(b, w, h);
        lv_bar_set_range(b, 0, max_value);
        lv_bar_set_value(b, value, LV_ANIM_OFF);

        lv_obj_set_style_radius(b, h / 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, C_SURFACE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_color(b, C_BORDER, LV_PART_MAIN);
        lv_obj_set_style_border_width(b, 1, LV_PART_MAIN);
        lv_obj_set_style_border_opa(b, LV_OPA_COVER, LV_PART_MAIN);

        lv_obj_set_style_radius(b, h / 2, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(b, indicator_color, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_INDICATOR);

        return b;
    }

    // Common compound widgets 

    // Full-size screen base
    static lv_obj_t* make_screen_base() {
        lv_obj_t* scr = lv_obj_create(nullptr);
        lv_obj_remove_style_all(scr);
        lv_obj_set_size(scr, WIDTH, HEIGHT);
        lv_obj_set_style_bg_color(scr, C_BG, 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(scr, 0, 0);
        lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
        return scr;
    }

    // Top bar + centred title + 1px separator line
    static void make_top_bar(lv_obj_t* scr, const char* title) {
        lv_obj_t* bar = make_obj(scr, 0, 0, WIDTH, TOP_BAR_H, 0,
                                  C_TOPBAR, LV_OPA_COVER, C_TOPBAR, 0);
        lv_obj_t* lbl = make_label(bar, title, C_TEXT, F12);
        lv_obj_set_style_text_letter_space(lbl, 3, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        // 1px separator
        make_obj(scr, 0, TOP_BAR_H, WIDTH, 1, 0, C_BORDER, LV_OPA_COVER, C_BORDER, 0);
    }

    // 1px separator within content area
    static void make_sep(lv_obj_t* parent, int32_t y) {
        make_obj(parent, MX, y, CW, 1, 0, C_SEP, LV_OPA_COVER, C_SEP, 0);
    }

    // Key/value row used in About. Returns the value label.
    static lv_obj_t* make_kv_row(lv_obj_t* scr, int32_t y,
                                   const char* key, const char* value) {
        lv_obj_t* key_lbl = make_label(scr, key, C_DIM, F10);
        lv_obj_set_pos(key_lbl, MX + 10, y);

        lv_obj_t* val_lbl = make_label(scr, value, C_TEXT, F12);
        lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_size(val_lbl, CW - 20, LV_SIZE_CONTENT);
        lv_obj_set_pos(val_lbl, MX + 10, y);

        return val_lbl;
    }

    // Two-button row at the bottom of the motion screen.
    // Returns the left (OK) button's main label for later text/colour updates.
    static lv_obj_t* make_button_row(lv_obj_t* scr, int32_t y,
                                       // Left (OK) button
                                       lv_color_t l_bg, lv_color_t l_border,
                                       const char* l_hint, const char* l_text, lv_color_t l_color,
                                       lv_obj_t** out_l_box,
                                       // Right (BACK) button
                                       lv_color_t r_bg, lv_color_t r_border,
                                       const char* r_hint, const char* r_text, lv_color_t r_color) {
        static constexpr int32_t BTN_W = 124;
        static constexpr int32_t BTN_H = 42;
        static constexpr int32_t GAP   = 8;

        // Left button
        lv_obj_t* l_box = make_obj(scr, MX, y, BTN_W, BTN_H, 5,
                                    l_bg, LV_OPA_COVER, l_border, 2);
        if (out_l_box) *out_l_box = l_box;

        lv_obj_t* l_hint_lbl = make_label(l_box, l_hint, C_DIMMER, F8);
        lv_obj_set_style_text_align(l_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(l_hint_lbl, BTN_W, LV_SIZE_CONTENT);
        lv_obj_align(l_hint_lbl, LV_ALIGN_TOP_MID, 0, 6);

        lv_obj_t* l_main_lbl = make_label(l_box, l_text, l_color, F14);
        lv_obj_set_style_text_align(l_main_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(l_main_lbl, BTN_W, LV_SIZE_CONTENT);
        lv_obj_align(l_main_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);

        // Right button
        int32_t r_x = MX + BTN_W + GAP;
        lv_obj_t* r_box = make_obj(scr, r_x, y, BTN_W, BTN_H, 5,
                                    r_bg, LV_OPA_COVER, r_border, 2);

        lv_obj_t* r_hint_lbl = make_label(r_box, r_hint, C_DIMMER, F8);
        lv_obj_set_style_text_align(r_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(r_hint_lbl, BTN_W, LV_SIZE_CONTENT);
        lv_obj_align(r_hint_lbl, LV_ALIGN_TOP_MID, 0, 6);

        lv_obj_t* r_main_lbl = make_label(r_box, r_text, r_color, F14);
        lv_obj_set_style_text_align(r_main_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(r_main_lbl, BTN_W, LV_SIZE_CONTENT);
        lv_obj_align(r_main_lbl, LV_ALIGN_BOTTOM_MID, 0, -6);

        return l_main_lbl;
    }

    // Badge pill used on the motion screen. Returns container + label.
    static void make_state_badge(lv_obj_t* scr, int32_t y,
                                   lv_color_t bg, lv_color_t border,
                                   const char* text, lv_color_t text_color,
                                   lv_obj_t** out_box, lv_obj_t** out_label) {
        static constexpr int32_t BADGE_W = 104;
        static constexpr int32_t BADGE_H = 24;
        int32_t badge_x = (WIDTH - BADGE_W) / 2;

        lv_obj_t* box = make_obj(scr, badge_x, y, BADGE_W, BADGE_H, 12,
                                  bg, LV_OPA_COVER, border, 2);
        lv_obj_t* lbl = make_label(box, text, text_color, F10);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_size(lbl, BADGE_W, LV_SIZE_CONTENT);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        if (out_box)   *out_box   = box;
        if (out_label) *out_label = lbl;
    }

    // Big value box (Scale / Speed screens) — returns the value label.
    static lv_obj_t* make_value_box(lv_obj_t* scr, int32_t y,
                                      lv_color_t color,
                                      const char* initial_text) {
        static constexpr int32_t BOX_W = 160;
        static constexpr int32_t BOX_H = 76;
        int32_t box_x = (WIDTH - BOX_W) / 2;

        make_obj(scr, box_x, y, BOX_W, BOX_H, 8,
                 C_SURFACE3, LV_OPA_COVER, C_BORDER, 1);

        lv_obj_t* lbl = make_label(scr, initial_text, color, F32);
        lv_obj_align(lbl, LV_ALIGN_TOP_MID, -10, y + 16);

        // Multiplier symbol alongside the number
        lv_obj_t* sym = make_label(scr, "\xc3\x97", color, F14);  // UTF-8 × (U+00D7)
        lv_obj_align_to(sym, lbl, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, 0);

        return lbl;
    }

    // Helper: float → bar integer (0–100)
    static int32_t range_to_bar(float val, float min_v, float max_v) {
        float norm = (val - min_v) / (max_v - min_v);
        if (norm < 0.0f) norm = 0.0f;
        if (norm > 1.0f) norm = 1.0f;
        return static_cast<int32_t>(norm * 100.0f + 0.5f);
    }

    // Modal helpers
    static void modal_ok_handler(lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        void (*cb)() = s_modal_ok_cb;
        dismiss_modal();
        if (cb) cb();
    }

    // Creates overlay + box. Returns box. Adds header + separator.
    static lv_obj_t* make_modal_base(int32_t box_y, int32_t box_h,
                                       lv_color_t header_bg,
                                       lv_color_t box_border,
                                       lv_color_t header_sep,
                                       const char* header_text,
                                       lv_color_t header_text_color,
                                       void (*on_ok)()) {
        dismiss_modal();

        // Dim overlay parented to the currently active screen
        lv_obj_t* overlay = lv_obj_create(lv_scr_act());
        lv_obj_remove_style_all(overlay);
        lv_obj_set_size(overlay, WIDTH, HEIGHT);
        lv_obj_set_pos(overlay, 0, 0);
        lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
        lv_obj_set_style_pad_all(overlay, 0, 0);
        lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

        s_modal_overlay = overlay;
        s_modal_ok_cb   = on_ok;

        // Modal box (child of overlay so it sits above the dim layer)
        lv_obj_t* box = make_obj(overlay, MX, box_y, CW, box_h, 8,
                                  lv_color_hex(color::EERIE_BLACK), LV_OPA_COVER, box_border, 2);

        // Header background (fill top portion; corners already rounded by box)
        lv_obj_t* hdr_bg = make_obj(box, 0, 0, CW, MODAL_HDR_H, 8,
                                     header_bg, LV_OPA_COVER, header_bg, 0);
        // Square off the bottom half of the header bg to avoid a gap
        make_obj(box, 0, MODAL_HDR_H / 2, CW, MODAL_HDR_H / 2, 0,
                 header_bg, LV_OPA_COVER, header_bg, 0);

        lv_obj_t* h_lbl = make_label(hdr_bg, header_text, header_text_color, F12);
        lv_obj_align(h_lbl, LV_ALIGN_LEFT_MID, 14, 0);

        // Header–body separator
        make_obj(box, 0, MODAL_HDR_H, CW, 1, 0, header_sep, LV_OPA_COVER, header_sep, 0);

        return box;
    }

    // Adds body text labels inside the box (below header).
    // Each call adds one label, offset from the top of the body area.
    static lv_obj_t* add_modal_label(lv_obj_t* box, int32_t y_body_offset,
                                       const char* text, lv_color_t color,
                                       const lv_font_t* font) {
        lv_obj_t* lbl = make_label(box, text, color, font);
        lv_obj_set_size(lbl, CW - 28, LV_SIZE_CONTENT);
        lv_obj_set_pos(lbl, 14, MODAL_HDR_H + 8 + y_body_offset);
        return lbl;
    }

    // Adds a centred OK button inside the box.
    static void add_modal_ok_btn(lv_obj_t* box, int32_t box_h,
                                   lv_color_t bg, lv_color_t border,
                                   lv_color_t label_color) {
        static constexpr int32_t BTN_W = 120;
        static constexpr int32_t BTN_H = 30;
        int32_t bx = (CW - BTN_W) / 2;
        int32_t by = box_h - BTN_H - 10;

        lv_obj_t* btn = make_obj(box, bx, by, BTN_W, BTN_H, 5,
                                  bg, LV_OPA_COVER, border, 2);
        lv_obj_t* lbl = make_label(btn, "[ OK ]", label_color, F12);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, modal_ok_handler, LV_EVENT_CLICKED, nullptr);
    }

    // Toast helpers

    // Creates the toast container on the active screen and sets s_toast.
    // Replaces any existing toast first.
    static lv_obj_t* make_toast_base(lv_color_t bg, lv_color_t border) {
        dismiss_toast();
        lv_obj_t* t = make_obj(lv_scr_act(),
                                MX, TOAST_Y, CW, TOAST_H, 8,
                                bg, LV_OPA_COVER, border, 2);
        s_toast = t;
        return t;
    }

    // Fills a toast: icon left, title + subtitle stacked to its right.
    static void fill_toast(lv_obj_t* t,
                            const char* icon, lv_color_t icon_color,
                            const char* title, lv_color_t title_color,
                            const char* subtitle, lv_color_t subtitle_color) {
        static constexpr int32_t ICON_LX  = 14;
        static constexpr int32_t TEXT_LX  = 38;
        static constexpr int32_t TEXT_W   = CW - TEXT_LX - 8;

        lv_obj_t* icon_lbl = make_label(t, icon, icon_color, F14);
        // If there's a subtitle, nudge the icon up slightly so it centres
        // between both lines; otherwise centre it in the full height.
        lv_obj_align(icon_lbl, LV_ALIGN_LEFT_MID, ICON_LX, subtitle ? -6 : 0);

        lv_obj_t* title_lbl = make_label(t, title, title_color, F12);
        lv_obj_set_size(title_lbl, TEXT_W, LV_SIZE_CONTENT);
        lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, TEXT_LX, 7);

        if (subtitle) {
            lv_obj_t* sub = make_label(t, subtitle, subtitle_color, F10);
            lv_obj_set_size(sub, TEXT_W, LV_SIZE_CONTENT);
            lv_obj_align(sub, LV_ALIGN_BOTTOM_LEFT, TEXT_LX, -6);
        }
    }


    // ═════════════════════════════════════════════════════════════════════════════
    // Public API — Screen creation
    // ═════════════════════════════════════════════════════════════════════════════

    void create_main_menu_screen(uint8_t selected_item) {
        if (screen_slot(screen_t::MAIN_MENU)) {
            utils::log<utils::log_level_t::WARN>(TAG, "create_main_menu_screen: already exists");
            return;
        }

        lv_obj_t* scr = make_screen_base();
        screen_slot(screen_t::MAIN_MENU) = scr;
        s_menu_selected = selected_item;

        make_top_bar(scr, "ESP PLOTTER");

        // Item definitions: title, optional subtitle, initial right-hand text
        struct ItemDef {
            const char* title;
            const char* subtitle;
            const char* right_text;  // nullptr = no right text
        };
        static constexpr ItemDef ITEMS[5] = {
            { "PLOT",  "Start plotting session",     nullptr  },
            { "WIFI",  "Upload G-code via browser",  nullptr  },
            { "SCALE", nullptr,                      "1.00\xc3\x97" },
            { "SPEED", nullptr,                      "1.00\xc3\x97" },
            { "ABOUT", nullptr,                      nullptr  },
        };

        int32_t y = CONTENT_Y + 8;

        for (uint8_t i = 0; i < 5; i++) {
            bool    sel    = (i == selected_item);
            int32_t h      = ITEMS[i].subtitle ? 44 : 30;
            lv_color_t bg  = sel ? C_ACCENT_BG : C_SURFACE;
            lv_color_t bdr = sel ? C_ACCENT    : C_SURFACE;

            lv_obj_t* item = make_obj(scr, MX, y, CW, h, 5,
                                       bg, LV_OPA_COVER, bdr, 2);
            s_menu_items[i] = item;

            // Title label — vertically centred, slightly raised if subtitle present
            lv_color_t tc = sel ? C_TEXT : lv_color_hex(color::UI_TEXT_MUTED); // TODO
            int32_t    ty = ITEMS[i].subtitle ? -(h / 4) : 0;
            lv_obj_t*  title_lbl = make_label(item, ITEMS[i].title, tc, F14);
            lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 14, ty);

            // Subtitle
            if (ITEMS[i].subtitle) {
                lv_obj_t* sub = make_label(item, ITEMS[i].subtitle, C_DIM, F10);
                lv_obj_align(sub, LV_ALIGN_LEFT_MID, 14, h / 4);
            }

            // WIFI: OFF badge on the right
            if (i == 1) {
                lv_obj_t* badge = make_obj(item, CW - 50, (h - 18) / 2, 44, 18, 9,
                                            lv_color_hex(color::UI_SURFACE_3), LV_OPA_COVER, C_BORDER, 1);
                lv_obj_t* bl = make_label(badge, "OFF", C_DIM, F10);
                lv_obj_align(bl, LV_ALIGN_CENTER, 0, 0);
            }

            // SCALE / SPEED: value text on the right
            if (ITEMS[i].right_text) {
                lv_obj_t* rv = make_label(item, ITEMS[i].right_text, C_DIMMER, F12);
                lv_obj_align(rv, LV_ALIGN_RIGHT_MID, -10, 0);
            }

            y += h + 4;
        }

        // Bottom encoder hint
        lv_obj_t* hint = make_label(scr, "\xe2\x86\x95 encoder  \xc2\xb7  OK to select", C_GHOST, F8);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    void create_wifi_screen(const wifi_data_t& data) {
        if (screen_slot(screen_t::WIFI)) {
            utils::log<utils::log_level_t::WARN>(TAG, "create_wifi_screen: already exists");
            return;
        }

        lv_obj_t* scr = make_screen_base();
        screen_slot(screen_t::WIFI) = scr;

        make_top_bar(scr, "WIFI");

        // "STATUS" section label
        lv_obj_t* sec = make_label(scr, "STATUS", C_DIM, F10);
        lv_obj_set_pos(sec, MX + 10, CONTENT_Y + 10);

        // Status box
        lv_color_t s_bg  = data.is_on ? C_GREEN_BG  : C_SURFACE;
        lv_color_t s_bdr = data.is_on ? C_GREEN_DK  : C_BORDER;
        lv_obj_t* sbox = make_obj(scr, MX, CONTENT_Y + 22, CW, 32, 5,
                                   s_bg, LV_OPA_COVER, s_bdr, 2);
        s_wifi_status_box = sbox;

        lv_obj_t* slbl = make_label(sbox,
                                     data.is_on ? "ON \xc2\xb7 Access Point" : "OFF",
                                     data.is_on ? C_GREEN : C_DIM, F12);
        lv_obj_align(slbl, LV_ALIGN_LEFT_MID, 14, 0);
        s_wifi_status_label = slbl;

        // "SSID" section label
        lv_obj_t* ssid_sec = make_label(scr, "SSID", C_DIM, F10);
        lv_obj_set_pos(ssid_sec, MX + 10, CONTENT_Y + 64);

        lv_obj_t* ssid_box = make_obj(scr, MX, CONTENT_Y + 76, CW, 28, 4,
                                       C_SURFACE2, LV_OPA_COVER, C_BORDER, 1);
        lv_obj_t* ssid_lbl = make_label(ssid_box, data.ssid, C_TEXT, F14);
        lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 16, 0);
        s_wifi_ssid_label = ssid_lbl;

        // "GATEWAY IP" section label
        lv_obj_t* ip_sec = make_label(scr, "GATEWAY IP", C_DIM, F10);
        lv_obj_set_pos(ip_sec, MX + 10, CONTENT_Y + 116);

        lv_obj_t* ip_box = make_obj(scr, MX, CONTENT_Y + 128, CW, 28, 4,
                                     C_SURFACE2, LV_OPA_COVER, C_BORDER, 1);
        lv_obj_t* ip_lbl = make_label(ip_box, data.ip, C_ACCENT, F14);
        lv_obj_align(ip_lbl, LV_ALIGN_LEFT_MID, 16, 0);
        s_wifi_ip_label = ip_lbl;

        // Status / hint lines at the bottom
        lv_obj_t* wait_lbl = make_label(scr,
                                         data.is_on ? "Waiting for upload..." : "",
                                         C_DIM, F10);
        lv_obj_align(wait_lbl, LV_ALIGN_BOTTOM_MID, 0, -18);
        s_wifi_hint_label = wait_lbl;

        lv_obj_t* hint = make_label(scr, "Auto-off on receive  \xc2\xb7  BACK to disable",
                                      C_GHOST, F8);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    void create_scale_screen(float current_scale) {
        if (screen_slot(screen_t::SCALE)) {
            utils::log<utils::log_level_t::WARN>(TAG, "create_scale_screen: already exists");
            return;
        }

        lv_obj_t* scr = make_screen_base();
        screen_slot(screen_t::SCALE) = scr;

        make_top_bar(scr, "SCALE");

        // Value box
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(current_scale));
        s_scale_value_label = make_value_box(scr, CONTENT_Y + 12, C_ACCENT, buf);

        // Range hint
        lv_obj_t* range_lbl = make_label(scr,
                                          "Range: 0.25\xc3\x97 \xe2\x80\x94 4.00\xc3\x97  \xc2\xb7  Step: 0.05",
                                          C_DIMMER, F10);
        lv_obj_align(range_lbl, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 106);

        // Range bar
        int32_t bar_val = range_to_bar(current_scale, SCALE_MIN, SCALE_MAX);
        s_scale_bar = make_bar(scr, MX + 20, CONTENT_Y + 120, CW - 40, 8,
                                C_ACCENT, bar_val, 100);

        // Encoder hint
        lv_obj_t* enc_lbl = make_label(scr, "\xe2\x86\xbb encoder to adjust", C_DIMMER, F10);
        lv_obj_align(enc_lbl, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 142);

        // Confirm / Cancel buttons
        make_button_row(scr, CONTENT_Y + 156,
                        C_BLUE_BG, C_BLUE,   "[ OK ]",   "CONFIRM", C_BLUE,   nullptr,
                        C_SURFACE, C_BORDER, "[ BACK ]", "CANCEL",  C_DIM);
    }

    void create_speed_screen(float current_speed) {
        if (screen_slot(screen_t::SPEED)) {
            utils::log<utils::log_level_t::WARN>(TAG, "create_speed_screen: already exists");
            return;
        }

        lv_obj_t* scr = make_screen_base();
        screen_slot(screen_t::SPEED) = scr;

        make_top_bar(scr, "SPEED");

        // Value box
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(current_speed));
        s_speed_value_label = make_value_box(scr, CONTENT_Y + 12, C_ORANGE, buf);

        // Computed feed rate
        char feed_buf[32];
        uint32_t eff = static_cast<uint32_t>(DEFAULT_FEED_RATE * current_speed);
        snprintf(feed_buf, sizeof(feed_buf), "= %lu mm/min", static_cast<unsigned long>(eff));
        s_speed_computed_label = make_label(scr, feed_buf, C_DIM, F12);
        lv_obj_align(s_speed_computed_label, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 100);

        // Range hint
        lv_obj_t* range_lbl = make_label(scr,
                                          "Range: 0.25\xc3\x97 \xe2\x80\x94 3.00\xc3\x97  \xc2\xb7  Step: 0.25",
                                          C_DIMMER, F10);
        lv_obj_align(range_lbl, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 118);

        // Range bar
        int32_t bar_val = range_to_bar(current_speed, SPEED_MIN, SPEED_MAX);
        s_speed_bar = make_bar(scr, MX + 20, CONTENT_Y + 132, CW - 40, 8,
                                C_ORANGE, bar_val, 100);

        // Encoder hint
        lv_obj_t* enc_lbl = make_label(scr, "\xe2\x86\xbb encoder to adjust", C_DIMMER, F10);
        lv_obj_align(enc_lbl, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 150);

        // Confirm / Cancel buttons
        make_button_row(scr, CONTENT_Y + 162,
                        C_ORANGE_BG, C_ORANGE_DK, "[ OK ]",   "CONFIRM", C_ORANGE, nullptr,
                        C_SURFACE,   C_BORDER,    "[ BACK ]", "CANCEL",  C_DIM);
    }

    void create_about_screen(const about_data_t& data) {
        if (screen_slot(screen_t::ABOUT)) {
            utils::log<utils::log_level_t::WARN>(TAG, "create_about_screen: already exists");
            return;
        }

        lv_obj_t* scr = make_screen_base();
        screen_slot(screen_t::ABOUT) = scr;

        make_top_bar(scr, "ABOUT");

        static constexpr int32_t ROW_H  = 20;
        static constexpr int32_t ROW_SP = 4;
        int32_t y = CONTENT_Y + 8;

        // FIRMWARE
        make_kv_row(scr, y, "FIRMWARE", data.firmware_version);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // STEPS / MM
        char steps_buf[16];
        snprintf(steps_buf, sizeof(steps_buf), "%.1f", static_cast<double>(data.steps_per_mm));
        make_kv_row(scr, y, "STEPS / MM", steps_buf);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // FEED RATE (effective)
        char feed_buf[24];
        snprintf(feed_buf, sizeof(feed_buf), "%lu mm/min",
                 static_cast<unsigned long>(data.effective_feed_rate));
        s_about_feed_rate_val = make_kv_row(scr, y, "FEED RATE", feed_buf);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // SCALE
        char scale_buf[12];
        snprintf(scale_buf, sizeof(scale_buf), "%.2f\xc3\x97", static_cast<double>(data.scale));
        s_about_scale_val = make_kv_row(scr, y, "SCALE", scale_buf);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // SPEED
        char speed_buf[12];
        snprintf(speed_buf, sizeof(speed_buf), "%.2f\xc3\x97", static_cast<double>(data.speed));
        s_about_speed_val = make_kv_row(scr, y, "SPEED", speed_buf);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // FILE SIZE
        char file_buf[16];
        snprintf(file_buf, sizeof(file_buf), "%.1f KB",
                 static_cast<double>(data.file_size_bytes) / 1024.0);
        s_about_file_size_val = make_kv_row(scr, y, "FILE SIZE", file_buf);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // FILESYSTEM
        char fs_buf[24];
        snprintf(fs_buf, sizeof(fs_buf), "%zu / %zu KB",
                 data.fs_used_bytes / 1024, data.fs_total_bytes / 1024);
        s_about_fs_val = make_kv_row(scr, y, "FILESYSTEM", fs_buf);
        y += ROW_H; make_sep(scr, y); y += ROW_SP + 2;

        // WIFI SSID (static — no update handle needed)
        make_kv_row(scr, y, "WIFI SSID", data.ssid);

        lv_obj_t* hint = make_label(scr, "[ BACK ] to return", C_GHOST, F8);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }

    void create_motion_screen(const motion_data_t& data) {
        if (screen_slot(screen_t::MOTION)) {
            utils::log<utils::log_level_t::WARN>(TAG, "create_motion_screen: already exists");
            return;
        }

        lv_obj_t* scr = make_screen_base();
        screen_slot(screen_t::MOTION) = scr;

        make_top_bar(scr, "PLOTTING");

        // State badge
        using S = gcode::types::state_t;
        lv_color_t badge_bg, badge_bdr, badge_tc;
        const char* badge_txt;

        if (data.state == S::RUNNING) {
            badge_bg  = lv_color_hex(color::UI_RUNNING_BG); // TODO
            badge_bdr = C_GREEN;
            badge_tc  = C_GREEN;
            badge_txt = "RUNNING";
        } else if (data.state == S::PAUSED) {
            badge_bg  = C_YELLOW_BG;
            badge_bdr = C_YELLOW;
            badge_tc  = C_YELLOW;
            badge_txt = "PAUSED";
        } else {
            badge_bg  = C_SURFACE;
            badge_bdr = C_BORDER;
            badge_tc  = C_DIM;
            badge_txt = "STOPPED";
        }

        make_state_badge(scr, CONTENT_Y + 8, badge_bg, badge_bdr, badge_txt, badge_tc,
                         &s_motion_badge_box, &s_motion_badge_label);

        // Progress label + percentage
        lv_obj_t* prog_lbl = make_label(scr, "PROGRESS", C_DIM, F10);
        lv_obj_set_pos(prog_lbl, MX + 10, CONTENT_Y + 44);

        char pct_buf[8];
        snprintf(pct_buf, sizeof(pct_buf), "%u%%", data.progress_pct);
        s_motion_pct_label = make_label(scr, pct_buf, C_SUBTEXT, F10);
        lv_obj_align(s_motion_pct_label, LV_ALIGN_TOP_RIGHT, -MX - 10, CONTENT_Y + 44);

        // Progress bar (colour follows state)
        lv_color_t bar_color = (data.state == S::PAUSED) ? C_YELLOW : C_ACCENT;
        s_motion_progress_bar = make_bar(scr, MX, CONTENT_Y + 58, CW, 16,
                                          bar_color, data.progress_pct, 100);

        // Line info label
        char line_buf[32];
        snprintf(line_buf, sizeof(line_buf), "Line %zu / %zu",
                 data.current_line, data.total_lines);
        s_motion_line_label = make_label(scr, line_buf, C_DIM, F10);
        lv_obj_set_pos(s_motion_line_label, MX + 10, CONTENT_Y + 84);

        // Scale / speed params (right-aligned, same row)
        char params_buf[40];
        snprintf(params_buf, sizeof(params_buf),
                 "Scale %.2f\xc3\x97  Speed %.2f\xc3\x97",
                 static_cast<double>(data.scale), static_cast<double>(data.speed));
        s_motion_params_label = make_label(scr, params_buf, lv_color_hex(color::UI_PARAMS_TEXT), F10); // TODO
        lv_obj_set_style_text_align(s_motion_params_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_size(s_motion_params_label, CW - 20, LV_SIZE_CONTENT);
        lv_obj_align(s_motion_params_label, LV_ALIGN_TOP_RIGHT, -MX - 10, CONTENT_Y + 84);

        make_sep(scr, CONTENT_Y + 102);

        lv_obj_t* ctrl_lbl = make_label(scr, "CONTROLS", C_FAINT, F8);
        lv_obj_align(ctrl_lbl, LV_ALIGN_TOP_MID, 0, CONTENT_Y + 110);

        // OK button (left) — label/style changes between RUNNING and PAUSED
        bool paused = (data.state == S::PAUSED);
        lv_color_t ok_bg  = paused ? C_GREEN_BG : C_BLUE_BG;
        lv_color_t ok_bdr = paused ? C_GREEN    : C_BLUE;
        lv_color_t ok_tc  = paused ? C_GREEN    : C_BLUE;
        const char* ok_txt = paused ? "RESUME"  : "PAUSE";

        s_motion_ok_btn_label = make_button_row(
            scr, CONTENT_Y + 122,
            ok_bg,  ok_bdr,  "[ OK ]",   ok_txt, ok_tc, &s_motion_ok_btn_box,
            C_RED_BG, C_RED, "[ BACK ]", "STOP", C_RED);

        lv_obj_t* hold_hint = make_label(scr,
                                          "Hold BACK 2s \xe2\x86\x92 abort & menu",
                                          lv_color_hex(color::UI_GHOST), F8); // TODO
        lv_obj_align(hold_hint, LV_ALIGN_BOTTOM_MID, 0, -4);
    }


    // Public API — Screen updates
    void update_main_menu_selection(uint8_t item) {
        if (!screen_slot(screen_t::MAIN_MENU)) {
            utils::log<utils::log_level_t::WARN>(TAG, "update_main_menu_selection: screen not created");
            return;
        }
        if (item >= 5) return;

        // Deselect old
        lv_obj_set_style_bg_color(s_menu_items[s_menu_selected], C_SURFACE, 0);
        lv_obj_set_style_border_color(s_menu_items[s_menu_selected], C_SURFACE, 0);

        // Select new
        lv_obj_set_style_bg_color(s_menu_items[item], C_ACCENT_BG, 0);
        lv_obj_set_style_border_color(s_menu_items[item], C_ACCENT, 0);

        s_menu_selected = item;
    }

    void update_wifi_screen(const wifi_data_t& data) {
        if (!screen_slot(screen_t::WIFI)) {
            utils::log<utils::log_level_t::WARN>(TAG, "update_wifi_screen: screen not created");
            return;
        }

        lv_color_t s_bg  = data.is_on ? C_GREEN_BG : C_SURFACE;
        lv_color_t s_bdr = data.is_on ? C_GREEN_DK : C_BORDER;
        lv_obj_set_style_bg_color(s_wifi_status_box, s_bg, 0);
        lv_obj_set_style_border_color(s_wifi_status_box, s_bdr, 0);

        lv_label_set_text(s_wifi_status_label,
                          data.is_on ? "ON \xc2\xb7 Access Point" : "OFF");
        lv_obj_set_style_text_color(s_wifi_status_label,
                                     data.is_on ? C_GREEN : C_DIM, 0);

        lv_label_set_text(s_wifi_ssid_label, data.ssid);
        lv_label_set_text(s_wifi_ip_label, data.ip);
        lv_label_set_text(s_wifi_hint_label, data.is_on ? "Waiting for upload..." : "");
    }

    void update_scale_screen(float scale) {
        if (!screen_slot(screen_t::SCALE)) {
            utils::log<utils::log_level_t::WARN>(TAG, "update_scale_screen: screen not created");
            return;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(scale));
        lv_label_set_text(s_scale_value_label, buf);
        lv_bar_set_value(s_scale_bar, range_to_bar(scale, SCALE_MIN, SCALE_MAX), LV_ANIM_OFF);
    }

    void update_speed_screen(float speed) {
        if (!screen_slot(screen_t::SPEED)) {
            utils::log<utils::log_level_t::WARN>(TAG, "update_speed_screen: screen not created");
            return;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(speed));
        lv_label_set_text(s_speed_value_label, buf);
        lv_bar_set_value(s_speed_bar, range_to_bar(speed, SPEED_MIN, SPEED_MAX), LV_ANIM_OFF);

        char feed_buf[32];
        uint32_t eff = static_cast<uint32_t>(DEFAULT_FEED_RATE * speed);
        snprintf(feed_buf, sizeof(feed_buf), "= %lu mm/min", static_cast<unsigned long>(eff));
        lv_label_set_text(s_speed_computed_label, feed_buf);
    }

    void update_about_screen(const about_data_t& data) {
        if (!screen_slot(screen_t::ABOUT)) {
            utils::log<utils::log_level_t::WARN>(TAG, "update_about_screen: screen not created");
            return;
        }

        char buf[32];

        snprintf(buf, sizeof(buf), "%lu mm/min",
                 static_cast<unsigned long>(data.effective_feed_rate));
        lv_label_set_text(s_about_feed_rate_val, buf);

        snprintf(buf, sizeof(buf), "%.2f\xc3\x97", static_cast<double>(data.scale));
        lv_label_set_text(s_about_scale_val, buf);

        snprintf(buf, sizeof(buf), "%.2f\xc3\x97", static_cast<double>(data.speed));
        lv_label_set_text(s_about_speed_val, buf);

        snprintf(buf, sizeof(buf), "%.1f KB",
                 static_cast<double>(data.file_size_bytes) / 1024.0);
        lv_label_set_text(s_about_file_size_val, buf);

        snprintf(buf, sizeof(buf), "%zu / %zu KB",
                 data.fs_used_bytes / 1024, data.fs_total_bytes / 1024);
        lv_label_set_text(s_about_fs_val, buf);
    }

    void update_motion_screen(const motion_data_t& data) {
        if (!screen_slot(screen_t::MOTION)) {
            utils::log<utils::log_level_t::WARN>(TAG, "update_motion_screen: screen not created");
            return;
        }

        using S = gcode::types::state_t;
        bool paused = (data.state == S::PAUSED);

        // Badge
        lv_color_t badge_bg, badge_bdr, badge_tc;
        const char* badge_txt;
        if (data.state == S::RUNNING) {
            badge_bg  = lv_color_hex(color::UI_RUNNING_BG); // TODO
            badge_bdr = C_GREEN;
            badge_tc  = C_GREEN;
            badge_txt = "RUNNING";
        } else if (paused) {
            badge_bg  = C_YELLOW_BG;             badge_bdr = C_YELLOW; badge_tc = C_YELLOW; badge_txt = "PAUSED";
        } else {
            badge_bg  = C_SURFACE;               badge_bdr = C_BORDER; badge_tc = C_DIM;    badge_txt = "STOPPED";
        }
        lv_obj_set_style_bg_color(s_motion_badge_box, badge_bg, 0);
        lv_obj_set_style_border_color(s_motion_badge_box, badge_bdr, 0);
        lv_label_set_text(s_motion_badge_label, badge_txt);
        lv_obj_set_style_text_color(s_motion_badge_label, badge_tc, 0);

        // Progress bar
        lv_color_t bar_color = paused ? C_YELLOW : C_ACCENT;
        lv_obj_set_style_bg_color(s_motion_progress_bar, bar_color, LV_PART_INDICATOR);
        lv_bar_set_value(s_motion_progress_bar, data.progress_pct, LV_ANIM_OFF);

        char pct_buf[8];
        snprintf(pct_buf, sizeof(pct_buf), "%u%%", data.progress_pct);
        lv_label_set_text(s_motion_pct_label, pct_buf);

        char line_buf[32];
        snprintf(line_buf, sizeof(line_buf), "Line %zu / %zu",
                 data.current_line, data.total_lines);
        lv_label_set_text(s_motion_line_label, line_buf);

        char params_buf[40];
        snprintf(params_buf, sizeof(params_buf),
                 "Scale %.2f\xc3\x97  Speed %.2f\xc3\x97",
                 static_cast<double>(data.scale), static_cast<double>(data.speed));
        lv_label_set_text(s_motion_params_label, params_buf);

        // OK button
        lv_color_t ok_bg  = paused ? C_GREEN_BG : C_BLUE_BG;
        lv_color_t ok_bdr = paused ? C_GREEN    : C_BLUE;
        lv_color_t ok_tc  = paused ? C_GREEN    : C_BLUE;
        lv_obj_set_style_bg_color(s_motion_ok_btn_box, ok_bg, 0);
        lv_obj_set_style_border_color(s_motion_ok_btn_box, ok_bdr, 0);
        lv_label_set_text(s_motion_ok_btn_label, paused ? "RESUME" : "PAUSE");
        lv_obj_set_style_text_color(s_motion_ok_btn_label, ok_tc, 0);
    }
    

    // Public API — Navigation
    void load_screen(screen_t screen) {
        lv_obj_t* scr = screen_slot(screen);
        if (!scr) {
            utils::log<utils::log_level_t::WARN>(TAG, "load_screen: screen not created");
            return;
        }
        lv_scr_load(scr);
    }

    
    // Public API — Toasts
    void show_toast_plotting_paused() {
        lv_obj_t* t = make_toast_base(C_YELLOW_BG, C_YELLOW);
        fill_toast(t,
                   LV_SYMBOL_PAUSE, C_YELLOW,
                   "Plotting paused",         C_YELLOW,
                   "Press OK to resume",      C_YELLOW_TXT_DK);
    }

    void show_toast_plotting_resumed(size_t from_line) {
        lv_obj_t* t = make_toast_base(C_GREEN_BG, C_GREEN);
        char sub[32];
        snprintf(sub, sizeof(sub), "Continuing from line %zu", from_line);
        fill_toast(t,
                   LV_SYMBOL_PLAY, C_GREEN,
                   "Plotting resumed", C_GREEN,
                   sub,                C_GREEN_TXT_DK);
    }

    void show_toast_plotting_complete() {
        lv_obj_t* t = make_toast_base(C_GREEN_BG, C_GREEN);
        fill_toast(t,
                   LV_SYMBOL_OK, C_GREEN,
                   "Plot complete",    C_GREEN,
                   "Returning to menu...", C_GREEN_TXT_DK);
    }

    void show_toast_wifi_enabled(const char* ssid, const char* ip) {
        lv_obj_t* t = make_toast_base(C_BLUE_BG, C_BLUE);
        char sub[48];
        snprintf(sub, sizeof(sub), "AP: %s  \xc2\xb7  %s", ssid, ip);
        fill_toast(t,
                   LV_SYMBOL_WIFI, C_BLUE,
                   "WiFi enabled", C_BLUE,
                   sub,            C_BLUE_DK);
    }

    void show_toast_wifi_disabled() {
        lv_obj_t* t = make_toast_base(C_SURFACE, C_BORDER);
        fill_toast(t,
                   LV_SYMBOL_CLOSE, C_SUBTEXT,
                   "WiFi disabled",       C_SUBTEXT,
                   "Access point stopped", C_DIM);
    }

    void show_toast_file_received() {
        lv_obj_t* t = make_toast_base(C_GREEN_BG, C_GREEN);
        fill_toast(t,
                   LV_SYMBOL_DOWNLOAD, C_GREEN,
                   "File received",    C_GREEN,
                   "WiFi off  \xc2\xb7  Ready to plot", C_GREEN_TXT_DK);
    }

    void dismiss_toast() {
        if (!s_toast) return;
        lv_obj_del(s_toast);
        s_toast = nullptr;
    }


    // Public API — Modals
    void show_modal_parse_error(size_t line_num, const char* line_str, void (*on_ok)()) {
        static constexpr int32_t BOX_H = 172;
        static constexpr int32_t BOX_Y = (HEIGHT - BOX_H) / 2;

        lv_obj_t* box = make_modal_base(BOX_Y, BOX_H,
                                         C_RED_BG, C_RED, C_RED_HDR,
                                         "\xe2\x9a\xa0  PARSE ERROR", C_RED_TXT,
                                         on_ok);

        add_modal_label(box, 0,  "Too many consecutive parse errors.", C_TEXT,     F12);
        add_modal_label(box, 18, "Session aborted.",                   C_TEXT,     F12);

        // Error line box
        char line_buf[gcode::config::MAX_GCODE_LINE_LENGTH + 16];
        snprintf(line_buf, sizeof(line_buf), "Line %zu:  %s", line_num, line_str);
        lv_obj_t* err_box = make_obj(box, 14, MODAL_HDR_H + 46, CW - 28, 22, 3,
                                      lv_color_hex(color::UI_SURFACE_3), LV_OPA_COVER, C_BORDER, 1);
        lv_obj_t* err_lbl = make_label(err_box, line_buf, C_RED_TXT, F10);
        lv_obj_align(err_lbl, LV_ALIGN_LEFT_MID, 8, 0);

        add_modal_label(box, 82, "Session will be stopped.", C_DIM, F10);

        add_modal_ok_btn(box, BOX_H, C_RED_BG, C_RED, C_RED_TXT);
    }

    void show_modal_file_not_found(void (*on_ok)()) {
        static constexpr int32_t BOX_H = 158;
        static constexpr int32_t BOX_Y = (HEIGHT - BOX_H) / 2;

        lv_obj_t* box = make_modal_base(BOX_Y, BOX_H,
                                         C_RED_BG, C_RED, C_RED_HDR,
                                         "\xe2\x9a\xa0  FILE NOT FOUND", C_RED_TXT,
                                         on_ok);

        add_modal_label(box, 0,  "No G-code file on filesystem.",      C_TEXT, F12);
        add_modal_label(box, 22, "Upload a file via WiFi first,",      C_SUBTEXT, F10);
        add_modal_label(box, 38, "then try plotting again.",           C_SUBTEXT, F10);

        add_modal_ok_btn(box, BOX_H, C_RED_BG, C_RED, C_RED_TXT);
    }

    void show_modal_file_read_error(void (*on_ok)()) {
        static constexpr int32_t BOX_H = 162;
        static constexpr int32_t BOX_Y = (HEIGHT - BOX_H) / 2;

        lv_obj_t* box = make_modal_base(BOX_Y, BOX_H,
                                         C_RED_BG, C_RED, C_RED_HDR,
                                         "\xe2\x9a\xa0  FILE READ ERROR", C_RED_TXT,
                                         on_ok);

        add_modal_label(box, 0,  "Filesystem I/O error during",        C_TEXT, F12);
        add_modal_label(box, 18, "session. File may be corrupt.",      C_TEXT, F12);
        add_modal_label(box, 44, "Re-upload the file and try again.",  C_SUBTEXT, F10);

        add_modal_ok_btn(box, BOX_H, C_RED_BG, C_RED, C_RED_TXT);
    }

    void show_modal_session_stopped(size_t completed_lines, size_t total_lines, void (*on_ok)()) {
        static constexpr int32_t BOX_H = 164;
        static constexpr int32_t BOX_Y = (HEIGHT - BOX_H) / 2;

        lv_obj_t* box = make_modal_base(BOX_Y, BOX_H,
                                         lv_color_hex(color::UI_AMBER_HEADER), C_YELLOW, C_MUTED_YEL,
                                         "\xe2\x96\xa0  SESSION STOPPED", C_YELLOW,
                                         on_ok);

        add_modal_label(box, 0,  "Plotting stopped before completion.", C_TEXT, F12);

        char lines_buf[40];
        snprintf(lines_buf, sizeof(lines_buf), "Lines completed:  %zu / %zu",
                 completed_lines, total_lines);
        add_modal_label(box, 24, lines_buf, C_SUBTEXT, F10);
        add_modal_label(box, 44, "File is unchanged. You can plot",  C_DIM, F10);
        add_modal_label(box, 60, "again from the beginning.",        C_DIM, F10);

        add_modal_ok_btn(box, BOX_H, lv_color_hex(color::UI_AMBER_HEADER), C_YELLOW, C_YELLOW);
    }

    void dismiss_modal() {
        if (!s_modal_overlay) return;
        lv_obj_del(s_modal_overlay);   // deletes box + all children too
        s_modal_overlay = nullptr;
        s_modal_ok_cb   = nullptr;
    }

} // namespace display
