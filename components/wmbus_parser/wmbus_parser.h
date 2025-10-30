#pragma once
#include "esphome.h"
#include "driver_registry.h"
#include "esphome/core/automation.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace wmbus_parser {

enum class RawLogLevel {
  RAW_LOG_LEVEL_NONE = 0,
  RAW_LOG_LEVEL_ALL,
  RAW_LOG_LEVEL_VALID_C1_HEADER,
  RAW_LOG_LEVEL_MATCHING_METER_ID,
};

// Compatibility aliases so generated code can refer to the enum values via
// ``wmbus_parser::RAW_LOG_LEVEL_*``.
inline constexpr RawLogLevel RAW_LOG_LEVEL_NONE = RawLogLevel::RAW_LOG_LEVEL_NONE;
inline constexpr RawLogLevel RAW_LOG_LEVEL_ALL = RawLogLevel::RAW_LOG_LEVEL_ALL;
inline constexpr RawLogLevel RAW_LOG_LEVEL_VALID_C1_HEADER = RawLogLevel::RAW_LOG_LEVEL_VALID_C1_HEADER;
inline constexpr RawLogLevel RAW_LOG_LEVEL_MATCHING_METER_ID = RawLogLevel::RAW_LOG_LEVEL_MATCHING_METER_ID;

using AttributeList = std::vector<std::string>;

class WMBusParser;
class WMBusParserDecodeTrigger;

class WMBusMeter : public Component {
 public:
  WMBusMeter(const std::string &meter_id, const std::string &driver);
  WMBusMeter(const std::string &id, const std::string &meter_id, const std::string &driver);
  void setup() override {}
  void loop() override {}

  void set_parent(WMBusParser *parent);

  // Bind sensor (called from Python codegen)
  void set_total_m3(sensor::Sensor *sensor);

  // Called by parser when a raw packet for this meter is available
  void handle_packet(const std::vector<uint8_t> &raw);

  // Public members
  std::string id_;
  std::string meter_id_;
  std::string driver_;
  sensor::Sensor *total_m3_sensor_{nullptr};

 private:
  // decode via driver
  bool decode_packet(const std::vector<uint8_t> &raw, std::map<std::string, std::string> &attrs, float &value);
  WMBusParser *parent_{nullptr};
};

class WMBusParser : public Component {
 public:
  WMBusParser() {}
  void setup() override {}
  void loop() override {}

  // Register meter created from Python to_code()
  void add_meter(WMBusMeter *meter);

  // Expose method that can be called from lambda: id(wmbus_parser)->receive_packet(x)
  void receive_packet(const std::vector<uint8_t> &raw);

  void set_raw_log_level(RawLogLevel level);
  void add_on_decode_trigger(WMBusParserDecodeTrigger *trigger);
  void fire_on_decode(const std::string &meter_id, float value, const AttributeList &attrs);

 protected:
  std::vector<WMBusMeter*> meters_;
  RawLogLevel raw_log_level_{RawLogLevel::RAW_LOG_LEVEL_NONE};
  std::vector<WMBusParserDecodeTrigger *> decode_triggers_;
};

class WMBusParserDecodeTrigger : public Trigger<float, AttributeList, std::string> {
 public:
  explicit WMBusParserDecodeTrigger(WMBusParser *parent) { parent->add_on_decode_trigger(this); }
};

}  // namespace wmbus_parser
}  // namespace esphome
