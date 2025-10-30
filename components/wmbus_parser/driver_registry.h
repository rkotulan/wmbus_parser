/**
 * Driver registry for WMBus decoders.
 *
 * Provides a lightweight mechanism that allows individual drivers to self-register
 * a decode function by name. This keeps the core parser agnostic of concrete
 * driver implementations and makes it easy to add new ones later on.
 */
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace esphome {
namespace wmbus_parser {

class DriverRegistry {
 public:
  using DecodeFn = bool (*)(const std::vector<uint8_t> &raw,
                            std::map<std::string, std::string> &attributes,
                            float &main_value);

  static DriverRegistry &instance() {
    static DriverRegistry instance;
    return instance;
  }

  void register_driver(const std::string &name, DecodeFn fn) {
    if (fn == nullptr)
      return;
    this->drivers_[name] = fn;
  }

  DecodeFn find(const std::string &name) const {
    auto it = this->drivers_.find(name);
    if (it == this->drivers_.end())
      return nullptr;
    return it->second;
  }

 private:
  std::map<std::string, DecodeFn> drivers_;
};

}  // namespace wmbus_parser
}  // namespace esphome

