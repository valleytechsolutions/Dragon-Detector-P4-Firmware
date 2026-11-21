#ifdef BOARD_WAVESHARE_P4_KNOB
#include "app.h"
#include "p4_radio.h"
#include "p4_radar_scan.h"
#include <atomic>
#include <Preferences.h>
#include <esp_event.h>
#include <esp_extconn.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <driver/gpio.h>
#include <sd_pwr_ctrl_by_on_chip_ldo.h>

bool p4RadioRailReady();

namespace {
enum class State : uint8_t { Starting, Listening, Ready, Empty, Paused, ScanError, Unavailable };
const char* const tag = "dragon_radio";
std::atomic<State> state{State::Starting};
std::atomic<uint8_t> scanMode{0};
Preferences radioPrefs;
bool prefsReady=false;
std::atomic<bool> enabled{true}, transportReady{false}, stationOn{false}, parked{false}, clockLease{false};
std::atomic<uint32_t> generation{0}, scans{0}, failures{0}, sampled{0};
std::atomic<uint16_t> lastCount{0};
std::atomic<esp_err_t> lastError{ESP_OK};
std::atomic<const char*> startupStage{"idle"};
std::atomic<bool> queueCreated{false}, workerCreated{false};
uint32_t internalFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT); }
uint32_t internalLargest() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT); }
uint32_t dmaFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA); }
uint32_t dmaLargest() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA); }
void heapReport(const char* stage,esp_err_t err) {
    ESP_LOGI(tag,"init stage=%s result=%s internal=%u largest=%u dma=%u dma_largest=%u",
        stage,esp_err_to_name(err),unsigned(internalFree()),unsigned(internalLargest()),unsigned(dmaFree()),unsigned(dmaLargest()));
}
void enterStage(const char* stage) {
    startupStage.store(stage);
    heapReport(stage,ESP_OK);
}
std::atomic<TaskHandle_t> task{nullptr};
std::atomic<bool> bleReady{false}, bleListening{false};
std::atomic<uint32_t> bleWindows{0}, bleReports{0}, ibeaconReports{0}, eddystoneReports{0};
std::atomic<esp_err_t> bleError{ESP_OK};
std::atomic<uint32_t> bleFaults{0},bleRecovered{0};
std::atomic<const char*> bleFaultStage{"none"};
std::atomic<bool> retryBleNow{false};
uint32_t bleRetryAt=0;
unsigned bleFaultStreak=0;
struct BleResult { uint32_t generation; p4_ble_snapshot_t scan; };
struct BleBuffers {
    BleResult work,pending;
    alignas(4) uint8_t queueStorage[sizeof(BleResult)];
};
BleBuffers* bleBuffers=nullptr;
StaticQueue_t bleQueueControl;
QueueHandle_t bleCompleted=nullptr;
uint32_t bleGeneration=0;

bool wantBle() {
    return scanMode.load()!=1 && enabled.load() && !clockLease.load() && generation.load()==bleGeneration;
}

enum class BleStartup : uint8_t { Idle, Starting, Ready, Failed };
std::atomic<BleStartup> bleStartup{BleStartup::Idle};
uint32_t bleStartupAt=0;

void startBleHost(void*) {
    heapReport("bluedroid_begin",ESP_OK);
    const esp_err_t result=p4_ble_listen_init();
    heapReport("bluedroid_end",result);
    BleStartup starting=BleStartup::Starting;
    if (result==ESP_OK) {
        if (bleStartup.compare_exchange_strong(starting,BleStartup::Ready)) {
            bleReady.store(true);
        }
    } else if (bleStartup.compare_exchange_strong(starting,BleStartup::Failed)) {
        bleError.store(result);
        ESP_LOGW(tag,"Passive Bluetooth startup: %s",esp_err_to_name(result));
    }
    vTaskDelete(nullptr);
}

void receiveBle(uint32_t request) {
    bleGeneration=request;
    if (!wantBle() || !bleCompleted) return;
    // Bluedroid initialization can block inside the vendor HCI transport.
    // A late completion must not stop Wi-Fi, hold the NTP lease or start a scan.
    if (bleStartup.load()==BleStartup::Idle) {
        bleStartup.store(BleStartup::Starting);
        heapReport("bluedroid_worker",ESP_OK);
        bleStartupAt=millis();
        if (xTaskCreate(startBleHost,"p4_ble_init",4608,nullptr,3,nullptr)!=pdPASS) {
            bleError.store(ESP_ERR_NO_MEM);
            bleStartup.store(BleStartup::Failed);
        }
        return;
    }
    if (bleStartup.load()==BleStartup::Starting && millis()-bleStartupAt>=7000) {
        BleStartup starting=BleStartup::Starting;
        if (bleStartup.compare_exchange_strong(starting,BleStartup::Failed)) {
            bleError.store(ESP_ERR_TIMEOUT);
            heapReport("bluedroid_timeout",ESP_ERR_TIMEOUT);
            ESP_LOGW(tag,"Passive Bluetooth startup timed out; continuing Wi-Fi");
        }
    }
    if (!bleReady.load() || !wantBle()) return;
    const bool retryNow=retryBleNow.exchange(false);
    if (!retryNow && bleFaultStreak && int32_t(millis()-bleRetryAt)<0) return;
    bleListening.store(true);
    const esp_err_t err=p4_ble_listen_window(&bleBuffers->work.scan,wantBle);
    bleListening.store(false);
    if (err==ESP_ERR_INVALID_STATE && !wantBle()) return;
    bleError.store(err);
    if (err!=ESP_OK) {
        ++bleFaults;
        bleFaultStage.store(p4_ble_window_stage());
        if (++bleFaultStreak==1)
            ESP_LOGW(tag,"Passive Bluetooth window %s: %s",bleFaultStage.load(),esp_err_to_name(err));
        const unsigned shift=bleFaultStreak>4?3:bleFaultStreak-1;
        bleRetryAt=millis()+(1000u<<shift);
        return;
    }
    if (bleFaultStreak) {
        bleFaultStreak=0;
        ++bleRecovered;
        ESP_LOGI(tag,"Passive Bluetooth discovery resumed");
    }
    if (!wantBle()) return;
    bleBuffers->work.generation=request;
    xQueueOverwrite(bleCompleted,&bleBuffers->work);
    bleReports.fetch_add(bleBuffers->work.scan.reports);
    ibeaconReports.fetch_add(bleBuffers->work.scan.ibeacon_reports);
    eddystoneReports.fetch_add(bleBuffers->work.scan.eddystone_reports);
    if (!bleWindows.fetch_add(1))
        ESP_LOGI(tag,"Passive Bluetooth window complete: %lu reports",(unsigned long)bleBuffers->work.scan.reports);
}
QueueHandle_t completed=nullptr;
struct Result { uint32_t generation; p4_radar_wifi_snapshot_t scan; };
Result work{},pending{};

const char *name(State s) {
    switch (s) {
    case State::Starting: return "Starting Wi-Fi";
    case State::Listening: return "Listening for Wi-Fi";
    case State::Ready: return "Passive Wi-Fi";
    case State::Empty: return "No Wi-Fi APs heard";
    case State::Paused: return "Radar paused";
    case State::ScanError: return "Wi-Fi scan error";
    default: return "Radio unavailable";
    }
}

void fail(esp_err_t err) {
    lastError.store(err);
    state.store(State::Unavailable);
    heapReport(startupStage.load(),err);
    ESP_LOGE(tag, "Passive radio unavailable: %s", esp_err_to_name(err));
}

esp_err_t startTransport() {
    enterStage("shared_rail");
    if (!p4RadioRailReady()) return ESP_ERR_INVALID_STATE;
    // Slot 0 (TF) stays mounted. Host-clock initialization on slot 1 is
    // exclusive with card I/O. Once initialized, SDMMC serializes transactions.
    enterStage("sd_lock");
    if (!sdMutex || xSemaphoreTake(sdMutex,pdMS_TO_TICKS(5000))!=pdTRUE) return ESP_ERR_TIMEOUT;
    gpio_hold_dis(GPIO_NUM_54);
    esp_extconn_config_t config=ESP_EXTCONN_CONFIG_DEFAULT();
    enterStage("esp_extconn_init");
    esp_err_t err=esp_extconn_init(&config);
    xSemaphoreGive(sdMutex);
    if (err!=ESP_OK) return err;
    enterStage("esp_netif_init");
    err=esp_netif_init();
    if (err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return err;
    enterStage("event_loop");
    err=esp_event_loop_create_default();
    if (err!=ESP_OK && err!=ESP_ERR_INVALID_STATE) return err;
    wifi_init_config_t wifi=WIFI_INIT_CONFIG_DEFAULT();
    wifi.nvs_enable=0;
    wifi.static_rx_buf_num=8;
    wifi.dynamic_rx_buf_num=16;
    enterStage("esp_wifi_init");
    err=esp_wifi_init(&wifi);
    if (err==ESP_OK) {
        enterStage("esp_wifi_set_storage");
        err=esp_wifi_set_storage(WIFI_STORAGE_RAM);
    }
    if (err==ESP_OK) {
        enterStage("esp_wifi_set_mode");
        err=esp_wifi_set_mode(WIFI_MODE_STA);
    }
    return err;
}

void receive(void*) {
    esp_err_t err=startTransport();
    if (err!=ESP_OK) {
        fail(err);
        task.store(nullptr);
        vTaskDelete(nullptr);
        return;
    }
    task.store(xTaskGetCurrentTaskHandle());
    enterStage("ready");
    transportReady.store(true);
    ESP_LOGI(tag,"SDIO transport ready; starting passive discovery");
    bool lastFailed=false;
    for (;;) {
        if (!enabled.load() || clockLease.load()) {
            if (stationOn.load()) {
                err=esp_wifi_stop();
                if (err!=ESP_OK && err!=ESP_ERR_WIFI_NOT_STARTED) {
                    lastError.store(err);
                    state.store(State::ScanError);
                    ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
                    continue;
                }
                stationOn.store(false);
                ESP_LOGI(tag,"Passive Wi-Fi paused; station stopped");
            }
            if (p4_ble_listen_quiesce()!=ESP_OK) {
                // The clock can claim exclusive radio ownership only after GAP is idle.
                ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
                continue;
            }
            state.store(State::Paused);
            parked.store(true);
            ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
            continue;
        }
        parked.store(false);
        const uint32_t requested=generation.load();
        if (scanMode.load()==2) {
            if (stationOn.load()) {
                err=esp_wifi_stop();
                if (err!=ESP_OK && err!=ESP_ERR_WIFI_NOT_STARTED) {
                    lastError.store(err);
                    state.store(State::ScanError);
                    ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
                    continue;
                }
                stationOn.store(false);
            }
            if (enabled.load() && !clockLease.load() && generation.load()==requested) {
                state.store(State::Ready);
                receiveBle(requested);
            }
            if (enabled.load() && !clockLease.load() && generation.load()==requested)
                ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(bleReady.load()?350:1000));
            continue;
        }
        if (!stationOn.load()) {
            err=esp_wifi_set_mode(WIFI_MODE_STA);
            if (err==ESP_OK) err=esp_wifi_start();
            stationOn.store(err==ESP_OK);
        }
        if (stationOn.load()) {
            state.store(State::Listening);
            err=p4_read_passive_radar(&work.scan);
        }
        if (!enabled.load() || clockLease.load() || generation.load()!=requested) continue;
        lastError.store(err);
        if (err==ESP_OK) {
            work.generation=requested;
            xQueueOverwrite(completed,&work);
            lastCount.store(work.scan.total);
            sampled.fetch_add(work.scan.count);
            const uint32_t previous=scans.fetch_add(1);
            state.store(work.scan.total ? State::Ready : State::Empty);
            if (!previous || lastFailed)
                ESP_LOGI(tag,"Passive Wi-Fi scan complete: %u APs",unsigned(work.scan.total));
            lastFailed=false;
        } else {
            failures.fetch_add(1);
            state.store(State::ScanError);
            if (!lastFailed) ESP_LOGW(tag,"Passive Wi-Fi scan failed: %s",esp_err_to_name(err));
            lastFailed=true;
        }
        // Time-share the companion. Stop the station before listening on BLE.
        // A missing Bluetooth controller must not interrupt subsequent Wi-Fi scans.
        if (scanMode.load()!=1 && enabled.load() && !clockLease.load() && generation.load()==requested) {
            esp_err_t stopped=stationOn.load()?esp_wifi_stop():ESP_OK;
            if (stopped==ESP_OK || stopped==ESP_ERR_WIFI_NOT_STARTED) {
                stationOn.store(false);
                receiveBle(requested);
            }
        }
        if (enabled.load() && !clockLease.load() && generation.load()==requested)
            ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(err==ESP_OK?1500:5000));
    }
}
} // namespace

// Arduino's dependency graph may include esp_hosted, whose constructor otherwise
// initializes the same slot as extconn. No hosted session exists on this model.
extern "C" int __wrap_esp_hosted_init(void) { return 0; }
extern "C" esp_err_t __wrap_esp_wifi_80211_tx(wifi_interface_t,const void*,int,bool) {
    return ESP_ERR_NOT_SUPPORTED;
}

// The companion supplies infrastructure scans and does not implement CSI.
extern "C" esp_err_t __wrap_esp_wifi_set_csi_rx_cb(wifi_csi_cb_t,void*) {
    return ESP_ERR_NOT_SUPPORTED;
}
extern "C" esp_err_t __wrap_esp_wifi_set_csi(bool) { return ESP_ERR_NOT_SUPPORTED; }
extern "C" esp_err_t __wrap_esp_wifi_set_csi_config(const wifi_csi_config_t*) {
    return ESP_ERR_NOT_SUPPORTED;
}

// Compiled into extconn only via its component compile definition. A null
// power-control handle selects fixed SDIO voltage in IDF. Slot 0 and the P4
// board initialization already hold the shared supply at 3.3 V.
extern "C" esp_err_t dragon_radio_fixed_rail(const sd_pwr_ctrl_ldo_config_t* config,
                                             sd_pwr_ctrl_handle_t* handle) {
    if (!config || !handle || config->ldo_chan_id!=4 || !p4RadioRailReady())
        return ESP_ERR_INVALID_ARG;
    *handle=nullptr;
    return ESP_OK;
}

const char* p4RadarStatus() { if (clockLease.load()) return "Wi-Fi clock sync";
    if (state.load()==State::Unavailable) return name(State::Unavailable);
    if (!enabled.load()) return name(State::Paused);
    if (scanMode.load()==2) {
        if (!transportReady.load()) return "Starting radio";
        if (bleError.load()!=ESP_OK) return bleReady.load()?"Bluetooth retrying":"Bluetooth unavailable";
        if (!bleReady.load()) return "Starting Bluetooth";
        return bleListening.load()?"Listening for Bluetooth":"Passive Bluetooth";
    }
    if (bleListening.load()) return "Listening for Bluetooth";
    return name(state.load()); }
bool p4RadarProfileSupported(unsigned mode) { return mode<=3; }
uint8_t p4RadarScanMode() { return scanMode.load(); }
bool p4RadarSetScanMode(unsigned mode) {
    if (mode>2) return false;
    if (scanMode.load()==mode) return true;
    if (!prefsReady || radioPrefs.putUChar("scan_mode",uint8_t(mode))!=1) return false;
    scanMode.store(uint8_t(mode));
    generation.fetch_add(1);
    TaskHandle_t handle=task.load();
    if (handle) xTaskNotifyGive(handle);
    return true;
}
bool p4RadarBluetoothAvailable() { return bleReady.load(); }
const char* p4RadarBluetoothStatus() {
    if (state.load()==State::Unavailable) return "Unavailable";
    if (!enabled.load() || clockLease.load()) return "Paused";
    if (scanMode.load()==1) return "Off in Wi-Fi mode";
    if (bleError.load()!=ESP_OK) return bleReady.load()?"Retrying passive scan":"Unavailable";
    if (!bleReady.load()) return "Waiting for first scan";
    return bleListening.load()?"Listening":"Passive";
}
bool p4RadarBleListening() { return bleListening.load(); }
uint32_t p4RadarBleWindows() { return bleWindows.load(); }
uint32_t p4RadarBleReports() { return bleReports.load(); }
uint32_t p4RadarIbeaconReports() { return ibeaconReports.load(); }
uint32_t p4RadarEddystoneReports() { return eddystoneReports.load(); }
bool p4RadarTakeBleSnapshot(p4_ble_snapshot_t& out) {
    if (!bleCompleted || xQueueReceive(bleCompleted,&bleBuffers->pending,0)!=pdTRUE ||
        !enabled.load() || clockLease.load() || bleBuffers->pending.generation!=generation.load()) return false;
    out=bleBuffers->pending.scan;
    return true;
}
uint16_t p4RadarLastApCount() { return lastCount.load(); }
uint32_t p4RadarCompletedScans() { return scans.load(); }
bool p4CoprocReady() { return transportReady.load(); }
bool p4CoprocBusy() { return state.load()==State::Starting || clockLease.load(); }
const char* p4CoprocStateName() { return p4RadarStatus(); }

void p4CoprocStart() {
    if (completed) return;
    prefsReady=radioPrefs.begin("dd-p4-radio",false);
    const uint8_t saved=prefsReady?radioPrefs.getUChar("scan_mode",0):0;
    scanMode.store(saved<=2?saved:0);
    // Other UI services can ask for a netif before radio startup finishes.
    enterStage("early_event_loop");
    esp_err_t events=esp_event_loop_create_default();
    if (events!=ESP_OK && events!=ESP_ERR_INVALID_STATE) { fail(events); return; }
    enterStage("result_queue");
    completed=xQueueCreate(1,sizeof(Result));
    queueCreated.store(completed!=nullptr);
    // Keep advertisement batches out of the internal DMA pool needed by SDIO.
    bleBuffers=static_cast<BleBuffers*>(heap_caps_calloc(1,sizeof(BleBuffers),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if (bleBuffers)
        bleCompleted=xQueueCreateStatic(1,sizeof(BleResult),bleBuffers->queueStorage,&bleQueueControl);
    if (!bleCompleted) bleError.store(ESP_ERR_NO_MEM);
    if (!completed) { fail(ESP_ERR_NO_MEM); return; }
    enterStage("worker");
    const bool created=xTaskCreate(receive,"p4_rx",8192,nullptr,4,nullptr)==pdPASS;
    workerCreated.store(created);
    if (!created) fail(ESP_ERR_NO_MEM);
}
void p4RadarSetEnabled(bool value) {
    if (enabled.exchange(value)==value) return;
    generation.fetch_add(1);
    if (value && !clockLease.load()) parked.store(false);
    if (!value && transportReady.load()) state.store(State::Paused);
    TaskHandle_t handle=task.load();
    if (handle) xTaskNotifyGive(handle);
}
bool p4RadarRetry() {
    TaskHandle_t handle=task.load();
    if (!handle || !enabled.load() || !transportReady.load() || clockLease.load()) return false;
    retryBleNow.store(true);
    xTaskNotifyGive(handle);
    return true;
}
bool p4RadarAcquireClockNetwork(uint32_t timeoutMs) {
    TaskHandle_t handle=task.load();
    if (!handle || !transportReady.load()) return false;
    bool available=false;
    if (!clockLease.compare_exchange_strong(available,true)) return false;
    generation.fetch_add(1);
    xTaskNotifyGive(handle);
    const uint32_t start=millis();
    while (!parked.load()) {
        if (millis()-start>=timeoutMs) {
            p4RadarReleaseClockNetwork();
            return false;
        }
        delay(10);
    }
    ESP_LOGI(tag,"Station released to clock sync");
    return true;
}

void p4RadarReleaseClockNetwork() {
    if (!clockLease.exchange(false)) return;
    generation.fetch_add(1);
    if (enabled.load()) parked.store(false);
    TaskHandle_t handle=task.load();
    if (handle) xTaskNotifyGive(handle);
}

bool p4RadarTakeSnapshot(p4_radar_wifi_snapshot_t& out) {
    if (!completed || xQueueReceive(completed,&pending,0)!=pdTRUE || !enabled.load() || clockLease.load() ||
        pending.generation!=generation.load()) return false;
    out=pending.scan;
    return true;
}
void p4CoprocStatus() {
    Serial.printf("P4RADIO_MEMORY stage=%s queue=%u worker=%u internal=%u internal_largest=%u dma=%u dma_largest=%u\n",
        startupStage.load(),unsigned(queueCreated.load()),unsigned(workerCreated.load()),
        unsigned(internalFree()),unsigned(internalLargest()),unsigned(dmaFree()),unsigned(dmaLargest()));
    Serial.printf("P4RADIO_STATUS state=\"%s\" transport_ready=%u station_on=%u requested=%u clock_lease=%u scans=%lu last_aps=%u samples=%lu failures=%lu error=%s wifi_passive=1 mode=%u ble=%u ble_listening=%u ble_windows=%lu ble_reports=%lu ibeacon=%lu eddystone=%lu ble_error=%s ble_faults=%lu ble_recovered=%lu ble_last_fault=%s raw=0\n",
        p4RadarStatus(),unsigned(transportReady.load()),unsigned(stationOn.load()),unsigned(enabled.load()),unsigned(clockLease.load()),
        (unsigned long)scans.load(),unsigned(lastCount.load()),(unsigned long)sampled.load(),
        (unsigned long)failures.load(),esp_err_to_name(lastError.load()),
        unsigned(scanMode.load()),unsigned(bleReady.load()),unsigned(bleListening.load()),(unsigned long)bleWindows.load(),
        (unsigned long)bleReports.load(),(unsigned long)ibeaconReports.load(),(unsigned long)eddystoneReports.load(),esp_err_to_name(bleError.load()),
        (unsigned long)bleFaults.load(),(unsigned long)bleRecovered.load(),bleFaultStage.load());
}
void p4CoprocCommand(bool install) {
    if (install) Serial.println("P4RADIO: onboard radio runtime is loaded into RAM at boot");
    p4CoprocStatus();
}
bool p4CoprocSleep() {
    if (clockLease.load()) return false;
    const bool restore=enabled.load();
    p4RadarSetEnabled(false);
    const uint32_t start=millis();
    while (transportReady.load() && !parked.load() && millis()-start<5000) delay(20);
    if (transportReady.load() && !parked.load()) { p4RadarSetEnabled(restore); return false; }
    gpio_set_direction(GPIO_NUM_54,GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_54,0);
    gpio_hold_en(GPIO_NUM_54);
    return true;
}
#endif
