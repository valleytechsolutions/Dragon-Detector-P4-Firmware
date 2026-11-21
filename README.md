# Dragon Detector for the Waveshare P4 Knob

This private checkpoint contains the board-specific source for Dragon Detector 0.14.8 on the Waveshare ESP32-P4-Knob-Touch-AMOLED-1.75X. The ESP32-P4 drives the 466 × 466 AMOLED, touch, rotary controls and audio. A separate onboard ESP32 receives Wi-Fi and Bluetooth over SDIO. [Board guide](docs/WAVESHARE-P4-KNOB-175.md)

## Firmware identity

| Item | Tested build |
| --- | --- |
| PlatformIO target | `waveshare_p4_knob` |
| ESP32-P4 silicon | v3.00 through v3.99 |
| Application image size | 4,056,544 bytes |
| SHA-256 of the complete app-only image | `e6b2035a51913ff56228956f6ce908519bd777833bca285d11c2249ca6b53843` |
| Build dependencies | ESP-IDF 5.5.5, Arduino-ESP32 3.3.12, esp-extconn 0.3.3 |
| Recovered shared application base | `bb6c9237f758651bfc0e71660bb87a8ad837db40` plus local P4 changes |

## Hardware verification

The exact application image above passed all 22 startup checks on the P4 knob. The radio observed 10 Wi-Fi access points and received Bluetooth advertisements. The saved scanner preference was Both. The RTC reported valid time. The clock and stopwatch both paused the survey and the survey resumed afterward. The Evil menu’s Wi-Fi Channel Visualizer and BLE Beacon Scan summarize received APs and advertisements. BLE Lab shows observed devices. The passive radar uses Wi-Fi, Bluetooth or both. A saved-network clock sync can join Wi-Fi for NTP. Raw-frame tooling, active GATT and beacon emulation, CSI Room Motion and ESP-NOW nodes are unavailable in this build.

## Source coverage

The receiver alternates passive Wi-Fi surveys and Bluetooth advertisement windows, or listens to either protocol on its own. The clock and timer pause surveying. The board-specific UI implements the radio modes, Wi-Fi channel display, BLE beacon summary, bottom-edge navigation and SD media browser. The board adapter provides the display, audio, touch, RTC, storage and knob interfaces. The SD audio source contains the recorder, WAV handling and MP3 playback integration.

The full application build also requires the matching private Dragon Detector shared source, assets and board libraries. These include the main loop, contact and classification logic, watch faces, LVGL screen renderer and SD services. This repository supplies the board-specific portion. The exact tested [application-only image](firmware.bin) is in this repository. The complete recovery archive is kept separately. The SDK and its runtime libraries are resolved from the versions in the board configuration. [Board configuration](hardware/waveshare-p4-knob-175)

## Running the host tests

Install the Python `ziglang` package in a local environment, then run the following from the repository root.

```sh
python tools/test_p4_passive_rx.py
python tools/test_p4_ble_lifecycle.py
```

The Wi-Fi test exercises the production reader with empty, full, malformed and failed driver responses. The Bluetooth test exercises the production GAP handler through 64 cancellation and resume cycles, a missing completion, stop and acknowledgement errors, passive reports and beacon classification. Both tests use synthetic observations and run without a radio.

## Building a complete checkout

Use the matching private shared application tree. The board-specific files use their original source paths. The target section recorded in `hardware/waveshare-p4-knob-175/platformio-section.ini` belongs in the full project’s `platformio.ini` and inherits its common settings. With that checkout and a dedicated PlatformIO core directory, run `pio run -e waveshare_p4_knob`. The P4 configuration fixes the published image header to the supported silicon range. Build timestamps are embedded, so the image digest identifies the original build.

## Credits

[Project license](LICENSE) · [Board, driver and dependency notices](CREDITS.md) · [File checksums](SOURCE-SHA256.txt)
