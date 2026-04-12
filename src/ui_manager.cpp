#include "ui_manager.hpp"
#include "ble_client.hpp"
#include "config.hpp"
#include <lvgl.h>
#include <vector>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Internal state
// ─────────────────────────────────────────────────────────────────────────────
static BLEStatus s_lastStatus    = BLEStatus::IDLE;
static bool      s_localPowerOn  = false;
static uint8_t   s_localActiveId = 0xFF;

// ─────────────────────────────────────────────────────────────────────────────
// Screen objects
// ─────────────────────────────────────────────────────────────────────────────
static lv_obj_t* scr_connecting = nullptr;
static lv_obj_t* scr_main       = nullptr;

// Connecting screen
static lv_obj_t* lbl_scan_status = nullptr;

// Main screen
static lv_obj_t* btn_power       = nullptr;
static lv_obj_t* lbl_power       = nullptr;
static lv_obj_t* cont_presets    = nullptr;   // scrollable flex container

// Preset button tracking
struct PresetBtn {
    uint8_t      id;
    lv_obj_t*    obj;
};
static std::vector<PresetBtn> s_presetBtns;

// ─────────────────────────────────────────────────────────────────────────────
// Shared styles
// ─────────────────────────────────────────────────────────────────────────────
static lv_style_t sty_preset_normal;
static lv_style_t sty_preset_active;
static bool       s_styles_ready = false;

static void init_styles() {
    if (s_styles_ready) return;

    lv_style_init(&sty_preset_normal);
    lv_style_set_bg_color    (&sty_preset_normal, lv_color_make(0x35, 0x35, 0x40));
    lv_style_set_bg_opa      (&sty_preset_normal, LV_OPA_COVER);
    lv_style_set_text_color  (&sty_preset_normal, lv_color_make(0xCC, 0xCC, 0xCC));
    lv_style_set_border_width(&sty_preset_normal, 0);
    lv_style_set_radius      (&sty_preset_normal, 5);
    lv_style_set_pad_hor     (&sty_preset_normal, 12);
    lv_style_set_pad_ver     (&sty_preset_normal, 0);
    lv_style_set_shadow_width(&sty_preset_normal, 0);

    lv_style_init(&sty_preset_active);
    lv_style_set_bg_color    (&sty_preset_active, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_bg_opa      (&sty_preset_active, LV_OPA_COVER);
    lv_style_set_text_color  (&sty_preset_active, lv_color_white());
    lv_style_set_border_width(&sty_preset_active, 0);
    lv_style_set_radius      (&sty_preset_active, 5);
    lv_style_set_pad_hor     (&sty_preset_active, 12);
    lv_style_set_pad_ver     (&sty_preset_active, 0);
    lv_style_set_shadow_width(&sty_preset_active, 0);

    s_styles_ready = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Power button
// ─────────────────────────────────────────────────────────────────────────────
static void apply_power_style(bool on) {
    if (!btn_power) return;
    if (on) {
        lv_obj_set_style_bg_color(btn_power, lv_color_make(0x00, 0xB0, 0x50),
                                  LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_power, lv_color_make(0x00, 0x88, 0x3A),
                                  LV_STATE_PRESSED);
        lv_label_set_text(lbl_power, LV_SYMBOL_PLAY "  ON");
    } else {
        lv_obj_set_style_bg_color(btn_power, lv_color_make(0x50, 0x50, 0x58),
                                  LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn_power, lv_color_make(0x38, 0x38, 0x40),
                                  LV_STATE_PRESSED);
        lv_label_set_text(lbl_power, LV_SYMBOL_STOP "  OFF");
    }
}

static void on_power_btn_click(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    s_localPowerOn = !s_localPowerOn;
    ble_write_power(s_localPowerOn);
    // Optimistic update — corrected by the incoming BLE notification
    apply_power_style(s_localPowerOn);
}

// ─────────────────────────────────────────────────────────────────────────────
// Preset list
// ─────────────────────────────────────────────────────────────────────────────
static void apply_active_highlight(uint8_t activeId) {
    s_localActiveId = activeId;
    for (auto& pb : s_presetBtns) {
        bool isActive = (pb.id == activeId && activeId != 0xFF);
        // Remove both styles, then add the correct one
        lv_obj_remove_style(pb.obj, &sty_preset_normal, LV_STATE_DEFAULT);
        lv_obj_remove_style(pb.obj, &sty_preset_active, LV_STATE_DEFAULT);
        lv_obj_add_style   (pb.obj, isActive ? &sty_preset_active : &sty_preset_normal,
                            LV_STATE_DEFAULT);
    }
}

static void on_preset_click(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    auto id = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));
    ble_write_preset(id);
    apply_active_highlight(id);   // optimistic
}

static void rebuild_preset_list(const std::vector<PresetInfo>& presets) {
    if (!cont_presets) return;

    lv_obj_clean(cont_presets);
    s_presetBtns.clear();

    for (const auto& p : presets) {
        lv_obj_t* btn = lv_btn_create(cont_presets);
        // Width: fill container; height fixed so all items are uniform
        lv_obj_set_size(btn, LV_PCT(100), 38);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_add_style(btn, &sty_preset_normal, LV_STATE_DEFAULT);
        lv_obj_add_event_cb(btn, on_preset_click, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<uintptr_t>(p.id)));

        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, p.name.c_str());
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

        s_presetBtns.push_back({p.id, btn});
    }

    apply_active_highlight(s_localActiveId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen builders
// ─────────────────────────────────────────────────────────────────────────────
static void build_connecting_screen() {
    scr_connecting = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_connecting, lv_color_make(0x10, 0x10, 0x18), 0);
    lv_obj_clear_flag(scr_connecting, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* spinner = lv_spinner_create(scr_connecting, 1000, 60);
    lv_obj_set_size(spinner, 64, 64);
    lv_obj_align(spinner, LV_ALIGN_CENTER, 0, -24);
    lv_obj_set_style_arc_color(spinner, lv_palette_main(LV_PALETTE_LIGHT_BLUE),
                               LV_PART_INDICATOR);

    lbl_scan_status = lv_label_create(scr_connecting);
    lv_obj_set_style_text_color(lbl_scan_status, lv_color_make(0xAA, 0xAA, 0xBB), 0);
    lv_label_set_text(lbl_scan_status, "Scanning for WLED...");
    lv_obj_align(lbl_scan_status, LV_ALIGN_CENTER, 0, 28);
}

static void build_main_screen() {
    init_styles();

    scr_main = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_main, lv_color_make(0x18, 0x18, 0x22), 0);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);

    // ── Header bar (height 26px) ──────────────────────────────────────────
    lv_obj_t* hdr = lv_obj_create(scr_main);
    lv_obj_set_size(hdr, DISP_WIDTH, 26);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color  (hdr, lv_color_make(0x20, 0x20, 0x30), 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_radius    (hdr, 0, 0);
    lv_obj_clear_flag           (hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "WLED Remote");
    lv_obj_set_style_text_color(lbl_title, lv_color_make(0xFF, 0xFF, 0xFF), 0);
    lv_obj_align(lbl_title, LV_ALIGN_LEFT_MID, 8, 0);

    lv_obj_t* lbl_conn = lv_label_create(hdr);
    lv_label_set_text(lbl_conn, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(lbl_conn, lv_color_make(0x00, 0xCC, 0x66), 0);
    lv_obj_align(lbl_conn, LV_ALIGN_RIGHT_MID, -8, 0);

    // ── Power button (height 62px, y=34) ─────────────────────────────────
    btn_power = lv_btn_create(scr_main);
    lv_obj_set_size(btn_power, DISP_WIDTH - 20, 62);
    lv_obj_align(btn_power, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_style_radius(btn_power, 8, 0);
    lv_obj_set_style_shadow_width(btn_power, 0, 0);
    lv_obj_add_event_cb(btn_power, on_power_btn_click, LV_EVENT_CLICKED, nullptr);

    lbl_power = lv_label_create(btn_power);
    lv_obj_set_style_text_font(lbl_power, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_power);
    apply_power_style(false);   // default until BLE state arrives

    // ── "Presets" section label (y=105) ──────────────────────────────────
    lv_obj_t* lbl_hdr = lv_label_create(scr_main);
    lv_label_set_text(lbl_hdr, "Presets");
    lv_obj_set_style_text_color(lbl_hdr, lv_color_make(0x77, 0x77, 0x88), 0);
    lv_obj_set_pos(lbl_hdr, 10, 105);

    // ── Preset scroll container (y=124, fills to bottom) ─────────────────
    // A plain lv_obj with vertical flex layout is used instead of lv_list
    // so we have full control over item styling.
    cont_presets = lv_obj_create(scr_main);
    lv_obj_set_size(cont_presets, DISP_WIDTH - 10, DISP_HEIGHT - 128);
    lv_obj_set_pos (cont_presets, 5, 124);
    lv_obj_set_style_bg_color    (cont_presets, lv_color_make(0x20, 0x20, 0x28), 0);
    lv_obj_set_style_border_width(cont_presets, 1, 0);
    lv_obj_set_style_border_color(cont_presets, lv_color_make(0x40, 0x40, 0x50), 0);
    lv_obj_set_style_radius      (cont_presets, 6, 0);
    lv_obj_set_style_pad_all     (cont_presets, 4, 0);
    lv_obj_set_style_pad_row     (cont_presets, 3, 0);
    lv_obj_set_scroll_dir        (cont_presets, LV_DIR_VER);
    lv_obj_set_scrollbar_mode    (cont_presets, LV_SCROLLBAR_MODE_ACTIVE);
    lv_obj_set_layout            (cont_presets, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow         (cont_presets, LV_FLEX_FLOW_COLUMN);
}

// ─────────────────────────────────────────────────────────────────────────────
// Connecting screen label text
// ─────────────────────────────────────────────────────────────────────────────
static void update_scan_label(BLEStatus st) {
    if (!lbl_scan_status) return;
    switch (st) {
        case BLEStatus::SCANNING:
            lv_label_set_text(lbl_scan_status, "Scanning for WLED...");   break;
        case BLEStatus::CONNECTING:
            lv_label_set_text(lbl_scan_status, "Connecting...");          break;
        case BLEStatus::DISCONNECTED:
            lv_label_set_text(lbl_scan_status,
                              "Connection lost.\nReconnecting...");        break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void ui_init() {
    build_connecting_screen();
    build_main_screen();
    lv_scr_load(scr_connecting);
}

void ui_apply_updates() {
    // Non-blocking mutex attempt — skip this frame if the BLE task holds it
    if (xSemaphoreTake(g_ble.mutex, 0) != pdTRUE) return;

    // Snapshot everything under the lock, then release immediately
    BLEStatus curStatus      = g_ble.status;
    bool      powerChanged   = g_ble.powerChanged;
    bool      activeChanged  = g_ble.activePresetChanged;
    bool      presetsChanged = g_ble.presetsChanged;
    bool      powerVal       = g_ble.powerOn;
    uint8_t   activeId       = g_ble.activePresetId;

    // Clear dirty flags while we still hold the lock
    g_ble.statusChanged       = false;
    g_ble.powerChanged        = false;
    g_ble.activePresetChanged = false;
    g_ble.presetsChanged      = false;

    // Copy preset list only when needed (avoids unnecessary allocation)
    std::vector<PresetInfo> presets;
    if (presetsChanged) presets = g_ble.presets;

    xSemaphoreGive(g_ble.mutex);

    // ── Status / screen transitions ────────────────────────────────────────
    if (curStatus != s_lastStatus) {
        s_lastStatus = curStatus;

        if (curStatus == BLEStatus::CONNECTED) {
            lv_scr_load_anim(scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
        } else {
            update_scan_label(curStatus);
            if (lv_scr_act() != scr_connecting) {
                lv_scr_load_anim(scr_connecting, LV_SCR_LOAD_ANIM_FADE_IN,
                                 300, 0, false);
            }
        }
    }

    // Only update main-screen widgets when the main screen is visible
    if (lv_scr_act() != scr_main) return;

    if (powerChanged) {
        s_localPowerOn = powerVal;
        apply_power_style(powerVal);
    }
    if (activeChanged && !presetsChanged) {
        // Preset list exists — just update the highlight
        apply_active_highlight(activeId);
    }
    if (presetsChanged) {
        // Update local active ID first so rebuild_preset_list() picks it up
        if (activeChanged) s_localActiveId = activeId;
        rebuild_preset_list(presets);
    }
}