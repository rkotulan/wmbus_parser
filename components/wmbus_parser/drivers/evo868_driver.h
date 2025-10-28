#pragma once
#include "esphome.h"
#include <vector>
#include <string>
#include <map>

namespace esphome {
namespace wmbus_parser {
namespace evo868 {

class Evo868Driver {
 public:
  // Decode returns true on success, fills attributes map and main_value (m3)
  static bool decode(const std::vector<uint8_t> &raw,
                     std::map<std::string, std::string> &attributes,
                     float &main_value);

 private:
  static uint32_t le_to_uint32(const uint8_t *ptr);
  static std::string bytes_to_hex(const uint8_t *data, size_t len, bool reverse = false);
  static std::string bcd_to_date_str(const uint8_t *data, size_t len);
  static int bcd_to_int(uint8_t v);
  static std::string get_timestamp();
};

}  // namespace evo868
}  // namespace wmbus_parser
}  // namespace esphome
