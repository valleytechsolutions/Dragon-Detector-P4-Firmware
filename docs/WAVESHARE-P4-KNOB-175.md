# Waveshare P4 knob (1.75-inch AMOLED)

Dragon Detector runs on the ESP32-P4. The onboard ESP32 supplies the radio over SDIO. This build loads the radio runtime into RAM at startup. The TF card and radio use separate SDMMC slots and share the board's 3.3 V supply. [Waveshare board documentation](https://docs.waveshare.com/ESP32-P4-Knob-Touch-AMOLED-1.75X)

## Radar and standby

Open **Utilities → Activity → Radio Activity** to see the number of completed Wi-Fi scans and the number of access points heard in the most recent scan. A count of zero after a completed scan means the radio listened and heard no access points. A startup or scan error has its own message.

Clock and stopwatch put the survey into standby to conserve power. Going back to radar resumes listening if the radio was enabled before opening the clock or timer. The hardware RTC keeps time independently of the radar.

## Controls and SD audio

Swipe up from the bottom of the radar to open Command. On the clock and other pages, the same swipe goes back one screen. Turn the knob clockwise or counterclockwise on the clock to change watch faces. **Utilities → Activity** contains Voice Recorder, Music Library, Radio Activity and History. Put personal MP3s in `/music` on the TF card. Existing audio in `/audio` is available through the media browser.

## Feature comparison

| Feature | Waveshare P4 knob development profile |
| --- | --- |
| Clock, badge and timers | Uses the shared 466-pixel watch faces, personal badge storage, stopwatch and locksport timer. The board has a PCF85063 RTC. |
| Wi-Fi radar and Hunt | Updates dots and signal strength from passive access-point surveys. Tap a dot to inspect it. |
| Modes | Select the radio to listen to independently of the detector family shown on the radar. A name or vendor match carries the confidence of the underlying evidence. |
| BLE Lab | Passive discovery and observed advertisements use the ESP32 coprocessor. |
| Evil-M5Project instruments | Wi-Fi Channel Visualizer shows counts and strongest signal per channel. BLE Beacon Scan summarizes recognized advertisements. Both use the current radio observations. |
| Storage and audio | The knob has removable TF storage, a microphone, a speaker and a 3.5 mm jack. Music Library and Voice Recorder are under Utilities. |
| Driver gaps | The P4 port currently lacks raw 802.11 frame capture, CSI Room Motion, and the ESP-NOW link required for Biscuit/JCMK node mode. |

The P4 and the 32 MB Waveshare 1.75C both provide two 6 MiB application slots. They use different radio drivers. Internal flash capacity alone does not establish feature compatibility.

## USB diagnostics

The board routes the USB-C connector to either the native USB interface or its serial bridge depending on plug orientation. The native Dragon Detector console accepts `radio chip status`. The serial bridge carries the boot log. On a running radar, the `scans` field should advance and `transport_ready` should read 1. In clock standby the station stops and completed scans stop advancing. The `ble` and `raw` fields indicate the capabilities of the running build.
