#include "autosense/diagnostic.hpp"
#include "autosense/protocol.hpp"
#include "autosense/types.hpp"

#include <cmath>
#include <iostream>
#include <string>

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "CHECK failed: " #cond << " at " << __FILE__        \
                      << ':' << __LINE__ << '\n';                            \
            return 1;                                                        \
        }                                                                    \
    } while (0)

using autosense::Diagnostic;
using autosense::SensorData;
using autosense::Severity;

SensorData normal_sample(std::uint64_t sequence = 1) {
    SensorData data;
    data.sequence = sequence;
    data.timestamp_ms = 1000 + static_cast<std::int64_t>(sequence) * 100;
    data.speed_kmh = 55.0;
    data.voltage_v = 365.0;
    data.temperature_c = 35.0;
    data.soc_percent = 80;
    data.accel_x_mps2 = 0.1;
    data.accel_y_mps2 = 0.2;
    data.accel_z_mps2 = 9.81;
    return data;
}

int main() {
    {
        Diagnostic diagnostic;
        const auto result = diagnostic.evaluate(normal_sample());
        CHECK(result.severity == Severity::Normal);
        CHECK(result.issues.empty());
    }
    {
        Diagnostic diagnostic;
        auto data = normal_sample();
        data.temperature_c = 50.0;
        const auto result = diagnostic.evaluate(data);
        CHECK(result.severity == Severity::Warning);
    }
    {
        Diagnostic diagnostic;
        auto data = normal_sample();
        data.temperature_c = 65.0;
        const auto result = diagnostic.evaluate(data);
        CHECK(result.severity == Severity::Critical);
    }
    {
        Diagnostic diagnostic;
        auto data = normal_sample();
        data.soc_percent = 101;
        const auto result = diagnostic.evaluate(data);
        CHECK(result.severity == Severity::Critical);
    }
    {
        Diagnostic diagnostic;
        (void)diagnostic.evaluate(normal_sample(1));
        const auto result = diagnostic.evaluate(normal_sample(3));
        CHECK(result.severity == Severity::Warning);
        CHECK(!result.issues.empty());
        CHECK(result.issues.front().find("Possible missing samples") != std::string::npos);
    }
    {
        auto data = normal_sample(42);
        const auto serialized = autosense::serialize_sample(data);
        const auto parsed = autosense::parse_sample(serialized);
        CHECK(parsed.has_value());
        CHECK(parsed->sequence == data.sequence);
        CHECK(std::abs(parsed->voltage_v - data.voltage_v) < 0.001);
        CHECK(parsed->soc_percent == data.soc_percent);
    }
    {
        const auto parsed = autosense::parse_sample("1,2,3,4");
        CHECK(!parsed.has_value());
        const auto negative_sequence = autosense::parse_sample("-1,2,3,4,5,6,7,8,9");
        CHECK(!negative_sequence.has_value());
    }

    std::cout << "All AutoSenseGuard tests passed.\n";
    return 0;
}
