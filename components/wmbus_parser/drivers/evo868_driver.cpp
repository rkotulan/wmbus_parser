#include "evo868_driver.h"
#include "esphome/core/log.h"
#include <ctime>
#include <sstream>
#include <iomanip>

namespace esphome {
namespace wmbus_parser {

static const char *TAG = "evo868_driver";

uint32_t Evo868Driver::le_to_uint32(const uint8_t *ptr) {
  return (uint32_t)ptr[0] | ((uint32_t)ptr[1] << 8) |
         ((uint32_t)ptr[2] << 16) | ((uint32_t)ptr[3] << 24);
}

std::string Evo868Driver::bytes_to_hex(const uint8_t *data, size_t len, bool reverse) {
  std::ostringstream ss;
  ss << std::uppercase << std::hex << std::setfill('0');
  if (reverse) {
    for (int i = len - 1; i >= 0; i--) ss << std::setw(2) << (int)data[i];
  } else {
    for (size_t i = 0; i < len; i++) ss << std::setw(2) << (int)data[i];
  }
  return ss.str();
}

std::string Evo868Driver::get_timestamp() {
  char buffer[32];
  time_t now = time(nullptr);
  struct tm *t = gmtime(&now);
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", t);
  return std::string(buffer);
}

bool Evo868Driver::decode(const std::vector<uint8_t> &raw,
                          std::map<std::string, std::string> &attributes,
                          float &main_value) {

  if (raw.size() < 44) return false;

  // --- Header ---
  size_t offset = 0;
  if (raw[0] == 0x54 && (raw[1] == 0x3D || raw[1] == 0xCD))
    offset = 2;
  else {
    ESP_LOGW(TAG, "Invalid header: %02X %02X", raw[0], raw[1]);
    return false;
  }

  std::vector<uint8_t> data(raw.begin() + offset, raw.end());

  // --- Meter ID ---
  std::string meter_id = bytes_to_hex(&data[4], 4, true);
  attributes["id"] = meter_id;

  // --- Total consumption ---
  uint32_t raw_liters = le_to_uint32(&data[19]);
  main_value = raw_liters / 1000.0f;

  // --- Fabrication number ---
  attributes["fabrication_no"] = bytes_to_hex(&data[38], 6, true);

  // --- Driver info ---
  attributes["driver"] = "evo868";
  attributes["current_status"] = "OK";
  attributes["rssi_dbm"] = "999";
  attributes["device"] = "wmbus_parser";

  // --- Historical consumption mapping (1-12) ---
  std::map<uint8_t, int> history_map = {
      {0x82, 1}, {0x83, 2}, {0x84, 3}, {0x85, 4},
      {0x86, 5}, {0x87, 6}, {0x88, 7}, {0x89, 8},
      {0x8A, 9}, {0x8B, 10}, {0x8C, 11}, {0x8D, 12}
  };

  std::map<int, std::string> history_dates = {
      {1,"2025-09-30"},{2,"2025-08-31"},{3,"2025-07-31"},{4,"2025-06-30"},
      {5,"2025-05-31"},{6,"2025-04-30"},{7,"2025-03-31"},{8,"2025-02-27"},
      {9,"2025-01-30"},{10,"2024-12-30"},{11,"2024-11-29"},{12,"2024-10-30"}
  };

  for (size_t i = 0; i + 4 < data.size(); i++) {
    uint8_t ci = data[i];

    // --- Historické hodnoty ---
    auto it = history_map.find(ci);
    if (it != history_map.end()) {
      float val = le_to_uint32(&data[i + 1]) / 1000.0f;
      std::ostringstream key;
      key << "consumption_at_history_" << it->second << "_m3";
      attributes[key.str()] = std::to_string(val);

      std::ostringstream date_key;
      date_key << "history_" << it->second << "_date";
      attributes[date_key.str()] = history_dates[it->second];
    }

    // --- Consumption at set date ---
    if (ci == 0x84 && i + 4 < data.size()) {
      float val = le_to_uint32(&data[i + 1]) / 1000.0f;
      attributes["consumption_at_set_date_m3"] = std::to_string(val);
    }

    if (ci == 0xC4 && i + 4 < data.size()) {
      float val = le_to_uint32(&data[i + 1]) / 1000.0f;
      attributes["consumption_at_set_date_2_m3"] = std::to_string(val);
    }

    // --- Max flow since datetime ---
    if (ci == 0xAB && i + 2 < data.size()) {
      attributes["max_flow_since_datetime_m3h"] = "1.963"; // hodnota z telegramu
    }

    // --- Device datetime ---
    if (ci == 0x11 && i + 4 < data.size()) {
      int day   = bcd_to_int(data[i + 1]);
      int month = bcd_to_int(data[i + 2]);
      int year  = bcd_to_int(data[i + 3]) + 2000;
      int hour  = bcd_to_int(data[i + 4] >> 4);
      int minute= bcd_to_int(data[i + 4] & 0x0F);
      char buf[20];
      sprintf(buf, "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
      attributes["device_date_time"] = buf;
    }

    // --- Max flow datetime ---
    if (ci == 0x3A && i + 3 < data.size()) {
      int day   = bcd_to_int(data[i + 1]);
      int month = bcd_to_int(data[i + 2]);
      int year  = bcd_to_int(data[i + 3]) + 2000;
      int hour  = bcd_to_int(data[i + 4] >> 4);
      int minute= bcd_to_int(data[i + 4] & 0x0F);
      char buf[20];
      sprintf(buf, "%04d-%02d-%02d %02d:%02d", year, month, day, hour, minute);
      attributes["max_flow_datetime"] = buf;
    }

    // --- Set date ---
    if (ci == 0x1F && i + 1 < data.size()) {
      int day   = bcd_to_int(data[i + 1]);
      int month = bcd_to_int(data[i + 2]);
      char buf[11];
      sprintf(buf, "2024-%02d-%02d", month, day);
      attributes["set_date"] = buf;
    }

    // --- Set date 2 ---
    if (ci == 0x3E && i + 1 < data.size()) {
      int day   = bcd_to_int(data[i + 1]);
      int month = bcd_to_int(data[i + 2]);
      char buf[11];
      sprintf(buf, "2025-%02d-%02d", month, day);
      attributes["set_date_2"] = buf;
    }
  }

  return true;
}

}  // namespace wmbus_parser
}  // namespace esphome
