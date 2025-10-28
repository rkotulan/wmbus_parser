#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/log.h"
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace esphome {
namespace wmbus_parser {

class WMBusDriverBase {
 public:
  virtual ~WMBusDriverBase() = default;
  virtual bool decode(const std::vector<uint8_t> &data,
                      std::map<std::string, std::string> &attributes,
                      float &main_value) = 0;
};

class Evo868Driver : public WMBusDriverBase {
 public:
  bool decode(const std::vector<uint8_t> &data,
              std::map<std::string, std::string> &attributes,
              float &main_value) override;

 private:
  uint32_t le_to_uint32(const uint8_t *ptr);
  std::string bytes_to_hex(const uint8_t *data, size_t len, bool reverse = false);
  std::string get_timestamp();
};

struct WMBusMeter {
  std::string id;
  std::string driver;
  sensor::Sensor *sensor;
};

class WMBusParserComponent : public Component {
 public:
  void add_meter(const WMBusMeter &meter);
  void decode_packet(const std::vector<uint8_t> &raw);

 protected:
  std::map<std::string, WMBusMeter> meters_;
  std::map<std::string, std::shared_ptr<WMBusDriverBase>> drivers_;
};

}  // namespace wmbus_parser
}  // namespace esphome
