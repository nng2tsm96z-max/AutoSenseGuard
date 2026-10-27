#include "autosense/diagnostic.hpp"

#include <cmath>
#include <sstream>
#include <string>

namespace autosense {
namespace {

void add_issue(DiagnosticResult& result, Severity severity, const std::string& message) {
    result.issues.push_back(message);
    if (static_cast<int>(severity) > static_cast<int>(result.severity)) {
        result.severity = severity;
    }
}

bool finite(double value) {
    return std::isfinite(value);
}

}  // namespace

DiagnosticResult Diagnostic::evaluate(const SensorData& data) {
    DiagnosticResult result;

    if (last_sequence_.has_value()) {
        if (data.sequence <= *last_sequence_) {
            add_issue(result, Severity::Critical, "Sequence number repeated or moved backwards");
        } else if (data.sequence > *last_sequence_ + 1) {
            std::ostringstream message;
            message << "Possible missing samples: expected sequence " << (*last_sequence_ + 1)
                    << ", received " << data.sequence;
            add_issue(result, Severity::Warning, message.str());
        }
    }

    if (last_timestamp_ms_.has_value() && data.timestamp_ms <= *last_timestamp_ms_) {
        add_issue(result, Severity::Warning, "Timestamp is not increasing");
    }

    // These are illustrative demo thresholds, not production vehicle safety limits.
    if (!finite(data.speed_kmh) || data.speed_kmh < 0.0 || data.speed_kmh > 250.0) {
        add_issue(result, Severity::Critical, "Speed value is invalid or outside the demo range");
    } else if (data.speed_kmh > 160.0) {
        add_issue(result, Severity::Warning, "Speed is above the demo warning threshold (160 km/h)");
    }

    if (!finite(data.voltage_v) || data.voltage_v < 200.0 || data.voltage_v > 500.0) {
        add_issue(result, Severity::Critical, "Voltage value is invalid or outside the demo range");
    } else if (data.voltage_v < 280.0 || data.voltage_v > 430.0) {
        add_issue(result, Severity::Warning, "Voltage is outside the demo normal band (280-430 V)");
    }

    if (!finite(data.temperature_c) || data.temperature_c < -20.0 || data.temperature_c > 100.0) {
        add_issue(result, Severity::Critical, "Temperature value is invalid or outside the demo range");
    } else if (data.temperature_c > 60.0) {
        add_issue(result, Severity::Critical, "Temperature exceeds 60 C: critical temperature alarm");
    } else if (data.temperature_c >= 45.0) {
        add_issue(result, Severity::Warning, "Temperature is in the 45-60 C warning band");
    }

    if (data.soc_percent < 0 || data.soc_percent > 100) {
        add_issue(result, Severity::Critical, "SOC must be between 0 and 100 percent");
    } else if (data.soc_percent <= 10) {
        add_issue(result, Severity::Warning, "SOC is low (10 percent or less)");
    }

    const double accel_limit = 20.0;
    if (!finite(data.accel_x_mps2) || !finite(data.accel_y_mps2) || !finite(data.accel_z_mps2)) {
        add_issue(result, Severity::Critical, "IMU acceleration contains a non-finite value");
    } else if (std::abs(data.accel_x_mps2) > accel_limit ||
               std::abs(data.accel_y_mps2) > accel_limit ||
               std::abs(data.accel_z_mps2) > accel_limit) {
        add_issue(result, Severity::Warning, "IMU acceleration exceeds the demo threshold (20 m/s^2)");
    }

    last_sequence_ = data.sequence;
    last_timestamp_ms_ = data.timestamp_ms;
    return result;
}

void Diagnostic::reset() {
    last_sequence_.reset();
    last_timestamp_ms_.reset();
}

}  // namespace autosense
