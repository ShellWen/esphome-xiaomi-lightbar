import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import event
from esphome.const import CONF_TRIGGER_ID

from . import (
    CONF_SERIAL,
    CONF_XIAOMI_LIGHTBAR_ID,
    SERIAL_SCHEMA,
    XiaomiLightbarHub,
    xiaomi_lightbar_ns,
)

CONF_FORWARD_TO = "forward_to"

XiaomiLightbarEvent = xiaomi_lightbar_ns.class_(
    "XiaomiLightbarEvent", event.Event, cg.Component
)
XiaomiLightbarActionTrigger = xiaomi_lightbar_ns.class_(
    "XiaomiLightbarActionTrigger", automation.Trigger.template()
)

# Event types in the same order/naming used by lightbar2mqtt.
EVENT_TYPES = [
    "press",
    "turn_clockwise",
    "turn_counterclockwise",
    "press_turn_clockwise",
    "press_turn_counterclockwise",
    "hold",
]

# Per-action convenience triggers -> the action string they fire on.
ACTION_TRIGGERS = {
    "on_press": "press",
    "on_turn_clockwise": "turn_clockwise",
    "on_turn_counterclockwise": "turn_counterclockwise",
    "on_press_turn_clockwise": "press_turn_clockwise",
    "on_press_turn_counterclockwise": "press_turn_counterclockwise",
    "on_hold": "hold",
}


def validate_forward_to(config):
    if config.get(CONF_FORWARD_TO) == config[CONF_SERIAL]:
        raise cv.Invalid(
            "forward_to must be different from the remote serial",
            path=[CONF_FORWARD_TO],
        )
    return config


CONFIG_SCHEMA = cv.All(
    event.event_schema(XiaomiLightbarEvent)
    .extend(
        {
            cv.GenerateID(CONF_XIAOMI_LIGHTBAR_ID): cv.use_id(XiaomiLightbarHub),
            cv.Required(CONF_SERIAL): SERIAL_SCHEMA,
            cv.Optional(CONF_FORWARD_TO): SERIAL_SCHEMA,
            **{
                cv.Optional(key): automation.validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                            XiaomiLightbarActionTrigger
                        ),
                    }
                )
                for key in ACTION_TRIGGERS
            },
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_forward_to,
)


async def to_code(config):
    var = await event.new_event(config, event_types=EVENT_TYPES)
    await cg.register_component(var, config)
    hub = await cg.get_variable(config[CONF_XIAOMI_LIGHTBAR_ID])
    cg.add(var.set_serial(config[CONF_SERIAL]))
    if CONF_FORWARD_TO in config:
        cg.add(var.set_forward_to(config[CONF_FORWARD_TO]))
    cg.add(hub.register_remote(var))

    for key, action_name in ACTION_TRIGGERS.items():
        for conf in config.get(key, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            cg.add(var.add_action_trigger(action_name, trigger))
            await automation.build_automation(trigger, [], conf)
