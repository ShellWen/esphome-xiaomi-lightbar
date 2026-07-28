#include "lightbar_button.h"

#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar.button";

void XiaomiLightbarButton::dump_config() {
  LOG_BUTTON("", "Xiaomi Lightbar Button", this);
  ESP_LOGCONFIG(TAG, "  Serial: 0x%06X", this->serial_);
  ESP_LOGCONFIG(TAG, "  Type: %s",
                this->button_type_ == BUTTON_TYPE_PAIR ? "pair" : "toggle_internal_state");
}

void XiaomiLightbarButton::press_action() {
  if (this->parent_ == nullptr)
    return;
  switch (this->button_type_) {
    case BUTTON_TYPE_PAIR:
      // Pairing re-uses the RESET command (send within 10s of power-cycling the bar).
      this->parent_->send_command(this->serial_, COMMAND_RESET);
      break;
    case BUTTON_TYPE_TOGGLE_INTERNAL_STATE: {
      auto *light = this->parent_->get_light(this->serial_);
      if (light != nullptr) {
        light->toggle_internal_state();
      } else {
        ESP_LOGW(TAG, "No light registered for serial 0x%06X", this->serial_);
      }
      break;
    }
  }
}

}  // namespace xiaomi_lightbar
}  // namespace esphome
