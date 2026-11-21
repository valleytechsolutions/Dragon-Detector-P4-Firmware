# Dragon Detector for the Waveshare P4 Knob

This private checkpoint contains the board-specific source for Dragon Detector 0.14.8 on the Waveshare ESP32-P4-Knob-Touch-AMOLED-1.75X. The ESP32-P4 drives the 466 × 466 AMOLED, touch, rotary controls and audio. A separate onboard ESP32 receives Wi-Fi and Bluetooth over SDIO. [Board guide](docs/WAVESHARE-P4-KNOB-175.md)

## Firmware identity

| Item | Tested build |
| --- | --- |
| PlatformIO target | `waveshare_p4_knob` |
| ESP32-P4 silicon | v3.00 through v3.99 |
| Application image size | 4,068,576 bytes |
| SHA-256 of the complete app-only image | `b92948c7d3ed2ee415089ee9d64d8fddcb85e47e2e92c6a4c63cd8f2ef2873e0` |
| Build dependencies | ESP-IDF 5.5.5, Arduino-ESP32 3.3.12, esp-extconn 0.3.3 |
| Recovered shared application base | `bb6c9237f758651bfc0e71660bb87a8ad837db40` plus local P4 changes |

## Hardware verification

All 22 hardware checks passed on the knob. After restart the candidate radar plotted one classified BLE contact among 35 recent Wi-Fi and BLE contacts. The inspector retained 25 complete advertisements, including 15 with company IDs and seven with a recognized protocol. The last Wi-Fi survey contained 10 access points. The RTC was within one second of the host. All four removable-card resources matched their release hashes, and /audio, /music and /recordings were available. The radar uses passive Wi-Fi and BLE. Saved-network clock sync uses a Wi-Fi connection.

[Public web installer](https://valleytech-dragon-detector.pages.dev/#install) · [P4 microSD files](https://valleytech-dragon-detector.pages.dev/resources/p4-knob-sd-starter-0.14.8.zip) · [1.75C feature comparison](docs/P4-AND-AMOLED-175C.md)

## Source coverage

The receiver alternates passive Wi-Fi surveys and Bluetooth advertisement windows, or listens to either protocol on its own. The clock and timer pause surveying. The board-specific UI implements the radio modes, Wi-Fi channel display, BLE beacon summary, bottom-edge navigation and SD media browser. The board adapter provides the display, audio, touch, RTC, storage and knob interfaces. The SD audio source contains the recorder, WAV handling and MP3 playback integration.

The full application build also requires the matching private Dragon Detector shared source, assets and board libraries. These include the main loop, contact and classification logic, watch faces, LVGL screen renderer and SD services. This repository supplies the board-specific portion. The exact tested [application-only image](firmware.bin) is in this repository. The complete recovery archive is kept separately. The [shared-source patch](patches/p4-shared-detector.patch) applies to the private shared files from the previous e6b2035a P4 checkpoint. Overlay this repository’s board files alongside it. The contact-policy and label tests require that combined tree. The SDK and its runtime libraries are resolved from the versions in the board configuration. [Board configuration](hardware/waveshare-p4-knob-175)

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
