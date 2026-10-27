#pragma once

#include "autosense/types.hpp"

#include <optional>
#include <string>

namespace autosense {

// Wire format (one line per sample):
// sequence,timestamp_ms,speed_kmh,voltage_v,temperature_c,soc,accel_x,accel_y,accel_z
std::string serialize_sample(const SensorData& data);
std::optional<SensorData> parse_sample(const std::string& line, std::string* error = nullptr);

}  // namespace autosense
