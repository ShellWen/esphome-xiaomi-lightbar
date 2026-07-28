#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_traits.h"
#include "xiaomi_lightbar.h"

namespace esphome {
namespace xiaomi_lightbar {

class XiaomiLightbarLight : public light::LightOutput,
                           public Parented<XiaomiLightbarHub>,
                           public XiaomiLightbarLightBase {
 public:
  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::COLOR_TEMPERATURE});
    traits.set_min_mireds(153);
    traits.set_max_mireds(370);
    return traits;
  }

  void setup_state(light::LightState *state) override { this->state_ = state; }
  void write_state(light::LightState *state) override;

  // XiaomiLightbarLightBase
  void toggle_internal_state() override;

 protected:
  light::LightState *state_{nullptr};
  bool assumed_on_{true};
  bool has_brightness_{false};
  uint8_t brightness_step_{0};
  bool has_temperature_{false};
  uint8_t temperature_step_{0};
  bool suppress_send_{false};
};

}  // namespace xiaomi_lightbar
}  // namespace esphome
