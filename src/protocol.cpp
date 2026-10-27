#include "autosense/protocol.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace autosense {
namespace {

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    // getline does not return the final empty field for a trailing comma.
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

template <typename T>
T parse_integer(const std::string& text) {
    std::size_t used = 0;
    if constexpr (std::is_unsigned_v<T>) {
        if (text.empty() || text.front() == '-') {
            throw std::invalid_argument("negative value is not allowed in an unsigned field");
        }
        const unsigned long long value = std::stoull(text, &used);
        if (used != text.size()) {
            throw std::invalid_argument("unexpected characters in integer field");
        }
        if (value > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
            throw std::out_of_range("integer field is too large");
        }
        return static_cast<T>(value);
    } else {
        const long long value = std::stoll(text, &used);
        if (used != text.size()) {
            throw std::invalid_argument("unexpected characters in integer field");
        }
        if (value < static_cast<long long>(std::numeric_limits<T>::min()) ||
            value > static_cast<long long>(std::numeric_limits<T>::max())) {
            throw std::out_of_range("integer field is outside its allowed range");
        }
        return static_cast<T>(value);
    }
}

double parse_double(const std::string& text) {
    std::size_t used = 0;
    const double value = std::stod(text, &used);
    if (used != text.size() || !std::isfinite(value)) {
        throw std::invalid_argument("invalid floating-point field");
    }
    return value;
}

}  // namespace

std::string serialize_sample(const SensorData& data) {
    std::ostringstream out;
    out << data.sequence << ',' << data.timestamp_ms << ','
        << std::fixed << std::setprecision(3)
        << data.speed_kmh << ',' << data.voltage_v << ',' << data.temperature_c << ','
        << data.soc_percent << ',' << data.accel_x_mps2 << ','
        << data.accel_y_mps2 << ',' << data.accel_z_mps2 << '\n';
    return out.str();
}

std::optional<SensorData> parse_sample(const std::string& line, std::string* error) {
    try {
        std::string normalized = line;
        while (!normalized.empty() && (normalized.back() == '\n' || normalized.back() == '\r')) {
            normalized.pop_back();
        }
        const auto fields = split_csv(normalized);
        if (fields.size() != 9) {
            throw std::invalid_argument("expected exactly 9 comma-separated fields");
        }

        SensorData data;
        data.sequence = parse_integer<std::uint64_t>(fields[0]);
        data.timestamp_ms = parse_integer<std::int64_t>(fields[1]);
        data.speed_kmh = parse_double(fields[2]);
        data.voltage_v = parse_double(fields[3]);
        data.temperature_c = parse_double(fields[4]);
        data.soc_percent = parse_integer<int>(fields[5]);
        data.accel_x_mps2 = parse_double(fields[6]);
        data.accel_y_mps2 = parse_double(fields[7]);
        data.accel_z_mps2 = parse_double(fields[8]);
        return data;
    } catch (const std::exception& ex) {
        if (error != nullptr) {
            *error = ex.what();
        }
        return std::nullopt;
    }
}

}  // namespace autosense
