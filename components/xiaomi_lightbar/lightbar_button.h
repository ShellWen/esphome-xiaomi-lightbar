#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/button/button.h"
#include "xiaomi_lightbar.h"

namespace esphome {
namespace xiaomi_lightbar {

enum ButtonType : uint8_t {
  BUTTON_TYPE_PAIR = 0,
  BUTTON_TYPE_TOGGLE_INTERNAL_STATE = 1,
};

class XiaomiLightbarButton : public button::Button,
                            public Component,
                            public Parented<XiaomiLightbarHub> {
 public:
  void set_serial(uint32_t serial) { this->serial_ = serial; }
  void set_button_type(ButtonType type) { this->button_type_ = type; }
  void dump_config() override;

 protected:
  void press_action() override;

  uint32_t serial_{0};
  ButtonType button_type_{BUTTON_TYPE_PAIR};
};

}  // namespace xiaomi_lightbar
}  // namespace esphome
