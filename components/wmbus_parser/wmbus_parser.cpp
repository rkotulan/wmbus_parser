#include "wmbus_parser.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wmbus_parser {

static const char *TAG = "wmbus_parser";

void WMBusParserComponent::add_meter(const WMBusMeter &meter) {
  meters_[meter.id] = meter;
  if (drivers_.find(meter.driver) == drivers_.end()) {
    if (meter.driver == "evo868") {
      drivers_[meter.driver] = std::make_shared<Evo868Driver>();
      ESP_LOGI(TAG, "Registered driver: evo868");
    }
  }
  ESP_LOGI(TAG, "Registered meter %s (%s)", meter.id.c_str(), meter.driver.c_str());
}

void WMBusParserComponent::decode_packet(const std::vector<uint8_t> &raw) {
  if (raw.size() < 10) {
    ESP_LOGW(TAG, "Packet too short");
    return;
  }

  // Skip prefix (0x54 0x3D or 0x54 0xCD)
  std::vector<uint8_t> data = raw;
  if (data[0] == 0x54 && (data[1] == 0x3D || data[1] == 0xCD))
    data.erase(data.begin(), data.begin() + 2);

  // Extract Meter ID (bytes 4..7 reversed)
  char id_buf[9];
  sprintf(id_buf, "%02X%02X%02X%02X", data[7], data[6], data[5], data[4]);
  std::string meter_id(id_buf);

  auto it = meters_.find(meter_id);
  if (it == meters_.end()) {
    ESP_LOGW(TAG, "Unknown meter ID: %s", meter_id.c_str());
    return;
  }

  auto &meter = it->second;
  auto drv_it = drivers_.find(meter.driver);
  if (drv_it == drivers_.end()) {
    ESP_LOGE(TAG, "Driver not found for %s", meter.driver.c_str());
    return;
  }

  std::map<std::string, std::string> attrs;
  float main_value = NAN;

  if (drv_it->second->decode(data, attrs, main_value)) {
    ESP_LOGI(TAG, "Meter %s total_m3=%.3f", meter_id.c_str(), main_value);
    if (meter.sensor != nullptr)
      meter.sensor->publish_state_with_attributes(main_value, attrs);
  } else {
    ESP_LOGW(TAG, "Failed to decode telegram for meter %s", meter_id.c_str());
  }
}

}  // namespace wmbus_parser
}  // namespace esphome
