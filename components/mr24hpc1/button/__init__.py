import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import CONF_MR24HPC1_ID, mr24hpc1Component, mr24hpc1_ns

ResetButton = mr24hpc1_ns.class_("ResetButton", button.Button)

CONF_RESET = "reset"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(mr24hpc1Component),
    cv.Optional(CONF_RESET): button.button_schema(
        ResetButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
}


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if reset_config := config.get(CONF_RESET):
        b = await button.new_button(reset_config)
        await cg.register_parented(b, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_reset_button(b))
