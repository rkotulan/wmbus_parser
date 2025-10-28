#include "wmbus_parser.h"
#include "esphome/core/log.h"
#include <cstdio>

namespace {

std::string format_raw_hex(const std::vector<uint8_t> &raw) {
  std::string hex;
  hex.reserve(raw.size() * 3);
  char buf[4];
  for (size_t i = 0; i < raw.size(); ++i) {
    snprintf(buf, sizeof(buf), "%02X", raw[i]);
    hex += buf;
    if (i + 1 < raw.size())
      hex += ' ';
  }
  return hex;
}

}  // namespace

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

void WMBusParser::set_raw_log_level(RawLogLevel level) { this->raw_log_level_ = level; }

void WMBusParser::receive_packet(const std::vector<uint8_t> &raw) {
  // Determine meter id inside packet (after header bytes)
  if (raw.size() < 10) {
    ESP_LOGW(TAG, "Packet too short");
    return;
  }
  size_t offset = 0;
  bool has_c1_header = raw.size() >= 2 && raw[0] == 0x54 && (raw[1] == 0x3D || raw[1] == 0xCD);

  if (this->raw_log_level_ == RawLogLevel::RAW_LOG_LEVEL_ALL ||
      (this->raw_log_level_ == RawLogLevel::RAW_LOG_LEVEL_VALID_C1_HEADER && has_c1_header)) {
    std::string hex = format_raw_hex(raw);
    const char *suffix = this->raw_log_level_ == RawLogLevel::RAW_LOG_LEVEL_VALID_C1_HEADER
                             ? " (valid C1 header)"
                             : "";
    ESP_LOGD(TAG, "Raw telegram%s: %s", suffix, hex.c_str());
  }

  if (has_c1_header)
    offset = 2;

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
      if (this->raw_log_level_ == RawLogLevel::RAW_LOG_LEVEL_MATCHING_METER_ID) {
        std::string hex = format_raw_hex(raw);
        ESP_LOGD(TAG, "Raw telegram for meter %s: %s", meter_id_str.c_str(), hex.c_str());
      }
      ESP_LOGI(TAG, "Packet for meter %s (instance %s)", meter_id_str.c_str(), m->id_.c_str());
      m->handle_packet(raw);
      return;
    }
  }

  ESP_LOGW(TAG, "No registered meter found for id %s", meter_id_str.c_str());
}

}  // namespace wmbus_parser
}  // namespace esphome
