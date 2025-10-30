#include "wmbus_parser.h"
#include "esphome/core/log.h"
#include "evo868_driver.h"
#include <cstdio>

namespace {

std::string format_raw_hex(const std::vector<uint8_t> &raw) {
  std::string hex;
  hex.reserve(raw.size() * 3);
  char buf[4];
  for (size_t i = 0; i < raw.size(); ++i) {
    snprintf(buf, sizeof(buf), "%02X", raw[i]);
    hex += buf;
  }
  return hex;
}

}  // namespace

namespace esphome {
namespace wmbus_parser {

static const char *TAG = "wmbus_parser";

WMBusMeter::WMBusMeter(const std::string &meter_id, const std::string &driver)
    : WMBusMeter(meter_id, meter_id, driver) {}

WMBusMeter::WMBusMeter(const std::string &id, const std::string &meter_id, const std::string &driver)
    : id_(id), meter_id_(meter_id), driver_(driver) {}

void WMBusMeter::set_parent(WMBusParser *parent) { this->parent_ = parent; }

void WMBusMeter::set_total_m3(sensor::Sensor *sensor) { this->total_m3_sensor_ = sensor; }

bool WMBusMeter::decode_packet(const std::vector<uint8_t> &raw, std::map<std::string, std::string> &attrs, float &value) {
  auto decode_fn = DriverRegistry::instance().find(this->driver_);
  if (decode_fn == nullptr) {
    ESP_LOGW(TAG, "Driver not supported: %s", this->driver_.c_str());
    return false;
  }
  return decode_fn(raw, attrs, value);
}

void WMBusMeter::handle_packet(const std::vector<uint8_t> &raw) {
  std::map<std::string, std::string> attrs;
  float main_value = NAN;

  if (!this->decode_packet(raw, attrs, main_value)) {
    ESP_LOGW(TAG, "Failed to decode packet for meter %s", this->meter_id_.c_str());
    return;
  }

  if (this->total_m3_sensor_ != nullptr) {
    this->total_m3_sensor_->publish_state(main_value);
    if (!attrs.empty()) {
      for (const auto &kv : attrs) {
        ESP_LOGD(TAG, "  %s: %s", kv.first.c_str(), kv.second.c_str());
      }
    }
  } else {
    ESP_LOGI(TAG, "Meter %s decoded (no sensor): total=%.3f", this->meter_id_.c_str(), main_value);
  }

  AttributeList attr_list;
  attr_list.reserve(attrs.size());
  for (const auto &kv : attrs) {
    attr_list.emplace_back(kv.first + "=" + kv.second);
  }
  if (this->parent_ != nullptr) {
    this->parent_->fire_on_decode(this->meter_id_, main_value, attr_list);
  }
}

void WMBusParser::add_meter(WMBusMeter *meter) {
  meter->set_parent(this);
  this->meters_.push_back(meter);
  ESP_LOGI(TAG, "Added meter id=%s meter_id=%s driver=%s", meter->id_.c_str(), meter->meter_id_.c_str(), meter->driver_.c_str());
}

void WMBusParser::set_raw_log_level(RawLogLevel level) { this->raw_log_level_ = level; }

void WMBusParser::receive_packet(const std::vector<uint8_t> &raw) {
  if (raw.size() < 10) {
    ESP_LOGW(TAG, "Packet too short");
    return;
  }
  size_t offset = 0;
  bool has_c1_header = raw.size() >= 2 && raw[0] == 0x54 && (raw[1] == 0x3D || raw[1] == 0xCD);

  if (this->raw_log_level_ == RAW_LOG_LEVEL_ALL || (this->raw_log_level_ == RAW_LOG_LEVEL_VALID_C1_HEADER && has_c1_header)) {
    std::string hex = format_raw_hex(raw);
    const char *suffix = has_c1_header ? " (valid C1 header)" : "";
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

  ESP_LOGI(TAG, "Meter id from telegram: %s", meter_id_str.c_str());

  for (auto *m : this->meters_) {
    if (m->meter_id_ == meter_id_str) {
      if (this->raw_log_level_ == RAW_LOG_LEVEL_MATCHING_METER_ID) {
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

void WMBusParser::add_on_decode_trigger(WMBusParserDecodeTrigger *trigger) { this->decode_triggers_.push_back(trigger); }

void WMBusParser::fire_on_decode(const std::string &meter_id, float value, const AttributeList &attrs) {
  for (auto *trigger : this->decode_triggers_) {
    trigger->trigger(value, attrs, meter_id);
  }
}

}  // namespace wmbus_parser
}  // namespace esphome

