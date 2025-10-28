import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

DEPENDENCIES = []

# Namespace and C++ classes
wmbus_parser_ns = cg.esphome_ns.namespace('wmbus_parser')
WMBusParser = wmbus_parser_ns.class_('WMBusParser', cg.Component)
WMBusMeter = wmbus_parser_ns.class_('WMBusMeter', cg.Component)

# YAML keys
CONF_METERS = 'meters'
CONF_METER_ID = 'meter_id'
CONF_DRIVER = 'driver'
CONF_TOTAL_M3 = 'total_m3'

TOTAL_M3_SCHEMA = sensor.sensor_schema(
    unit_of_measurement='m³',
    accuracy_decimals=3,
)

METER_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.declare_id(WMBusMeter),   # declares child instance id
    cv.Required(CONF_METER_ID): cv.string,
    cv.Required(CONF_DRIVER): cv.string,
    cv.Optional(CONF_TOTAL_M3): TOTAL_M3_SCHEMA,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WMBusParser),
    cv.Required(CONF_METERS): cv.ensure_list(METER_SCHEMA),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    parser = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(parser, config)

    for meter in config[CONF_METERS]:
        # předáváme meter_id a driver konstruktoru, ID handle je meter[CONF_ID]
        m = cg.new_Pvariable(meter[CONF_ID], meter[CONF_METER_ID], meter[CONF_DRIVER])
        await cg.register_component(m, meter)  # teď funguje, pokud WMBusMeter dědí z Component
        cg.add(parser.add_meter(m))

        if CONF_TOTAL_M3 in meter:
            sens = await sensor.new_sensor(meter[CONF_TOTAL_M3])
            cg.add(m.set_total_m3(sens))

