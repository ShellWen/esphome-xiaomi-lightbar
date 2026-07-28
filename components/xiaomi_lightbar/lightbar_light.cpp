#include "lightbar_light.h"

#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar.light";

void XiaomiLightbarLight::write_state(light::LightState *state) {
  bool want_on = state->current_values.is_on();

  if (this->suppress_send_) {
    // The state change came from toggle_internal_state(); sync without transmitting.
    this->assumed_on_ = want_on;
    this->suppress_send_ = false;
    return;
  }

  if (this->parent_ == nullptr)
    return;

  // On/off is a toggle at the hardware level, so only send when the state flips.
  if (want_on != this->assumed_on_) {
    this->parent_->send_command(this->light_serial_, COMMAND_ON_OFF);
    this->assumed_on_ = want_on;
  }

  if (!want_on)
    return;  // don't push brightness/temperature while the bar is off

  // Brightness -> absolute step 0..15 (dim to minimum, then brighten n steps).
  uint8_t brightness_step = static_cast<uint8_t>(state->current_values.get_brightness() * 15.0f + 0.5f);
  if (!this->has_brightness_ || brightness_step != this->brightness_step_) {
    this->parent_->send_command(this->light_serial_, COMMAND_DIMMER, OPTION_ABSOLUTE_RESET);
    this->parent_->send_command(this->light_serial_, COMMAND_BRIGHTER, brightness_step);
    this->brightness_step_ = brightness_step;
    this->has_brightness_ = true;
  }

  // Color temperature (mireds) -> absolute step 0..15 (cool to minimum, then warm n steps).
  float mireds = state->current_values.get_color_temperature();
  if (mireds < 153.0f)
    mireds = 153.0f;
  if (mireds > 370.0f)
    mireds = 370.0f;
  uint8_t temperature_step =
      static_cast<uint8_t>((1.0f - (mireds - 153.0f) / (370.0f - 153.0f)) * 15.0f + 0.5f);
  if (!this->has_temperature_ || temperature_step != this->temperature_step_) {
    this->parent_->send_command(this->light_serial_, COMMAND_COOLER, OPTION_ABSOLUTE_RESET);
    this->parent_->send_command(this->light_serial_, COMMAND_WARMER, temperature_step);
    this->temperature_step_ = temperature_step;
    this->has_temperature_ = true;
  }
}

void XiaomiLightbarLight::toggle_internal_state() {
  this->assumed_on_ = !this->assumed_on_;
  ESP_LOGD(TAG, "Toggled internal state of 0x%06X to %s", this->light_serial_,
           this->assumed_on_ ? "ON" : "OFF");
  if (this->state_ != nullptr) {
    // Republish to Home Assistant without transmitting an RF command.
    this->suppress_send_ = true;
    auto call = this->state_->make_call();
    call.set_state(this->assumed_on_);
    call.perform();
  }
}

}  // namespace xiaomi_lightbar
}  // namespace esphome
