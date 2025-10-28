import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_NAME

DEPENDENCIES = []

# Definice komponenty v C++
wmbus_parser_ns = cg.esphome_ns.namespace('wmbus_parser')
WMBusParser = wmbus_parser_ns.class_('WMBusParser', cg.Component)

CONF_METERS = 'meters'
CONF_METER_ID = 'meter_id'
CONF_DRIVER = 'driver'
CONF_TOTAL_M3 = 'total_m3'

TOTAL_M3_SCHEMA = sensor.sensor_schema(
    unit_of_measurement='m³',
    accuracy_decimals=3,
)

METER_SCHEMA = cv.Schema({
    cv.Required(CONF_ID): cv.use_id(WMBusParser),
    cv.Required(CONF_METER_ID): cv.string,
    cv.Required(CONF_DRIVER): cv.string,
    cv.Optional(CONF_TOTAL_M3): TOTAL_M3_SCHEMA,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WMBusParser),
    cv.Required(CONF_METERS): cv.ensure_list(METER_SCHEMA),
}).extend(cv.COMPONENT_SCHEMA)

def to_code(config):
    parser = cg.new_Pvariable(config[CONF_ID])
    cg.add_global(parser)
    for meter in config[CONF_METERS]:
        meter_obj = parser.add_meter(
            meter[CONF_METER_ID],
            meter[CONF_DRIVER],
        )
        if CONF_TOTAL_M3 in meter:
            sensor.Pvariable(meter[CONF_TOTAL_M3], meter_obj.set_total_m3)
