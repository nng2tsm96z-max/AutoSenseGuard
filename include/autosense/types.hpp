#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace autosense {

struct SensorData {
    std::uint64_t sequence{0};
    std::int64_t timestamp_ms{0};
    double speed_kmh{0.0};
    double voltage_v{0.0};
    double temperature_c{0.0};
    int soc_percent{0};
    double accel_x_mps2{0.0};
    double accel_y_mps2{0.0};
    double accel_z_mps2{0.0};
};

enum class Severity {
    Normal = 0,
    Warning = 1,
    Critical = 2
};

struct DiagnosticResult {
    Severity severity{Severity::Normal};
    std::vector<std::string> issues;
};

inline const char* to_string(Severity severity) {
    switch (severity) {
        case Severity::Normal: return "NORMAL";
        case Severity::Warning: return "WARNING";
        case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

}  // namespace autosense
