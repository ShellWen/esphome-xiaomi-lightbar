#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/nrf24/nrf24.h"

namespace esphome {
namespace xiaomi_lightbar {

// Command codes (from lightbar2mqtt Lightbar::Command).
enum Command : uint8_t {
  COMMAND_ON_OFF = 0x01,
  COMMAND_COOLER = 0x02,
  COMMAND_WARMER = 0x03,
  COMMAND_BRIGHTER = 0x04,
  COMMAND_DIMMER = 0x05,
  COMMAND_RESET = 0x06,
};

// The "reset to minimum before setting an absolute value" options byte.
// In the original firmware this is (byte)(0x0 - 16) == 0xF0.
static const uint8_t OPTION_ABSOLUTE_RESET = 0xF0;

// Lightweight interface implemented by the event platform (a remote).
// Kept separate from the event header so the hub does not depend on it.
class XiaomiLightbarRemoteBase {
 public:
  virtual ~XiaomiLightbarRemoteBase() = default;
  void set_serial(uint32_t serial) { this->remote_serial_ = serial; }
  uint32_t get_serial() const { return this->remote_serial_; }
  // Called by the hub with a decoded (command, options) pair.
  virtual void on_command(uint8_t command, uint8_t options) = 0;

 protected:
  uint32_t remote_serial_{0};
};

// Lightweight interface implemented by the light platform.
class XiaomiLightbarLightBase {
 public:
  virtual ~XiaomiLightbarLightBase() = default;
  void set_serial(uint32_t serial) { this->light_serial_ = serial; }
  uint32_t get_serial() const { return this->light_serial_; }
  // Flip the optimistic on/off state without transmitting anything.
  virtual void toggle_internal_state() = 0;

 protected:
  uint32_t light_serial_{0};
};

class XiaomiLightbarHub : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  // Run after the nrf24 component has been set up.
  float get_setup_priority() const override { return setup_priority::DATA - 1.0f; }

  void set_nrf24(nrf24::NRF24Component *nrf24) { this->nrf24_ = nrf24; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_tx_repeats(uint8_t repeats) { this->tx_repeats_ = repeats; }
  void set_tx_interval(uint32_t interval_ms) { this->tx_interval_ = interval_ms; }

  // Build a 17-byte frame for (serial, command, options) and enqueue it for a
  // non-blocking burst transmission from loop().
  void send_command(uint32_t serial, uint8_t command, uint8_t options = 0x00);

  void register_remote(XiaomiLightbarRemoteBase *remote) { this->remotes_.push_back(remote); }
  void register_light(XiaomiLightbarLightBase *light) { this->lights_.push_back(light); }
  XiaomiLightbarLightBase *get_light(uint32_t serial);

 protected:
  struct TxJob {
    uint8_t frame[17];
    uint8_t repeats_left;
  };
  struct RxState {
    bool seen{false};
    uint8_t last_seq{0};
  };

  // Re-apply the parts of the radio configuration that the lightbar protocol
  // needs but that the nrf24 component does not own / does not restore after a
  // reset (CRC off, auto-ack off, payload size, addresses, pipes).
  void assert_radio_profile_();
  bool radio_profile_ok_();
  uint8_t next_sequence_(uint32_t serial);
  void on_radio_data_(const uint8_t *raw, uint8_t len);
  void handle_packet_(const uint8_t *data);

  nrf24::NRF24Component *nrf24_{nullptr};
  uint8_t channel_{68};
  uint8_t tx_repeats_{20};
  uint32_t tx_interval_{10};

  // Non-blocking TX queue drained one frame per interval from loop().
  std::vector<TxJob> tx_queue_;
  bool tx_active_{false};
  uint32_t last_tx_ms_{0};

  bool started_{false};
  uint32_t last_profile_check_ms_{0};

  std::map<uint32_t, uint8_t> tx_sequence_;
  std::map<uint32_t, RxState> rx_state_;

  std::vector<XiaomiLightbarRemoteBase *> remotes_;
  std::vector<XiaomiLightbarLightBase *> lights_;
};

}  // namespace xiaomi_lightbar
}  // namespace esphome
