# esphome-xiaomi-lightbar

A native [ESPHome](https://esphome.io) external component to control a **Xiaomi Mi
Computer Monitor Light Bar (MJGJD01YL)** over an nRF24L01 radio, and to receive its
remote's rotary/press actions.

This is a port of [`ebinf/lightbar2mqtt`](https://github.com/ebinf/lightbar2mqtt)
(MIT) to ESPHome. Instead of hand-rolled WiFi + MQTT + Home Assistant discovery, it
uses ESPHome's native API, so WiFi, OTA, logging, and Home Assistant entities are all
handled for you. The light bar becomes a `light` entity, the remote an `event` entity.

> Only the **MJGJD01YL** (no BLE/WiFi) is supported. The MJGJD02YL will **not** work.

The low-level SPI radio driver is **not** re-implemented — this component consumes
[`loucks1/nrf24_espidf`](https://github.com/loucks1/nrf24_espidf) **unmodified** through
its public C++ API.

## Features

- `light` entity: on/off, brightness (16 steps), color temperature (mireds 153–370).
- `event` entity: the 6 remote actions (`press`, `turn_clockwise`,
  `turn_counterclockwise`, `press_turn_clockwise`, `press_turn_counterclockwise`,
  `hold`), with native `on_event` plus per-action triggers (`on_press`, …).
- `button` entities: `pair` and `toggle_internal_state` (resync optimistic on/off).
- `xiaomi_lightbar.send_command` templatable action for advanced / raw use.
- Multiple light bars and remotes on a single radio.

## Hardware / wiring

An ESP32 plus an nRF24L01 module. Use a stable 3.3 V supply and a decoupling capacitor
across the radio's VCC/GND.

| nRF24L01 | ESP32 (example) | Notes |
| -------- | --------------- | ----- |
| VCC      | 3V3             | stable 3.3 V + capacitor |
| GND      | GND             | |
| CE       | GPIO17          | `ce_pin` on the `nrf24:` component |
| CSN      | GPIO5           | `cs_pin` on the `nrf24:` component |
| SCK      | GPIO18          | SPI `clk_pin` |
| MOSI     | GPIO23          | SPI `mosi_pin` |
| MISO     | GPIO19          | SPI `miso_pin` |

Pins are examples — set them to match your board.

## Installation

Add both external components (the unmodified nrf24 driver and this one), then configure
the radio, hub, and entities. See [example.yaml](example.yaml) for a complete config.

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/loucks1/nrf24_espidf
    components: [nrf24]
  - source:
      type: git
      url: https://github.com/zweckj/esphome-xiaomi-lightbar
    components: [xiaomi_lightbar]

spi:
  clk_pin: GPIO18
  mosi_pin: GPIO23
  miso_pin: GPIO19

# The hub asserts CRC-off / auto-ack-off / payload-size / addresses itself, but the
# nrf24 watchdog only restores channel/data-rate/PA after a chip reset, so set those
# here to match the lightbar profile.
nrf24:
  id: radio
  cs_pin: GPIO5
  ce_pin: GPIO17
  channel: 68
  rf_data_rate: 2MBPS
  pa_level: MAX

xiaomi_lightbar:
  id: lightbar_hub
  nrf24_id: radio
```

> The ESP-IDF framework is recommended (the nrf24 component targets ESP-IDF).

## Configuration reference

### `xiaomi_lightbar:` (hub)

| Option        | Default | Description |
| ------------- | ------- | ----------- |
| `nrf24_id`    | —       | **Required.** ID of the `nrf24:` component. |
| `channel`     | `68`    | RF channel the hub asserts (match the `nrf24:` channel). |
| `tx_repeats`  | `20`    | Times each frame is re-sent per command (matches upstream). |
| `tx_interval` | `10ms`  | Spacing between frame repeats (drained non-blocking from `loop()`). |

### `light:`

```yaml
light:
  - platform: xiaomi_lightbar
    name: "Monitor Light Bar"
    xiaomi_lightbar_id: lightbar_hub
    serial: 0xABCDEF        # 24-bit light bar serial
```

Optimistic: the bar reports no state, so on/off is assumed. Brightness maps to 16
steps; color temperature maps to 153–370 mireds. If the bar gets out of sync (e.g. it
was toggled with the physical remote), use the `toggle_internal_state` button.

### `event:`

```yaml
event:
  - platform: xiaomi_lightbar
    name: "Light Bar Remote"
    xiaomi_lightbar_id: lightbar_hub
    serial: 0x123456        # 24-bit remote serial
    on_press:
      then:
        - logger.log: "pressed"
    # on_turn_clockwise / on_turn_counterclockwise /
    # on_press_turn_clockwise / on_press_turn_counterclockwise / on_hold
```

Event types: `press`, `turn_clockwise`, `turn_counterclockwise`,
`press_turn_clockwise`, `press_turn_counterclockwise`, `hold`. The native `on_event`
trigger (with `event_type`) is also available.

### `button:`

```yaml
button:
  - platform: xiaomi_lightbar
    name: "Light Bar Pair"
    xiaomi_lightbar_id: lightbar_hub
    serial: 0xABCDEF
    type: pair                    # sends RESET; use within 10s of power-cycling the bar
  - platform: xiaomi_lightbar
    name: "Light Bar Resync State"
    xiaomi_lightbar_id: lightbar_hub
    serial: 0xABCDEF
    type: toggle_internal_state   # flips optimistic on/off, no RF sent
```

### `xiaomi_lightbar.send_command` action

Templatable low-level action (advanced use / sniffing). `command`/`options` are raw
protocol bytes (`ON_OFF 0x01`, `COOLER 0x02`, `WARMER 0x03`, `BRIGHTER 0x04`,
`DIMMER 0x05`, `RESET 0x06`).

```yaml
- xiaomi_lightbar.send_command:
    id: lightbar_hub
    serial: 0xABCDEF
    command: 0x06
    options: 0x00
```

## Finding a remote's serial

Flash the config, then press/turn the remote and watch the logs (DEBUG/INFO):

```
[I][xiaomi_lightbar]: Action from unknown serial 0x7B7E12 (command 0x01). Add an 'event' with this serial to handle it.
```

Copy that serial into an `event:` (and/or `light:`) block.

## Pairing

To decouple the bar from its original remote (or use a different serial), power-cycle
the light bar and, within 10 seconds, press the `pair` button (or send
`xiaomi_lightbar.send_command` with `command: 0x06`). The bar blinks to confirm.

## Migrating from lightbar2mqtt

| lightbar2mqtt MQTT topic / payload | ESPHome equivalent |
| ---------------------------------- | ------------------ |
| `.../<serial>/command` `{state,brightness,color_temp}` | `light:` entity (state, brightness 0–15 ≙ 0–100%, mireds 153–370) |
| `.../<serial>/state` (`press`, `turn_clockwise`, …) | `event:` entity (`on_event` / `on_press` / …) |
| `.../<serial>/pair` | `button:` `type: pair` |
| `.../<serial>/toggle_internal` | `button:` `type: toggle_internal_state` |
| MQTT availability topic | ESPHome API/`status` (built in) |
| Home Assistant MQTT discovery | ESPHome native API discovery |

## How it works / deviations from upstream

- **nrf24 stays unmodified.** The whole lightbar profile is achieved via the public
  API: before every `start_listening()` the hub calls `open_reading_pipe(0, …)` (the
  driver's `stop_listening()` closes all pipes and `open_writing_pipe()` clobbers the
  pipe-0 RX address), and it asserts CRC-off/auto-ack-off/payload-17/addresses itself.
- **Non-blocking TX.** Upstream sends `20 × (write + delay(10))` ≈ 200 ms inline, which
  would trip ESPHome's loop watchdog. Here each command is queued and drained one frame
  per `tx_interval` from `loop()` (NO_ACK writes return in well under 1 ms).
- **Radio-profile self-heal.** The nrf24 15 s watchdog only restores channel/data-rate/
  PA. The hub detects a chip reset cheaply (CRC re-enabled) and re-asserts the full
  lightbar profile, so RX/TX recover automatically.
- **Deduplication fix.** Upstream compared the packet sequence number against the wrong
  struct field; the intent (reject a repeat of the last sequence number per serial) is
  ported correctly here.

## Acknowledgements

- [ebinf/lightbar2mqtt](https://github.com/ebinf/lightbar2mqtt) — the project this ports.
- [lamperez/xiaomi-lightbar-nrf24](https://github.com/lamperez/xiaomi-lightbar-nrf24) —
  the original protocol reverse engineering.
- [benallen-dev/xiaomi-lightbar](https://github.com/benallen-dev/xiaomi-lightbar).
- [loucks1/nrf24_espidf](https://github.com/loucks1/nrf24_espidf) — the ESPHome nRF24 driver.

## License

MIT — see [LICENSE](LICENSE).
