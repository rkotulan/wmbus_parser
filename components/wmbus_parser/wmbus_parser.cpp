#include "wmbus_parser.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wmbus_parser {

static const char *TAG = "wmbus_parser";

WMBusMeter::WMBusMeter(const std::string &id, const std::string &meter_id, const std::string &driver)
    : id_(id), meter_id_(meter_id), driver_(driver) {}

void WMBusMeter::set_total_m3(sensor::Sensor *sensor) {
  this->total_m3_sensor_ = sensor;
}

bool WMBusMeter::decode_packet(const std::vector<uint8_t> &raw, std::map<std::string, std::string> &attrs, float &value) {
  // Currently only evo868 driver implemented
  if (driver_ == "evo868") {
    return evo868::Evo868Driver::decode(raw, attrs, value);
  }
  ESP_LOGW(TAG, "Driver not supported: %s", driver_.c_str());
  return false;
}

void WMBusMeter::handle_packet(const std::vector<uint8_t> &raw) {
  std::map<std::string, std::string> attrs;
  float main_value = NAN;

  if (!this->decode_packet(raw, attrs, main_value)) {
    ESP_LOGW(TAG, "Failed to decode packet for meter %s", this->meter_id_.c_str());
    return;
  }

  // publish main sensor + attributes if sensor configured
  if (this->total_m3_sensor_ != nullptr) {
    // sensor::Sensor::publish_state_with_attributes expects a map<string,string>
    this->total_m3_sensor_->publish_state_with_attributes(main_value, attrs);
  } else {
    // no sensor configured -> just log
    ESP_LOGI(TAG, "Meter %s decoded (no sensor): total=%.3f", this->meter_id_.c_str(), main_value);
  }
}

void WMBusParser::add_meter(WMBusMeter *meter) {
  this->meters_.push_back(meter);
  ESP_LOGI(TAG, "Added meter id=%s meter_id=%s driver=%s",
           meter->id_.c_str(), meter->meter_id_.c_str(), meter->driver_.c_str());
}

void WMBusParser::receive_packet(const std::vector<uint8_t> &raw) {
  // Determine meter id inside packet (after header bytes)
  if (raw.size() < 10) {
    ESP_LOGW(TAG, "Packet too short");
    return;
  }
  size_t offset = 0;
  if (raw.size() >= 2 && raw[0] == 0x54 && (raw[1] == 0x3D || raw[1] == 0xCD)) offset = 2;

  if (raw.size() < offset + 8) {
    ESP_LOGW(TAG, "Packet too short for id extraction");
    return;
  }

  const std::vector<uint8_t> data(raw.begin() + offset, raw.end());
  char id_buf[9];
  sprintf(id_buf, "%02X%02X%02X%02X", data[7], data[6], data[5], data[4]);
  std::string meter_id_str(id_buf);

  // Find registered meter(s) matching meter_id_
  for (auto *m : this->meters_) {
    if (m->meter_id_ == meter_id_str) {
      ESP_LOGI(TAG, "Packet for meter %s (instance %s)", meter_id_str.c_str(), m->id_.c_str());
      m->handle_packet(raw);
      return;
    }
  }

  ESP_LOGW(TAG, "No registered meter found for id %s", meter_id_str.c_str());
}

}  // namespace wmbus_parser
}  // namespace esphome
