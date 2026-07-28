import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_TYPE

from . import (
    CONF_SERIAL,
    CONF_XIAOMI_LIGHTBAR_ID,
    SERIAL_SCHEMA,
    XiaomiLightbarHub,
    xiaomi_lightbar_ns,
)

XiaomiLightbarButton = xiaomi_lightbar_ns.class_(
    "XiaomiLightbarButton", button.Button, cg.Component
)
ButtonType = xiaomi_lightbar_ns.enum("ButtonType")
BUTTON_TYPES = {
    "pair": ButtonType.BUTTON_TYPE_PAIR,
    "toggle_internal_state": ButtonType.BUTTON_TYPE_TOGGLE_INTERNAL_STATE,
}

CONFIG_SCHEMA = button.button_schema(XiaomiLightbarButton).extend(
    {
        cv.GenerateID(CONF_XIAOMI_LIGHTBAR_ID): cv.use_id(XiaomiLightbarHub),
        cv.Required(CONF_SERIAL): SERIAL_SCHEMA,
        cv.Required(CONF_TYPE): cv.enum(BUTTON_TYPES, lower=True, space="_"),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    hub = await cg.get_variable(config[CONF_XIAOMI_LIGHTBAR_ID])
    cg.add(var.set_parent(hub))
    cg.add(var.set_serial(config[CONF_SERIAL]))
    cg.add(var.set_button_type(config[CONF_TYPE]))
