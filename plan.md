# Port `lightbar2mqtt` to an ESPHome external component

## Problem

[`ebinf/lightbar2mqtt`](https://github.com/ebinf/lightbar2mqtt) (MIT, Arduino sketch) controls a
Xiaomi Mi Computer Monitor Light Bar (MJGJD01YL) over an nRF24L01 radio and bridges it to
Home Assistant via hand-rolled WiFi + PubSubClient + MQTT discovery JSON.

We want the same functionality as a **native ESPHome external component**, so that WiFi, OTA,
logging, the HA API and entity discovery are handled by ESPHome. The nRF24L01 SPI driver is
**not** re-implemented: we consume [`loucks1/nrf24_espidf`](https://github.com/loucks1/nrf24_espidf)
as an unmodified external component.

## Approach

Three layers:

1. **Hub** `xiaomi_lightbar:` — owns protocol encode/decode, CRC16, per-serial sequence counters,
   a non-blocking TX repeat queue, and RX dispatch. Talks to the `nrf24` component via its
   public C++ API (`stop_listening`, `write`, `add_on_data_callback`, …).
2. **Light** `light: platform: xiaomi_lightbar` — one entity per light bar serial. Optimistic
   `COLOR_TEMPERATURE` light, brightness scale 15, mireds 153–370.
3. **Remote** `event: platform: xiaomi_lightbar` — one entity per remote serial, 6 event types
   plus per-action YAML triggers. Buttons (`pair`, `toggle_internal_state`) and a templatable
   `xiaomi_lightbar.send_command` action complete parity with the MQTT topics.

Everything that was MQTT-topic-shaped in the original disappears; ESPHome's native API replaces it.

### Target layout

```
/Users/zweckjosef/dev/esphome-xiaomi-lightbar/
├── README.md                 wiring, YAML reference, migration notes, attribution
├── LICENSE                   MIT (matching upstream lightbar2mqtt)
├── .gitignore
├── example.yaml              full working config (ESP32 + nrf24 + light + event + buttons)
└── components/
    └── xiaomi_lightbar/
        ├── __init__.py       hub schema + `send_command` action codegen
        ├── crc16.h           CRC16 poly 0x1021 / init 0xFFFE (replaces RobTillaart CRC16 lib)
        ├── xiaomi_lightbar.h/.cpp   XiaomiLightbarHub + XiaomiLightbarRemote
        ├── automation.h      triggers + SendCommandAction
        ├── light.py          + lightbar_light.h/.cpp   (XiaomiLightbarLight)
        ├── event.py          + lightbar_event.h/.cpp   (XiaomiLightbarEvent = remote)
        └── button.py         + lightbar_button.h/.cpp  (pair / toggle_internal_state)
```

### Protocol facts carried over verbatim (from `radio.cpp` / `lightbar.cpp`)

- 17-byte frame: `[0..7] preamble {53 39 14 DD 1C 49 34 12}`, `[8..10] serial (BE)`,
  `[11] 0xFF`, `[12] sequence`, `[13] command`, `[14] options`, `[15..16] CRC16 (BE)`.
- CRC16: poly `0x1021`, init `0xFFFE`, xorout `0x0000`, no input/output reflection, over bytes 0–14.
- Radio: TX addr `0x5555555555`, RX pipe 0 addr `0xAAAAAAAAAA`, address width 5, channel **68**,
  **2 MBPS**, **CRC disabled**, **auto-ack off**, payload size 17, retries(15,15).
- TX = same frame written **20×** with ~10 ms spacing, NO_ACK (`multicast = true`).
- RX de-shift (5-bit realignment, see lamperez's baseband notes):
  `data[0] = 0x50 | raw[0]>>5`, and for `i>0`
  `data[i] = ((raw[i-1]>>1 & 0x0F)<<4) | ((raw[i-1]&0x01)<<3) | (raw[i]>>5)`.
- Commands: `ON_OFF 0x01, COOLER 0x02, WARMER 0x03, BRIGHTER 0x04, DIMMER 0x05, RESET 0x06`.
- Absolute brightness = `DIMMER 0xF0` (i.e. `0x00-16`) then `BRIGHTER <0..15>`.
  Absolute temperature = `COOLER 0xF0` then `WARMER <0..15>`.
  Mireds → step: `round((1 - (m-153)/(370-153)) * 15)`, clamped 153..370.
- Remote action mapping (from `mqtt.cpp::sendAction`):
  `ON_OFF→press`, `BRIGHTER→turn_clockwise`, `DIMMER→turn_counterclockwise`,
  `COOLER→press_turn_clockwise`, `WARMER→press_turn_counterclockwise`, `RESET→hold`.

## Known risks / decisions

- **Blocking TX.** Original does `20 × (write + delay(10))` ≈ 200 ms+ inline. That trips ESPHome's
  50 ms loop warning. → implement a small TX job queue drained from `loop()` with `millis()`
  spacing; `stop_listening()` at job start, `start_listening()` when the repeat count is exhausted.
  If `NRF24Component::write()`'s 95 ms internal timeout is ever hit, fall back to
  `start_write()` + non-blocking status polling.
- **Radio config ownership.** `nrf24:` YAML sets channel/data-rate/PA, and its 15 s watchdog
  `soft_reset()` re-applies *only* those three — not CRC-off, auto-ack-off or payload size.
  → the hub asserts the full lightbar radio profile in `setup()` (with
  `setup_priority = DATA - 1.0f` so it runs after `nrf24`) and cheaply re-asserts it when it
  detects the chip was reset (e.g. `get_channel() != channel`).
- **RX callback length.** `NRF24Component::loop()` only reads when `is_listening_` and a data
  callback is registered, and passes `get_payload_size()` (= 17) bytes. Matches the original
  (whose 18-byte buffer was clamped to 17 by RF24 anyway). Verify `open_reading_pipe(uint8_t)`
  /`open_writing_pipe(uint64_t)` byte order matches RF24's LSB-first behaviour.
- **Upstream dedup bug.** `radio.cpp` compares `package_id` against `package_ids[i].serial`
  instead of `.package_id`. We port the *intent* — reject a repeat of the last seen sequence
  number, wrap-aware — and document the deliberate fix in the README.
- **Python version.** ESPHome does not support Python 3.14 (system default here).
  Build the venv with `/opt/homebrew/bin/python3.13`.
- `nrf24_espidf` stays **unmodified** and is referenced as a git `external_components` source.

## Todos

1. `scaffold` — create repo dir, `.gitignore`, MIT `LICENSE`, `git init`.
2. `hub-core` — `crc16.h`, `xiaomi_lightbar.h/.cpp`: radio profile setup, frame build, CRC,
   sequence counters, non-blocking TX queue, RX de-shift/validate/dispatch, unknown-serial logging.
3. `hub-codegen` — `components/xiaomi_lightbar/__init__.py` hub schema (`nrf24_id`, optional
   `channel`, `tx_repeats`, `tx_interval`) + `xiaomi_lightbar.send_command` templatable action.
4. `light-platform` — `light.py` + `lightbar_light.h/.cpp`: `LightOutput` with
   `COLOR_TEMPERATURE`, mireds 153–370, brightness→0..15, change detection, optimistic on/off,
   `pair()` / `toggle_internal_state()`.
5. `event-platform` — `event.py` + `lightbar_event.h/.cpp`: remote registration on the hub,
   6 event types, `on_press` / `on_turn_clockwise` / … / `on_hold` and `on_action` triggers.
6. `button-platform` — `button.py` + `lightbar_button.h/.cpp`: `PAIR` and
   `TOGGLE_INTERNAL_STATE` buttons referencing a light output id.
7. `example-yaml` — `example.yaml` wiring both external components, hub, light, event, buttons,
   plus a sniffer-style logger snippet for discovering an unknown remote serial.
8. `build-verify` — python3.13 venv, `pip install esphome`, `esphome config example.yaml`
   then `esphome compile example.yaml`; fix compile/codegen errors until clean.
9. `readme` — wiring table, full YAML reference, lightbar2mqtt→ESPHome migration table
   (MQTT topic → entity/action), attribution to ebinf / lamperez / benallen-dev / loucks1,
   documented deviations (dedup fix, non-blocking TX).
