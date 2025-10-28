#pragma once
#include "esphome.h"
#include <vector>
#include <string>
#include <map>
#include <memory>

#include "esphome/components/wmbus_parser/drivers/evo868_driver.h"

namespace esphome {
namespace wmbus_parser {

class WMBusMeter : public Component {
 public:
  WMBusMeter(const std::string &id, const std::string &meter_id, const std::string &driver);
  void setup() override {}
  void loop() override {}

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

 protected:
  std::vector<WMBusMeter*> meters_;
};

}  // namespace wmbus_parser
}  // namespace esphome
