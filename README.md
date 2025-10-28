# WMBus Parser (Proof-of-Concept)

**wmbus_parser** is a proof-of-concept ESPHome component for decoding wM-Bus Evo868 water meters.  
It supports multiple meters with individual IDs, publishing total consumption and detailed attributes to Home Assistant.

> ⚠️ This is a proof-of-concept and does **not** aim to cover all water meter types.

## Features

- Supports multiple Evo868 meters with unique IDs.
- Publishes `total_m3` as the main sensor state.
- Decodes detailed attributes:
  - `fabrication_no`
  - Historical consumption (`history_1_m3` … `history_12_m3`) and dates
  - Time attributes (`device_date_time`, `max_flow_datetime`, `set_date`, `set_date_2`)
  - `max_flow_since_datetime_m3h`
- Compatible with ESP32 using SX126x LoRa module.

## Installation

1. Place the `wmbus_parser` folder in your `custom_components` directory of your ESPHome project.
2. Include the component in your YAML configuration:

```yaml
wmbus_parser:
  meters:
    - id: water_23123046_total
      meter_id: "23123046"
      driver: evo868
      total_m3:
        name: "Water Meter Total"
