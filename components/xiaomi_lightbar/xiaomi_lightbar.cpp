#include "xiaomi_lightbar.h"
#include "crc16.h"

#include <cstring>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar";

// Protocol constants (verified against lightbar2mqtt radio.cpp / radio.h).
static const uint8_t PREAMBLE[8] = {0x53, 0x39, 0x14, 0xDD, 0x1C, 0x49, 0x34, 0x12};
static const uint8_t SEND_ADDRESS[5] = {0x55, 0x55, 0x55, 0x55, 0x55};
static const uint64_t RECEIVE_ADDRESS = 0xAAAAAAAAAAULL;

void XiaomiLightbarHub::setup() {
  if (this->nrf24_ == nullptr) {
    ESP_LOGE(TAG, "nRF24 radio not configured");
    this->mark_failed();
    return;
  }
  // RX is delivered through the nrf24 component's data callback.
  this->nrf24_->add_on_data_callback(
      [this](const uint8_t *data, uint8_t len) { this->on_radio_data_(data, len); });
}

void XiaomiLightbarHub::loop() {
  if (this->nrf24_ == nullptr)
    return;

  const uint32_t now = millis();

  // Assert the lightbar radio profile once the radio is up, and keep re-asserting
  // it if the chip is reset (the nrf24 watchdog only restores channel/data-rate/PA).
  if (!this->started_) {
    if (this->nrf24_->is_chip_connected()) {
      this->started_ = true;
      this->last_profile_check_ms_ = now;
      this->assert_radio_profile_();
    }
  } else if (now - this->last_profile_check_ms_ > 1000) {
    this->last_profile_check_ms_ = now;
    if (!this->radio_profile_ok_()) {
      ESP_LOGW(TAG, "Radio lost the lightbar profile; re-asserting");
      this->assert_radio_profile_();
    }
  }

  // Drain the TX queue: one frame per tx_interval_, never blocking the loop.
  if (!this->tx_queue_.empty()) {
    if (!this->tx_active_) {
      // Start of a burst: switch to TX mode once (opens the writing pipe).
      this->nrf24_->stop_listening(SEND_ADDRESS);
      this->tx_active_ = true;
      this->last_tx_ms_ = now - this->tx_interval_;  // allow an immediate first write
    }
    if (now - this->last_tx_ms_ >= this->tx_interval_) {
      TxJob &job = this->tx_queue_.front();
      this->nrf24_->write(job.frame, sizeof(job.frame), true);  // NO_ACK multicast
      this->last_tx_ms_ = now;
      if (job.repeats_left > 0)
        job.repeats_left--;
      if (job.repeats_left == 0)
        this->tx_queue_.erase(this->tx_queue_.begin());
    }
  } else if (this->tx_active_) {
    // Burst finished: restore the reading pipe and go back to listening.
    this->nrf24_->open_reading_pipe(0, RECEIVE_ADDRESS);
    this->nrf24_->start_listening();
    this->tx_active_ = false;
  }
}

void XiaomiLightbarHub::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Lightbar Hub:");
  ESP_LOGCONFIG(TAG, "  Channel: %u", this->channel_);
  ESP_LOGCONFIG(TAG, "  TX repeats: %u", this->tx_repeats_);
  ESP_LOGCONFIG(TAG, "  TX interval: %u ms", this->tx_interval_);
  ESP_LOGCONFIG(TAG, "  Lights: %u, Remotes: %u", (unsigned) this->lights_.size(),
                (unsigned) this->remotes_.size());
  if (this->nrf24_ == nullptr)
    ESP_LOGE(TAG, "  nRF24 radio not set!");
}

void XiaomiLightbarHub::assert_radio_profile_() {
  if (this->nrf24_ == nullptr)
    return;
  this->nrf24_->set_channel(this->channel_);
  this->nrf24_->set_rf_data_rate(nRF24L01::RF24_2MBPS);
  this->nrf24_->disable_crc();
  this->nrf24_->set_auto_ack(false);
  this->nrf24_->disable_dynamic_payloads();
  this->nrf24_->set_payload_size(17);
  this->nrf24_->set_address_width(5);
  this->nrf24_->set_retries(15, 15);
  this->nrf24_->open_writing_pipe(SEND_ADDRESS);
  this->nrf24_->open_reading_pipe(0, RECEIVE_ADDRESS);
  this->nrf24_->start_listening();
  this->tx_active_ = false;
  ESP_LOGD(TAG, "Asserted lightbar radio profile (channel %u, 2MBPS, CRC off, no auto-ack)",
           this->channel_);
}

bool XiaomiLightbarHub::radio_profile_ok_() {
  if (this->nrf24_ == nullptr || !this->nrf24_->is_chip_connected())
    return false;
  // A power-on reset re-enables CRC; use it as a cheap "was the chip reset?" probe.
  return this->nrf24_->get_crc_length() == nRF24L01::RF24_CRC_DISABLED;
}

uint8_t XiaomiLightbarHub::next_sequence_(uint32_t serial) {
  uint8_t &seq = this->tx_sequence_[serial];  // value-initialized to 0 on first use
  seq++;                                       // pre-increment: first command uses 1
  return seq;
}

void XiaomiLightbarHub::send_command(uint32_t serial, uint8_t command, uint8_t options) {
  TxJob job;
  memcpy(job.frame, PREAMBLE, sizeof(PREAMBLE));
  job.frame[8] = (serial >> 16) & 0xFF;
  job.frame[9] = (serial >> 8) & 0xFF;
  job.frame[10] = serial & 0xFF;
  job.frame[11] = 0xFF;
  job.frame[12] = this->next_sequence_(serial);
  job.frame[13] = command;
  job.frame[14] = options;
  uint16_t crc = crc16_lightbar(job.frame, 15);
  job.frame[15] = (crc >> 8) & 0xFF;
  job.frame[16] = crc & 0xFF;
  job.repeats_left = this->tx_repeats_;
  this->tx_queue_.push_back(job);
  ESP_LOGD(TAG, "Queued command 0x%02X (options 0x%02X) for serial 0x%06X", command, options,
           serial);
}

XiaomiLightbarLightBase *XiaomiLightbarHub::get_light(uint32_t serial) {
  for (auto *light : this->lights_) {
    if (light->get_serial() == serial)
      return light;
  }
  return nullptr;
}

void XiaomiLightbarHub::on_radio_data_(const uint8_t *raw, uint8_t len) {
  if (len < 17)
    return;
  // De-shift the raw baseband bytes back into a proper frame. See lamperez's notes:
  // https://github.com/lamperez/xiaomi-lightbar-nrf24#baseband-packet-format
  uint8_t data[17];
  data[0] = 0x50 | (raw[0] >> 5);
  for (uint8_t i = 1; i < 17; i++) {
    data[i] = static_cast<uint8_t>(((raw[i - 1] >> 1) & 0x0F) << 4) |
              static_cast<uint8_t>((raw[i - 1] & 0x01) << 3) | static_cast<uint8_t>(raw[i] >> 5);
  }
  this->handle_packet_(data);
}

void XiaomiLightbarHub::handle_packet_(const uint8_t *data) {
  if (memcmp(data, PREAMBLE, sizeof(PREAMBLE)) != 0)
    return;  // not a lightbar frame

  uint16_t calculated = crc16_lightbar(data, 15);
  uint16_t received = (static_cast<uint16_t>(data[15]) << 8) | data[16];
  if (calculated != received) {
    ESP_LOGW(TAG, "Ignoring packet with wrong checksum");
    return;
  }

  uint32_t serial = (static_cast<uint32_t>(data[8]) << 16) |
                    (static_cast<uint32_t>(data[9]) << 8) | static_cast<uint32_t>(data[10]);
  uint8_t seq = data[12];
  uint8_t command = data[13];
  uint8_t options = data[14];

  // Deduplicate: each action is transmitted ~20x with the same sequence number.
  // (The original firmware intended this but compared against the wrong field; we
  // port the intent and reject a repeat of the last sequence number per serial.)
  RxState &state = this->rx_state_[serial];
  if (state.seen && state.last_seq == seq)
    return;
  state.seen = true;
  state.last_seq = seq;

  for (auto *remote : this->remotes_) {
    if (remote->get_serial() == serial) {
      remote->on_command(command, options);
      return;
    }
  }
  ESP_LOGI(TAG,
           "Action from unknown serial 0x%06X (command 0x%02X). Add an 'event' with this serial "
           "to handle it.",
           serial, command);
}

}  // namespace xiaomi_lightbar
}  // namespace esphome
