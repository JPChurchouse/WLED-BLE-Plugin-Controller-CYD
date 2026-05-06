Good call. Let me give you the conceptual and architectural foundation so you can build and test each piece independently, rather than having a monolithic blob that's hard to debug.

---

## Architecture Overview

The project has four independent layers with one-way or clearly defined two-way interfaces:

```
┌─────────────────────────────────────────────┐
│               Application (main)             │
│   orchestrates tick, polls modules           │
└──────────────┬──────────────┬───────────────┘
               │              │
┌──────────────▼──┐    ┌──────▼──────────────┐
│   UI Manager    │    │    BLE Client        │
│  (LVGL screens) │    │  (NimBLE, task)      │
└──────────────┬──┘    └──────┬───────────────┘
               │              │
┌──────────────▼──────────────▼───────────────┐
│              HAL                             │
│   Display · Touch · LED                      │
└─────────────────────────────────────────────┘
```

The BLE client and UI manager never call each other directly. They communicate through a single shared-state struct protected by a mutex. The application layer reads dirty flags from that struct each loop iteration and forwards them to the UI.

---

## Module 1: HAL (Display + Touch + LED)

### What it does

Owns every hardware peripheral. Everything above it talks to hardware through this module only. The key point: **get this working completely before adding LVGL**.

### How to test it independently

Write a standalone sketch — no LVGL at all — that:

```cpp
#include "display_setup.h"

void setup() {
    Serial.begin(115200);
    gfx.init();
    gfx.setRotation(1);
    Serial.printf("w=%d h=%d\n", gfx.width(), gfx.height());

    gfx.fillScreen(TFT_RED);
    delay(500);
    gfx.fillScreen(TFT_GREEN);
    delay(500);
    gfx.fillScreen(TFT_BLUE);
    delay(500);
    gfx.drawRect(10, 10, 100, 50, TFT_WHITE);
    gfx.drawString("Hello", 50, 30, 2);
}

void loop() {
    uint16_t x, y;
    if (gfx.getTouch(&x, &y)) {
        Serial.printf("Touch: %d, %d\n", x, y);
        gfx.fillCircle(x, y, 5, TFT_YELLOW);
    }
}
```

This tells you immediately whether:
- Rotation is correct (red/green/blue flash fills the full screen in landscape)
- Touch is responding and mapped to correct coordinates
- Serial output shows `w=320 h=240` confirming landscape

**Only proceed to LVGL once this is clean.**

### Interface it exposes

```cpp
// display_setup.h
extern LGFX gfx;         // LovyanGFX device — used by flush/touch callbacks
void display_init();      // called once from setup(), before lv_init()

// rgb_led.h
enum class LedColor { OFF, RED, GREEN, BLUE };
void rgb_led_init();
void rgb_led_set(LedColor c);
```

### How LVGL connects to it

LVGL doesn't call LovyanGFX directly. You register two function pointers with LVGL at init time — a **flush callback** and a **touch read callback**. LVGL calls these at its own rate.

**Flush callback** — called by LVGL when a rectangular region of pixels is ready to be sent to the screen:
```cpp
// LVGL says: "here's a rectangle of pixels, please push them"
void disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* buf) {
    // You call LovyanGFX here
    // Then you MUST call this when done:
    lv_disp_flush_ready(drv);
}
```

**Touch read callback** — called by LVGL's input subsystem every `LV_INDEV_DEF_READ_PERIOD` ms:
```cpp
// LVGL says: "is anything being pressed right now?"
void touch_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t x, y;
    if (gfx.getTouch(&x, &y)) {
        data->state   = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
```

Both callbacks are registered inside `display_init()` after `lv_init()`.

---

## Module 2: BLE Client

### Conceptual model

BLE GATT is a client/server model. Your CYD is the **client** (called Central). The WLED controller is the **server** (called Peripheral). The server exposes a **service** (a logical grouping) containing **characteristics** (individual data points you can read, write, or subscribe to).

```
Server (WLED)
└── Service 4fafc201-...
    ├── Power char 4fafc202-...       read + write + notify
    ├── Presets char 4fafc203-...     read only
    └── Active char 4fafc204-...      read + write + notify
```

### The five operations you use

| Operation | NimBLE call | When |
|---|---|---|
| Read | `pChr->readValue()` | On connect: get initial power, active preset, preset list |
| Write without response | `pChr->writeValue(&val, 1, false)` | User taps power or preset |
| Subscribe (notify) | `pChr->subscribe(true, callback)` | On connect: power + active preset |
| Scan | `NimBLEDevice::getScan()->start(...)` | Boot and after disconnect |
| Connect | `pClient->connect(device)` | After scan finds target |

### The state machine

```
IDLE ──► SCANNING ──► CONNECTING ──► CONNECTED
                                          │
                              disconnect  │
                                 ◄────────┘
                                 │
                              DISCONNECTED ──► SCANNING (after delay)
```

Each state transition updates the shared struct and sets a dirty flag so the UI can react.

### Threading model — critical to get right

NimBLE runs its own FreeRTOS task on **core 0**. Arduino `loop()` runs on **core 1**. They must not call each other's APIs without synchronisation.

The pattern used here:

```
Core 0 (BLE task)           Core 1 (loop/LVGL)
─────────────────           ──────────────────
scan callback               read g_ble under mutex
  → set flag                update LVGL widgets
  → release mutex           call ble_write_*()
onDisconnect                  → NimBLE write
  → set flag                  (safe across cores)
doConnect()
  → readValue() blocks here
  → updates g_ble under mutex
```

`ble_write_power()` and `ble_write_preset()` can safely be called from `loop()` because NimBLE's write API posts a command to its internal event queue — it's designed to be called from any task.

### Notification callbacks

When the server sends a notification (e.g. user changed power state from another device), NimBLE calls your registered function on its own task:

```cpp
static void onPowerNotify(NimBLERemoteCharacteristic*, 
                          uint8_t* data, size_t len, bool isNotify) {
    // Running on BLE task — must not touch LVGL here
    // Write to shared struct under mutex, set dirty flag
    // LVGL update happens on core 1 next loop iteration
}
```

### Interface it exposes

```cpp
// ble_client.h

enum class BLEStatus : uint8_t { IDLE, SCANNING, CONNECTING, CONNECTED, DISCONNECTED };

struct PresetInfo { uint8_t id; std::string name; };

struct BLESharedData {
    SemaphoreHandle_t mutex;
    BLEStatus  status;
    bool       powerOn;
    uint8_t    activePresetId;   // 0xFF = none
    std::vector<PresetInfo> presets;
    // Dirty flags — set by BLE task, cleared by UI after processing
    bool statusChanged, powerChanged, activePresetChanged, presetsChanged;
};

extern BLESharedData g_ble;   // the single shared-state object

void ble_init();               // spawns BLE task, starts scanning
void ble_write_power(bool on); // safe to call from loop()
void ble_write_preset(uint8_t id);
```

### How to test it independently

Add this to `loop()` temporarily before the UI exists:

```cpp
void loop() {
    static BLEStatus last = BLEStatus::IDLE;
    if (xSemaphoreTake(g_ble.mutex, 0) == pdTRUE) {
        if (g_ble.status != last) {
            last = g_ble.status;
            Serial.printf("BLE status: %d\n", (int)g_ble.status);
        }
        if (g_ble.presetsChanged) {
            g_ble.presetsChanged = false;
            for (auto& p : g_ble.presets)
                Serial.printf("  Preset %d: %s\n", p.id, p.name.c_str());
        }
        if (g_ble.powerChanged) {
            g_ble.powerChanged = false;
            Serial.printf("Power: %s\n", g_ble.powerOn ? "ON" : "OFF");
        }
        xSemaphoreGive(g_ble.mutex);
    }
    delay(50);
}
```

You should see the scan finding WLED-BLE, presets printing, and power state appearing — all without a single line of LVGL.

---

## Module 3: UI Manager

### LVGL's object model

Everything in LVGL is an `lv_obj_t*`. Screens are objects. Buttons are objects. Labels are objects. Every object has a **parent** — when the parent moves or is deleted, children follow. Screens are the root objects with no parent (`lv_obj_create(NULL)`).

```
lv_scr_act()          ← the currently displayed screen
  └── lv_obj_create(scr)    ← a container or button
        └── lv_label_create(btn)  ← a label inside a button
```

### The two screens you need

**Connecting screen** — shown at boot and after disconnect:
- `lv_spinner_create(parent, speed_ms, arc_degrees)` — the animated arc
- `lv_label_create(parent)` + `lv_label_set_text(lbl, "Scanning...")` — status text
- Background: set via `lv_obj_set_style_bg_color(scr, color, 0)`

**Main screen** — shown when connected:
- A power button: `lv_btn_create(parent)` with a `lv_label_create(btn)` inside
- A scrollable preset list: `lv_obj_create(parent)` with flex layout
- Individual preset items: `lv_btn_create(list_container)` for each preset

### How events work

You attach a callback to an object for a specific event code:

```cpp
lv_obj_add_event_cb(btn_power, my_callback, LV_EVENT_CLICKED, user_data_ptr);

static void my_callback(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    void* data = lv_event_get_user_data(e);  // your pointer, e.g. preset ID
    // do something
}
```

For preset buttons you pass the preset ID as user data:

```cpp
// Cast uint8_t to pointer — safe because pointer is >= 4 bytes on ESP32
lv_obj_add_event_cb(btn, preset_cb, LV_EVENT_CLICKED,
                    (void*)(uintptr_t)preset_id);

// Recover in callback:
uint8_t id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
```

### How styles work

LVGL uses a CSS-like style system. You can style individual properties directly on an object without defining a full style object:

```cpp
// Direct inline style — simplest approach
lv_obj_set_style_bg_color(obj, lv_color_make(0x00, 0xAA, 0x44), LV_STATE_DEFAULT);
lv_obj_set_style_bg_color(obj, lv_color_make(0x00, 0x77, 0x30), LV_STATE_PRESSED);
lv_obj_set_style_text_color(obj, lv_color_white(), LV_STATE_DEFAULT);
lv_obj_set_style_radius(obj, 8, LV_STATE_DEFAULT);
lv_obj_set_style_shadow_width(obj, 0, LV_STATE_DEFAULT);
```

The second argument to every `lv_obj_set_style_*` is a **selector** combining a part and a state. `0` means `LV_PART_MAIN | LV_STATE_DEFAULT` — the main visual element in its resting state. `LV_STATE_PRESSED` is for the pressed visual.

To dynamically change a button's appearance (e.g. ON/OFF power state), just call `lv_obj_set_style_bg_color` again with the new colour — LVGL invalidates and redraws automatically.

### Scrollable preset list

A scrollable flex column is the most controllable approach:

```cpp
lv_obj_t* list = lv_obj_create(parent);
lv_obj_set_size(list, width, height);
lv_obj_set_layout(list, LV_LAYOUT_FLEX);
lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
lv_obj_set_scroll_dir(list, LV_DIR_VER);
lv_obj_set_style_pad_row(list, 3, 0);  // gap between items
```

Add items by creating buttons inside `list`. LVGL automatically handles scroll when items overflow the container height. Touch-and-drag scrolls the list; short-tap fires `LV_EVENT_CLICKED` on the item.

### Screen transitions

```cpp
// Load immediately (no animation)
lv_scr_load(scr_connecting);

// Load with fade (don't delete old screen — pass false)
lv_scr_load_anim(scr_main, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
```

Keep both screens alive in memory — you switch between them on connect/disconnect. Since `auto_del=false`, neither is freed and you can switch back freely.

### Interface it exposes

```cpp
// ui_manager.h

void ui_init();          // build both screens, load connecting screen
void ui_apply_updates(); // call every loop() — reads g_ble dirty flags,
                         // updates widgets as needed
                         // NOT thread-safe — call from loop() only
```

`ui_apply_updates()` is the only function the application layer calls regularly. Internally it:
1. Takes the mutex (non-blocking — skips if BLE task holds it)
2. Snapshots dirty flags and values
3. Releases mutex
4. Updates widgets from the snapshot (no mutex held during LVGL calls)

### How to test it independently

Before BLE works, drive it with fake data:

```cpp
// In main, after ui_init():

// Simulate "connected" after 3 seconds
// Simulate preset list
// Simulate power state
if (millis() > 3000 && !faked) {
    faked = true;
    if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY)) {
        g_ble.status = BLEStatus::CONNECTED;
        g_ble.statusChanged = true;
        g_ble.powerOn = true;
        g_ble.powerChanged = true;
        g_ble.activePresetId = 2;
        g_ble.activePresetChanged = true;
        g_ble.presets = {{1,"Rainbow"},{2,"Solid Red"},{3,"Ocean"}};
        g_ble.presetsChanged = true;
        xSemaphoreGive(g_ble.mutex);
    }
}
```

This lets you build and iterate the entire UI without any BLE hardware nearby.

---

## Module 4: Application (`main.cpp`)

The application layer is deliberately thin — it owns no state and makes no decisions. It just:

1. Calls `lv_tick_inc(elapsed)` — keeps LVGL's internal clock accurate
2. Calls `lv_timer_handler()` — runs LVGL's render and animation engine
3. Calls `ui_apply_updates()` — bridges BLE state to UI
4. Updates the RGB LED based on BLE status

```cpp
void loop() {
    // LVGL tick — must be called as frequently as possible
    uint32_t now = millis();
    lv_tick_inc(now - lastTick);
    lastTick = now;

    // LVGL render pass — internally calls flush callback when dirty
    lv_timer_handler();

    // Bridge: BLE state → UI widgets
    ui_apply_updates();

    // Status LED
    updateLed();

    delay(5);  // ~200Hz loop, LVGL has internal rate limiting
}
```

---

## Recommended build order

Work through these in sequence, confirming each is solid before adding the next:

```
Step 1: HAL only
    ├── LovyanGFX draws coloured rectangles correctly in landscape
    ├── fillScreen covers the entire display (no old content visible)
    └── Touch coordinates print sensibly to Serial

Step 2: LVGL on HAL  
    ├── Connecting screen renders (spinner + label, full background)
    ├── No corruption
    └── Touch events reach LVGL (tap anywhere, see Serial output)

Step 3: BLE only (no UI)
    ├── Serial shows scan → connect → presets printed
    ├── Power notifications arrive
    └── Writing power/preset causes visible WLED response

Step 4: Wire BLE → UI via fake data first
    ├── Preset list populates
    ├── Active preset highlights
    └── Power button reflects state

Step 5: Replace fake data driver with real BLE module
    └── Full end-to-end test
```

The display issue that's been blocking everything should be completely isolated in Step 1 without any LVGL complexity. If `gfx.fillScreen(TFT_RED)` leaves any part of the screen untouched, or `setRotation(1)` doesn't give 320×240 according to `gfx.width()`/`gfx.height()`, that's a pure LovyanGFX config issue with a small, focused reproduction case you can fix quickly.