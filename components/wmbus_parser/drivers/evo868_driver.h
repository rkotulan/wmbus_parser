#pragma once
#include "../wmbus_parser.h"
#include <string>
#include <vector>
#include <map>

namespace esphome {
namespace wmbus_parser {

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

}  // namespace wmbus_parser
}  // namespace esphome
