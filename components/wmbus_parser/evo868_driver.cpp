#include "evo868_driver.h"

#include "driver_registry.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace esphome {
namespace wmbus_parser {
namespace evo868 {

namespace {

constexpr const char *TAG = "wmbus_parser.evo868";

struct Date {
  int year{0};
  int month{0};
  int day{0};
  bool valid{false};
};

struct DateTime {
  int year{0};
  int month{0};
  int day{0};
  int hour{0};
  int minute{0};
  bool valid{false};
};

inline uint32_t read_le_uint(const uint8_t *data, size_t len) {
  uint32_t value = 0;
  for (size_t i = 0; i < len; i++) {
    value |= static_cast<uint32_t>(data[i]) << (8 * i);
  }
  return value;
}

inline std::string format_float(float value, int decimals = 3) {
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(decimals) << value;
  std::string result = oss.str();
  auto dot_pos = result.find('.');
  if (dot_pos != std::string::npos) {
    size_t end = result.size();
    while (end > dot_pos + 1 && result[end - 1] == '0')
      --end;
    if (end > dot_pos + 1 && result[end - 1] == '.')
      --end;
    result.erase(end);
  }
  return result;
}

inline std::string format_date(const Date &d) {
  if (!d.valid)
    return {};
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d", d.year, d.month, d.day);
  return std::string(buf);
}

inline std::string format_datetime(const DateTime &dt) {
  if (!dt.valid)
    return {};
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d", dt.year, dt.month, dt.day, dt.hour, dt.minute);
  return std::string(buf);
}

inline std::string decode_bcd_string(const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0)
    return {};
  std::string digits;
  digits.reserve(len * 2);
  bool has_invalid = false;
  for (size_t idx = len; idx-- > 0;) {
    uint8_t value = data[idx];
    uint8_t high = (value >> 4) & 0x0F;
    uint8_t low = value & 0x0F;
    if (high <= 9) {
      digits.push_back(static_cast<char>('0' + high));
    } else {
      has_invalid = true;
      digits.push_back('?');
    }
    if (low <= 9) {
      digits.push_back(static_cast<char>('0' + low));
    } else {
      has_invalid = true;
      digits.push_back('?');
    }
  }
  if (has_invalid) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (size_t idx = len; idx-- > 0;) {
      oss << std::setw(2) << static_cast<int>(data[idx]);
    }
    return oss.str();
  }
  return digits;
}

inline Date decode_date_g(uint16_t raw) {
  Date d;
  d.day = raw & 0x1F;
  d.month = (raw >> 8) & 0x0F;
  int year_high = (raw >> 12) & 0x0F;
  int year_low = (raw >> 5) & 0x07;
  d.year = 2000 + ((year_high << 3) | year_low);
  d.valid = d.day > 0 && d.month > 0;
  return d;
}

inline DateTime decode_datetime_f(const uint8_t *data, size_t len) {
  DateTime dt;
  if (len < 4 || data == nullptr)
    return dt;
  dt.minute = data[0] & 0x3F;
  dt.hour = data[1] & 0x1F;
  dt.day = data[2] & 0x1F;
  dt.month = data[3] & 0x0F;
  int year_high = (data[3] >> 4) & 0x0F;
  int year_low = (data[2] >> 5) & 0x07;
  dt.year = 2000 + ((year_high << 3) | year_low);
  dt.valid = dt.day > 0 && dt.month > 0;
  return dt;
}

inline std::string format_error_flags(uint32_t flags) {
  if (flags == 0)
    return "OK";
  char buf[32];
  snprintf(buf, sizeof(buf), "ERROR_FLAGS_%04X", flags & 0xFFFF);
  return std::string(buf);
}

inline std::string current_timestamp() {
  std::time_t now = std::time(nullptr);
  std::tm tm {};
#if defined(_WIN32) || defined(_WIN64)
  gmtime_s(&tm, &now);
#else
  gmtime_r(&now, &tm);
#endif
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
  return std::string(buf);
}

inline uint16_t data_field_length(uint8_t dif_code, const std::vector<uint8_t> &raw, size_t pos) {
  static const uint8_t lengths[16] = {
      0,  // 0: no data
      1,  // 1: 8 bit integer
      2,  // 2: 16 bit integer
      3,  // 3: 24 bit integer
      4,  // 4: 32 bit integer
      6,  // 5: 48 bit integer
      8,  // 6: 64 bit
      0,  // 7: variable length (handled separately if needed)
      0,  // 8: reserved
      1,  // 9: 2 digit BCD
      2,  // A: 4 digit BCD
      3,  // B: 6 digit BCD
      4,  // C: 8 digit BCD
      6,  // D: 12 digit BCD
      8,  // E: 16 digit BCD
      0   // F: special functions
  };
  if (dif_code < 16) {
    if (dif_code == 7) {
      if (pos >= raw.size())
        return 0;
      return raw[pos];
    }
    return lengths[dif_code];
  }
  return 0;
}

inline uint16_t compute_storage_number(uint8_t dif, const std::vector<uint8_t> &difes) {
  uint16_t storage = (dif >> 6) & 0x01;
  uint16_t shift = 1;
  for (uint8_t dife : difes) {
    storage |= static_cast<uint16_t>(dife & 0x0F) << shift;
    shift += 4;
  }
  return storage;
}

}  // namespace

bool Evo868Driver::decode(const std::vector<uint8_t> &raw,
                          std::map<std::string, std::string> &attributes,
                          float &main_value) {
  attributes.clear();
  main_value = NAN;

  if (raw.size() < 20) {
    ESP_LOGW(TAG, "Telegram too short (%u bytes)", static_cast<unsigned>(raw.size()));
    return false;
  }

  size_t offset = 0;
  if (raw[0] == 0x54 && (raw[1] == 0x3D || raw[1] == 0xCD))
    offset = 2;

  if (raw.size() <= offset + 15) {
    ESP_LOGW(TAG, "Telegram header incomplete");
    return false;
  }

  size_t pos = offset;
  pos += 1;  // L-field
  pos += 1;  // C-field
  pos += 2;  // manufacturer
  pos += 6;  // address
  pos += 1;  // CI-field
  pos += 1;  // access number
  pos += 1;  // status
  pos += 2;  // config word

  if (pos >= raw.size()) {
    ESP_LOGW(TAG, "Unexpected end of telegram");
    return false;
  }

  while (pos < raw.size() && raw[pos] == 0x2F)
    ++pos;

  float total_m3 = NAN;
  float consumption_set_date = NAN;
  float consumption_set_date2 = NAN;
  float max_flow_m3h = NAN;
  std::map<uint16_t, float> history_volumes;

  Date set_date;
  Date set_date2;
  Date history_reference_date;
  DateTime device_datetime;
  DateTime max_flow_datetime;
  uint8_t history_interval_months = 1;

  std::string fabrication_no;
  bool status_present = false;
  uint32_t status_flags = 0;

  while (pos < raw.size()) {
    uint8_t dif = raw[pos++];

    if (dif == 0x2F)
      continue;

    std::vector<uint8_t> difes;
    if (dif & 0x80) {
      bool more = true;
      while (more && pos < raw.size()) {
        uint8_t dife = raw[pos++];
        difes.push_back(dife);
        more = (dife & 0x80) != 0;
      }
    }

    if (pos >= raw.size())
      break;

    uint8_t vif = raw[pos++];
    std::vector<uint8_t> vifes;
    if (vif & 0x80) {
      bool more = true;
      while (more && pos < raw.size()) {
        uint8_t vife = raw[pos++];
        vifes.push_back(vife);
        more = (vife & 0x80) != 0;
      }
    }

    uint8_t dif_code = dif & 0x0F;
    uint16_t len = data_field_length(dif_code, raw, pos);
    if (len == 0) {
      if (dif_code == 0x07 && pos < raw.size()) {
        len = raw[pos];
        ++pos;
      } else {
        continue;
      }
    }

    if (pos + len > raw.size())
      break;

    const uint8_t *data = raw.data() + pos;
    pos += len;

    uint16_t storage = compute_storage_number(dif, difes);
    uint8_t vif_base = vif & 0x7F;

    if (vif_base == 0x13) {
      uint32_t raw_value = read_le_uint(data, len);
      float volume_m3 = static_cast<float>(raw_value) / 1000.0f;
      if (storage == 0 && std::isnan(total_m3)) {
        total_m3 = volume_m3;
      } else if (storage == 1) {
        consumption_set_date = volume_m3;
      } else if (storage == 2) {
        consumption_set_date2 = volume_m3;
      } else if (storage >= 8) {
        history_volumes[storage] = volume_m3;
      }
    } else if (vif_base == 0x6C && len >= 2) {
      Date date = decode_date_g(static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8));
      if (!date.valid)
        continue;
      if (storage == 1) {
        set_date = date;
      } else if (storage == 2) {
        set_date2 = date;
      } else if (storage == 8) {
        history_reference_date = date;
      }
    } else if (vif_base == 0x6D && len >= 4) {
      DateTime dt = decode_datetime_f(data, len);
      if (!dt.valid)
        continue;
      if (storage == 0) {
        device_datetime = dt;
      } else if (storage == 3) {
        max_flow_datetime = dt;
      }
    } else if (vif_base == 0x3B && len >= 3) {
      uint32_t raw_value = read_le_uint(data, 3);
      max_flow_m3h = static_cast<float>(raw_value) / 1000.0f;
    } else if (vif_base == 0x78) {
      fabrication_no = decode_bcd_string(data, len);
    } else if (vif_base == 0x7A && !vifes.empty()) {
      (void)vifes;
    } else if (vif == 0xFD && !vifes.empty()) {
      uint8_t vife = vifes.front() & 0x7F;
      if (vife == 0x17 && len >= 2) {
        status_flags = read_le_uint(data, len);
        status_present = true;
      } else if (vife == 0x28 && len >= 1) {
        history_interval_months = data[0];
        if (history_interval_months == 0)
          history_interval_months = 1;
      }
    }
  }

  if (std::isnan(total_m3)) {
    ESP_LOGW(TAG, "Missing total volume field");
    return false;
  }

  main_value = total_m3;

  attributes["total_m3"] = format_float(total_m3);
  attributes["timestamp"] = current_timestamp();

  if (device_datetime.valid)
    attributes["device_date_time"] = format_datetime(device_datetime);
  if (!fabrication_no.empty())
    attributes["fabrication_no"] = fabrication_no;
  if (status_present)
    attributes["current_status"] = format_error_flags(status_flags);
  if (!std::isnan(consumption_set_date))
    attributes["consumption_at_set_date_m3"] = format_float(consumption_set_date);
  if (set_date.valid)
    attributes["set_date"] = format_date(set_date);
  if (!std::isnan(consumption_set_date2))
    attributes["consumption_at_set_date_2_m3"] = format_float(consumption_set_date2);
  if (set_date2.valid)
    attributes["set_date_2"] = format_date(set_date2);
  if (!std::isnan(max_flow_m3h))
    attributes["max_flow_since_datetime_m3h"] = format_float(max_flow_m3h);
  if (max_flow_datetime.valid)
    attributes["max_flow_datetime"] = format_datetime(max_flow_datetime);
  if (history_reference_date.valid)
    attributes["history_reference_date"] = format_date(history_reference_date);
  if (!history_volumes.empty()) {
    for (const auto &item : history_volumes) {
      uint16_t storage = item.first;
      if (storage < 8)
        continue;
      uint16_t history_index = storage - 7;
      std::ostringstream key;
      key << "consumption_at_history_" << history_index << "_m3";
      attributes[key.str()] = format_float(item.second);
    }
    if (history_interval_months > 0) {
      attributes["history_interval_months"] = std::to_string(history_interval_months);
    }
  }

  return true;
}

namespace {

struct Evo868Registration {
  Evo868Registration() { DriverRegistry::instance().register_driver("evo868", &Evo868Driver::decode); }
};

static Evo868Registration registration;

}  // namespace

}  // namespace evo868
}  // namespace wmbus_parser
}  // namespace esphome

