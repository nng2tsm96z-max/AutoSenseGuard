#pragma once

#include "autosense/types.hpp"

#include <cstdint>
#include <optional>

namespace autosense {

class Diagnostic {
public:
    DiagnosticResult evaluate(const SensorData& data);
    void reset();

private:
    std::optional<std::uint64_t> last_sequence_;
    std::optional<std::int64_t> last_timestamp_ms_;
};

}  // namespace autosense
