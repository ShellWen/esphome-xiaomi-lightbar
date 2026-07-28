#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "xiaomi_lightbar.h"

namespace esphome {
namespace xiaomi_lightbar {

template<typename... Ts>
class SendCommandAction : public Action<Ts...>, public Parented<XiaomiLightbarHub> {
 public:
  TEMPLATABLE_VALUE(uint32_t, serial)
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint8_t, options)

  void play(Ts... x) override {
    this->parent_->send_command(this->serial_.value(x...), this->command_.value(x...),
                                this->options_.value(x...));
  }
};

}  // namespace xiaomi_lightbar
}  // namespace esphome
