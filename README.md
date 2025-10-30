# Water meter - ESPHome + Heltec WiFi LoRa 32 V3.1 868Mhz

## Harware
- [Heltec WiFi LoRa 32 V3.1 868Mhz](https://www.laskakit.cz/heltec-wifi-lora-32-v3-868mhz-0-96--wifi-modul/)
- Water meter [wM-BUS RADIO MODULE 868 MHz](https://www.maddalena.it/en/product/radio-evo/)

**wmbus_parser** is a proof-of-concept ESPHome component for decoding wM-Bus Evo868 water meters.  
It supports multiple meters with individual IDs, publishing total consumption and detailed attributes to Home Assistant.

> ⚠️ This is a proof-of-concept and does **not** aim to cover all water meter types.

You can check your telegram on [wmbusmeters.org](https://wmbusmeters.org/analyze/B04424344630122350077A7C0000202F2F0413CE400000046D03323D3A04FD17004000000E78562409822300441330000000426C1F3C8401132D08000082016C3E39D3013BAB0700C4016D3A2D3C398104FD280182046C3E398404132D080000C404132F0000008405132F000000C405132F0000008406132F000000C4065C8B132F0000008407132F000000C407133000000084081330000000C408133000000084091330000000C4091330000000A6).

More information about wmbus on ESPHome [here](https://github.com/SzczepanLeon/esphome-components/issues/272).

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
esphome:
  name: wmbusmeter-test-2
  libraries:
     - "SPI"
     - "Ticker"
  includes:
     - include/utils/icons.h   

external_components:
  - source: github://rkotulan/wmbus_parser@main    
    refresh: 1s   

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf

api:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

  # Enable fallback hotspot (captive portal) in case wifi connection fails
  ap:
    ssid: "${deviceid} Fallback Hotspot"
    password: "MjUhjdshTgd02" # XXX

web_server:
  port: 80
  version: 3

ota:
  - platform: esphome
    
logger:
  level: DEBUG

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
  bandwidth: 234_3kHz
  crc_enable: false
  sync_value:  [0x54, 0x3D]
  preamble_detect: 2
  deviation: 50000 
  bitrate: 100000
  payload_length: 178
  rf_switch: true
  shaping: NONE
  tcxo_voltage: 1_8V
  tcxo_delay: 5ms
  on_packet:
    then:
      - lambda: |-
          // call wmbus parser
          id(wmbus_parser_instance)->receive_packet(x);  

wmbus_parser:
  id: wmbus_parser_instance
  raw_log_level: ALL
  meters:
    - id: water_23123046_total
      meter_id: "23123046"
      driver: evo868
      total_m3:
        name: "Water Meter 23123046 Total"
  on_decode:
    - then:
        - lambda: |-
            ESP_LOGI("wmbus-on_decode", "✅ Meter %s decoded %.3f m³", meter_id.c_str(), value);
            for (auto &entry : attributes) {
              ESP_LOGD("wmbus-on_decode", "  %s", entry.c_str());
            }            
          #   // Public to MQTT:
          #   // id(mqtt_client).publish("wmbus/23123046/value", to_string(value));
