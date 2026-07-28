import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT

from . import (
    CONF_SERIAL,
    CONF_XIAOMI_LIGHTBAR_ID,
    SERIAL_SCHEMA,
    XiaomiLightbarHub,
    xiaomi_lightbar_ns,
)

XiaomiLightbarLight = xiaomi_lightbar_ns.class_(
    "XiaomiLightbarLight", light.LightOutput
)

CONFIG_SCHEMA = light.light_schema(
    XiaomiLightbarLight, light.LightType.BRIGHTNESS_ONLY
).extend(
    {
        cv.GenerateID(CONF_XIAOMI_LIGHTBAR_ID): cv.use_id(XiaomiLightbarHub),
        cv.Required(CONF_SERIAL): SERIAL_SCHEMA,
        # The light bar has 16 discrete brightness steps, so gamma correction and
        # transitions (which would emit many intermediate steps) are disabled.
        cv.Optional(CONF_GAMMA_CORRECT, default=0.0): cv.positive_float,
        cv.Optional(
            CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
    }
)


async def to_code(config):
    var = await light.new_light(config)
    hub = await cg.get_variable(config[CONF_XIAOMI_LIGHTBAR_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_serial(config[CONF_SERIAL]))
    cg.add(hub.register_light(var))
