#pragma once
#ifdef BOARD_WAVESHARE_P4_KNOB
#include "p4_radar_snapshot.h"
#include "p4_ble_passive.h"
const char* p4RadarStatus();
bool p4RadarBluetoothAvailable();
uint8_t p4RadarScanMode();
bool p4RadarSetScanMode(unsigned mode);
bool p4RadarProfileSupported(unsigned mode);
uint32_t p4RadarIbeaconReports();
uint32_t p4RadarEddystoneReports();
const char* p4RadarBluetoothStatus();
bool p4RadarBleListening();
uint32_t p4RadarBleWindows();
uint32_t p4RadarBleReports();
bool p4RadarTakeBleSnapshot(p4_ble_snapshot_t& out);
uint16_t p4RadarLastApCount();
uint32_t p4RadarCompletedScans();
void p4RadarSetEnabled(bool enabled);
bool p4RadarRetry();
bool p4RadarChannelSummary(uint16_t counts[P4_RADAR_CHANNELS], int8_t strongest[P4_RADAR_CHANNELS], uint32_t* completedMs);
bool p4RadarAcquireClockNetwork(uint32_t timeoutMs=5000);
void p4RadarReleaseClockNetwork();
bool p4RadarTakeSnapshot(p4_radar_wifi_snapshot_t& out);
#endif
