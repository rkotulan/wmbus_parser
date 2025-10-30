# ESPHome wM-Bus Evo868 Parser

`wmbus_parser` is a proof-of-concept ESPHome component that decodes wM-Bus Evo868 water meter telegrams and exposes them as ESPHome sensors and attributes. It is designed around the Heltec WiFi LoRa 32 V3.1 board (ESP32-S3 with an SX1262 modem), but any compatible ESP32 + SX126x setup should work.

> **Important:** The current driver targets the Maddalena Evo868 implementation of wM-Bus. Other meters will require additional drivers.

You can verify raw telegrams with [wmbusmeters.org](https://wmbusmeters.org/analyze/B04424344630122350077A7C0000202F2F0413CE400000046D03323D3A04FD17004000000E78562409822300441330000000426C1F3C8401132D08000082016C3E39D3013BAB0700C4016D3A2D3C398104FD280182046C3E398404132D080000C404132F0000008405132F000000C405132F0000008406132F000000C4065C8B132F0000008407132F000000C407133000000084081330000000C408133000000084091330000000C4091330000000A6). Additional background on wM-Bus support in ESPHome is available [here](https://github.com/SzczepanLeon/esphome-components/issues/272).

## Hardware
- [Heltec WiFi LoRa 32 V3.1 868 MHz](https://www.laskakit.cz/heltec-wifi-lora-32-v3-868mhz-0-96--wifi-modul/)
- [wM-BUS RADIO MODULE 868 MHz (Evo868)](https://www.maddalena.it/en/product/radio-evo/)
- 868 MHz antenna suitable for the installation environment

## Features
- Manage multiple Evo868 meters with unique meter IDs on a single node.
- Publish the main water consumption as a `total_m3` ESPHome sensor (three decimal places).
- Expose detailed attributes via the decode callback: fabrication number, timestamps, max flow data, status flags, and historic consumption snapshots.
- Configurable raw telegram logging to help with radio troubleshooting.
- Designed for ESP32 boards using the SX126x LoRa modem component in ESPHome.

## Installation
1. Clone or download this repository.
2. Either copy `components/wmbus_parser` into `custom_components/wmbus_parser`, or reference this repository directly via `external_components`:

   ```yaml
   external_components:
     - source: github://rkotulan/wmbus_parser@main
   ```
3. Include and configure the component in your ESPHome YAML file as shown below.

## Example ESPHome configuration

```yaml
esphome:
  name: water-meter-node
  libraries:
    - SPI
    - Ticker

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf

external_components:
  - source: github://rkotulan/wmbus_parser@main
    refresh: 1s

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "${deviceid} Fallback Hotspot"
    password: "wstation"

logger:
  level: DEBUG

api:
ota:

web_server:
  port: 80
  version: 3

spi:
  clk_pin: GPIO9
  mosi_pin: GPIO10
  miso_pin: GPIO11

sx126x:
  dio1_pin: GPIO14
  cs_pin: GPIO8
  busy_pin: GPIO13
  rst_pin: GPIO12
  hw_version: sx1262
  modulation: FSK
  frequency: 868950000
  bandwidth: 234300
  crc_enable: false
  sync_value: [0x54, 0x3D]
  preamble_detect: 2
  deviation: 50000
  bitrate: 100000
  payload_length: 178
  rf_switch: true
  tcxo_voltage: 1_8V
  tcxo_delay: 5ms
  on_packet:
    then:
      - lambda: |-
          id(wmbus_parser_instance)->receive_packet(x);

wmbus_parser:
  id: wmbus_parser_instance
  raw_log_level: MATCHING_METER_ID
  meters:
    - id: water_23123046
      meter_id: "23123046"
      driver: evo868
      total_m3:
        name: "Water Meter 23123046 Total"
  on_decode:
    - lambda: |-
        ESP_LOGI("wmbus-on_decode", "Meter %s decoded %.3f m3", meter_id.c_str(), value);
        for (auto &entry : attributes) {
          ESP_LOGD("wmbus-on_decode", "  %s", entry.c_str());
        }
```

### Raw telegram logging

The `raw_log_level` option controls how much of the incoming SX126x payload is logged:

- `NONE` (default) omits raw telegrams.
- `ALL` prints every frame in hexadecimal form.
- `VALID_C1` prints frames with a valid C1 header (0x54 0x3D or 0x54 0xCD).
- `MATCHING_METER_ID` prints frames that match a configured meter ID.

### Using the decode trigger

`on_decode` automations receive three arguments:

- `value` - the decoded total consumption in cubic metres.
- `attributes` - a vector of `key=value` strings with extended data.
- `meter_id` - the eight-character meter identifier extracted from the telegram.

Example: forward the data to MQTT in JSON form.

```yaml
wmbus_parser:
  ...
  on_decode:
    - mqtt.publish_json:
        topic: "wmbus/${meter_id}"
        payload: !lambda |-
          root["total_m3"] = value;
          for (auto &item : attributes) {
            auto pos = item.find('=');
            if (pos == std::string::npos)
              continue;
            root[item.substr(0, pos)] = item.substr(pos + 1);
          }
```

### Decoded attributes

When a telegram is decoded the driver may provide the following keys (depending on what the meter sends):

- `total_m3`
- `fabrication_no`
- `device_date_time`
- `max_flow_since_datetime_m3h`
- `max_flow_datetime`
- `set_date`
- `set_date_2`
- `consumption_at_set_date_m3`
- `consumption_at_set_date_2_m3`
- `consumption_at_history_<n>_m3`
- `history_interval_months`
- `current_status`
- `timestamp`

Not every telegram contains every field.

## Validating telegrams

Paste a captured frame into [wmbusmeters.org](https://wmbusmeters.org) to cross-check the decoded values. If you see only partial data, increase `raw_log_level` to confirm the full telegram is received.

## Troubleshooting

- Confirm antenna placement; Evo868 radios typically transmit every 16 seconds, so patience helps.
- If you see `Driver not supported`, verify the `driver` value (currently only `evo868` is implemented).
- Use `raw_log_level: ALL` temporarily to check radio reception quality.
- Ensure `meter_id` is entered as the uppercase hexadecimal ID shown on the meter.

## Roadmap

- Additional wM-Bus driver implementations.
- Automated tests for decoder edge cases.
