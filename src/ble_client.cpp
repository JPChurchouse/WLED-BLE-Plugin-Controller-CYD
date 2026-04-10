#include "ble_client.hpp"
#include "config.hpp"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

// ── Global shared state ───────────────────────────────────────────────────
BLESharedData g_ble;

// ── Module-private state ──────────────────────────────────────────────────
static NimBLEClient*               s_client      = nullptr;
static NimBLERemoteCharacteristic* s_chrPower    = nullptr;
static NimBLERemoteCharacteristic* s_chrPresets  = nullptr;
static NimBLERemoteCharacteristic* s_chrActive   = nullptr;
static NimBLEAdvertisedDevice*     s_targetDev   = nullptr;

static volatile bool s_doConnect     = false;
static volatile bool s_doScan        = false;
static uint32_t      s_lastReconnect = 0;

// ── Helpers ───────────────────────────────────────────────────────────────
static void setStatus(BLEStatus st) {
    if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY) == pdTRUE) {
        g_ble.status        = st;
        g_ble.statusChanged = true;
        xSemaphoreGive(g_ble.mutex);
    }
}

// ── Notification callbacks ────────────────────────────────────────────────
static void onPowerNotify(NimBLERemoteCharacteristic*, uint8_t* data,
                          size_t len, bool /*isNotify*/) {
    if (len < 1) return;
    if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY) == pdTRUE) {
        g_ble.powerOn      = (data[0] != 0x00);
        g_ble.powerChanged = true;
        xSemaphoreGive(g_ble.mutex);
    }
}

static void onActivePresetNotify(NimBLERemoteCharacteristic*, uint8_t* data,
                                 size_t len, bool /*isNotify*/) {
    if (len < 1) return;
    if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY) == pdTRUE) {
        g_ble.activePresetId      = data[0];
        g_ble.activePresetChanged = true;
        xSemaphoreGive(g_ble.mutex);
    }
}

// ── Scan callbacks ────────────────────────────────────────────────────────
class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        bool nameMatch = (dev->getName() == BLE_TARGET_NAME);
        bool uuidMatch = dev->isAdvertisingService(NimBLEUUID(BLE_SVC_UUID));
        if (nameMatch || uuidMatch) {
            Serial.printf("[BLE] Target found: %s\n",
                          dev->getAddress().toString().c_str());
            NimBLEDevice::getScan()->stop();
            s_targetDev = dev;
            s_doConnect = true;
            setStatus(BLEStatus::CONNECTING);
        }
    }
} s_scanCB;

// ── Client callbacks ──────────────────────────────────────────────────────
// NimBLE 1.4.x: onDisconnect takes only the client pointer.
// The disconnect reason is not surfaced at this API level in 1.4.x.
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* c) override {
        // Request faster connection parameters: interval 15–30 ms, no latency,
        // 4-second supervision timeout (units: 1.25 ms / 10 ms / 10 ms).
        c->updateConnParams(12, 24, 0, 400);
        Serial.println("[BLE] onConnect");
    }

    // ── Correct 1.4.x signature ───────────────────────────────────────────
    void onDisconnect(NimBLEClient* /*c*/) override {
        Serial.println("[BLE] Disconnected — scheduling rescan");
        s_chrPower   = nullptr;
        s_chrPresets = nullptr;
        s_chrActive  = nullptr;
        setStatus(BLEStatus::DISCONNECTED);
        s_lastReconnect = millis();
        s_doScan        = true;
    }
} s_clientCB;

// ── Connect + service discovery ───────────────────────────────────────────
static bool doConnect() {
    if (!s_client) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(&s_clientCB, /*deleteOnDisconnect=*/false);
    }

    Serial.println("[BLE] Connecting...");
    if (!s_client->connect(s_targetDev)) {
        Serial.println("[BLE] Connect failed");
        setStatus(BLEStatus::DISCONNECTED);
        s_lastReconnect = millis();
        s_doScan        = true;
        return false;
    }
    // MTU was already requested at the device level in ble_init().
    // Log the negotiated value for diagnostics.
    Serial.printf("[BLE] Negotiated MTU: %u\n", s_client->getMTU());

    // ── Service discovery ─────────────────────────────────────────────────
    NimBLERemoteService* svc = s_client->getService(BLE_SVC_UUID);
    if (!svc) {
        Serial.println("[BLE] Service not found");
        s_client->disconnect();
        return false;
    }

    s_chrPower   = svc->getCharacteristic(BLE_CHR_POWER_UUID);
    s_chrPresets = svc->getCharacteristic(BLE_CHR_PRESETS_UUID);
    s_chrActive  = svc->getCharacteristic(BLE_CHR_ACTIVE_UUID);

    if (!s_chrPower || !s_chrPresets || !s_chrActive) {
        Serial.println("[BLE] One or more characteristics missing");
        s_client->disconnect();
        return false;
    }

    // ── Subscribe to notifications ────────────────────────────────────────
    if (s_chrPower->canNotify())
        s_chrPower->subscribe(true, onPowerNotify);

    if (s_chrActive->canNotify())
        s_chrActive->subscribe(true, onActivePresetNotify);

    // ── Read initial Power state ──────────────────────────────────────────
    if (s_chrPower->canRead()) {
        std::string val = s_chrPower->readValue();
        if (!val.empty()) {
            if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY) == pdTRUE) {
                g_ble.powerOn      = (static_cast<uint8_t>(val[0]) != 0x00);
                g_ble.powerChanged = true;
                xSemaphoreGive(g_ble.mutex);
            }
        }
    }

    // ── Read initial Active Preset ────────────────────────────────────────
    if (s_chrActive->canRead()) {
        std::string val = s_chrActive->readValue();
        if (!val.empty()) {
            if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY) == pdTRUE) {
                g_ble.activePresetId      = static_cast<uint8_t>(val[0]);
                g_ble.activePresetChanged = true;
                xSemaphoreGive(g_ble.mutex);
            }
        }
    }

    // ── Read Preset List ──────────────────────────────────────────────────
    // NimBLE 1.4.x issues READ_BLOB_REQ automatically for values longer than
    // (MTU - 1) bytes, so this transparently reassembles fragmented reads.
    if (s_chrPresets->canRead()) {
        std::string json = s_chrPresets->readValue();
        Serial.printf("[BLE] Preset JSON (%u B): %s\n",
                      static_cast<unsigned>(json.size()), json.c_str());

        DynamicJsonDocument doc(2048);
        DeserializationError err = deserializeJson(doc, json);
        if (!err) {
            std::vector<PresetInfo> presets;
            for (JsonObject obj : doc.as<JsonArray>()) {
                PresetInfo p;
                p.id   = obj["id"].as<uint8_t>();
                p.name = obj["n"] | "?";
                presets.push_back(std::move(p));
            }
            Serial.printf("[BLE] Parsed %u presets\n",
                          static_cast<unsigned>(presets.size()));
            if (xSemaphoreTake(g_ble.mutex, portMAX_DELAY) == pdTRUE) {
                g_ble.presets        = std::move(presets);
                g_ble.presetsChanged = true;
                xSemaphoreGive(g_ble.mutex);
            }
        } else {
            Serial.printf("[BLE] JSON parse error: %s\n", err.c_str());
        }
    }

    setStatus(BLEStatus::CONNECTED);
    return true;
}

// ── BLE management task ───────────────────────────────────────────────────
static void ble_task(void* /*param*/) {
    while (true) {
        if (s_doConnect) {
            s_doConnect = false;
            doConnect();
        } else if (s_doScan) {
            if ((millis() - s_lastReconnect) >= BLE_RECONNECT_DELAY_MS) {
                s_doScan = false;
                setStatus(BLEStatus::SCANNING);
                Serial.println("[BLE] Starting scan...");
                NimBLEDevice::getScan()->start(0, nullptr, false);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ── Public API ────────────────────────────────────────────────────────────
void ble_init() {
    g_ble.mutex = xSemaphoreCreateMutex();
    configASSERT(g_ble.mutex);

    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P7);

    // ── Request preferred MTU at the device level (NimBLE 1.4.x API) ─────
    // The actual negotiated value depends on what the server accepts;
    // 517 is the BLE 5.x max for a single L2CAP packet.
    NimBLEDevice::setMTU(BLE_MTU);

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&s_scanCB, false);
    scan->setInterval(1349);
    scan->setWindow(449);
    scan->setActiveScan(true);

    s_doScan        = true;
    s_lastReconnect = 0;

    xTaskCreatePinnedToCore(
        ble_task, "ble_task",
        BLE_TASK_STACK_BYTES,
        nullptr,
        BLE_TASK_PRIORITY,
        nullptr,
        BLE_TASK_CORE
    );

    Serial.println("[BLE] Initialised");
}

void ble_write_power(bool on) {
    if (!s_chrPower || !s_client || !s_client->isConnected()) return;
    uint8_t val = on ? 0x01 : 0x00;
    s_chrPower->writeValue(&val, 1, false);
}

void ble_write_preset(uint8_t id) {
    if (!s_chrActive || !s_client || !s_client->isConnected()) return;
    s_chrActive->writeValue(&id, 1, false);
}