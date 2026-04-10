#pragma once

// Initialise LVGL screens. Call once after display_init().
void ui_init();

// Poll g_ble dirty flags and update the UI accordingly.
// Must be called from the main (LVGL) task — not thread-safe.
void ui_apply_updates();