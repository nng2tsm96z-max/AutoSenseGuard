#pragma once

#include "autosense/types.hpp"

#include <fstream>
#include <string>

namespace autosense {

class CsvLogger {
public:
    explicit CsvLogger(const std::string& path);
    void write(const SensorData& data, const DiagnosticResult& result);

private:
    std::ofstream file_;
};

}  // namespace autosense
