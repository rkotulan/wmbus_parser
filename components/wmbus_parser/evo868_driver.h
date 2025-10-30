#pragma once

#include <map>
#include <string>
#include <vector>

namespace esphome {
namespace wmbus_parser {
namespace evo868 {

class Evo868Driver {
 public:
  static bool decode(const std::vector<uint8_t> &raw,
                     std::map<std::string, std::string> &attributes,
                     float &main_value);
};

}  // namespace evo868
}  // namespace wmbus_parser
}  // namespace esphome

