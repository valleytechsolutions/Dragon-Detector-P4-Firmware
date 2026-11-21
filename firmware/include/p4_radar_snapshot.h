#pragma once

#include <stdint.h>

#define P4_RADAR_MAX_APS 32
#define P4_RADAR_CHANNELS 14

typedef struct {
    uint8_t bssid[6];
    uint8_t ssid[32];
    uint8_t ssid_length;
    uint8_t channel;
    int8_t rssi;
} p4_radar_ap_t;

typedef struct {
    uint32_t finished_ms;
    uint16_t total;
    uint16_t count;
    uint16_t channel_counts[P4_RADAR_CHANNELS];
    int8_t channel_peaks[P4_RADAR_CHANNELS];
    p4_radar_ap_t aps[P4_RADAR_MAX_APS];
} p4_radar_wifi_snapshot_t;
