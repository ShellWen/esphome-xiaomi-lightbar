#pragma once

#include <map>
#include <string>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/event/event.h"
#include "xiaomi_lightbar.h"

namespace esphome {
namespace xiaomi_lightbar {

class XiaomiLightbarActionTrigger : public Trigger<> {};

class XiaomiLightbarEvent : public event::Event,
                           public Component,
                           public XiaomiLightbarRemoteBase {
 public:
  void dump_config() override;

  // XiaomiLightbarRemoteBase
  void on_command(uint8_t command, uint8_t options) override;

  void add_action_trigger(const std::string &action, XiaomiLightbarActionTrigger *trigger) {
    this->action_triggers_[action] = trigger;
  }

 protected:
  std::map<std::string, XiaomiLightbarActionTrigger *> action_triggers_;
};

}  // namespace xiaomi_lightbar
}  // namespace esphome
