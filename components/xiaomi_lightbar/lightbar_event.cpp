#include "lightbar_event.h"

#include "esphome/core/log.h"

namespace esphome {
namespace xiaomi_lightbar {

static const char *const TAG = "xiaomi_lightbar.event";

void XiaomiLightbarEvent::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi Lightbar Remote '%s'", this->get_name().c_str());
  ESP_LOGCONFIG(TAG, "  Serial: 0x%06X", this->get_serial());
}

void XiaomiLightbarEvent::on_command(uint8_t command, uint8_t options) {
  const char *action;
  switch (command) {
    case COMMAND_ON_OFF:
      action = "press";
      break;
    case COMMAND_BRIGHTER:
      action = "turn_clockwise";
      break;
    case COMMAND_DIMMER:
      action = "turn_counterclockwise";
      break;
    case COMMAND_COOLER:
      action = "press_turn_clockwise";
      break;
    case COMMAND_WARMER:
      action = "press_turn_counterclockwise";
      break;
    case COMMAND_RESET:
      action = "hold";
      break;
    default:
      return;
  }

  std::string action_str(action);
  this->trigger(action_str);  // fires the event entity + any on_event automations

  auto it = this->action_triggers_.find(action_str);
  if (it != this->action_triggers_.end())
    it->second->trigger();
}

}  // namespace xiaomi_lightbar
}  // namespace esphome
