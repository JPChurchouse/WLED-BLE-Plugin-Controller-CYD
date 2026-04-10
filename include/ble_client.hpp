#pragma once
#include <Arduino.h>
#include <vector>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ── Connection state machine ──────────────────────────────────────────────
enum class BLEStatus : uint8_t {
    IDLE,
    SCANNING,
    CONNECTING,
    CONNECTED,
    DISCONNECTED
};

// ── One decoded preset entry ──────────────────────────────────────────────
struct PresetInfo {
    uint8_t     id;
    std::string name;
};

// ── Shared state (written by BLE task, read by UI task) ───────────────────
// All fields except `mutex` itself must be accessed under the mutex.
// Dirty flags are set by the BLE task and cleared by the UI task after it
// has processed them.
struct BLESharedData {
    SemaphoreHandle_t       mutex;

    BLEStatus               status            = BLEStatus::IDLE;
    bool                    powerOn           = false;
    uint8_t                 activePresetId    = 0xFF;  // 0xFF = none
    std::vector<PresetInfo> presets;

    // Dirty flags
    bool statusChanged        = true;
    bool powerChanged         = false;
    bool activePresetChanged  = false;
    bool presetsChanged       = false;
};

extern BLESharedData g_ble;

// Initialise NimBLE and spawn the BLE management task.
void ble_init();

// Write the power state to the WLED controller (called from UI task).
// No-op if not connected.
void ble_write_power(bool on);

// Write a preset ID to the active-preset characteristic (called from UI task).
// No-op if not connected.
void ble_write_preset(uint8_t id);