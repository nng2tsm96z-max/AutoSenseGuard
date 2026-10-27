#include "autosense/csv_logger.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace autosense {
namespace {

std::string csv_quote(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value) {
        if (ch == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

std::string join_issues(const std::vector<std::string>& issues) {
    if (issues.empty()) {
        return "";
    }
    std::ostringstream out;
    for (std::size_t i = 0; i < issues.size(); ++i) {
        if (i != 0) out << " | ";
        out << issues[i];
    }
    return out.str();
}

}  // namespace

CsvLogger::CsvLogger(const std::string& path) {
    const std::filesystem::path file_path(path);
    if (file_path.has_parent_path()) {
        std::filesystem::create_directories(file_path.parent_path());
    }

    const bool exists_and_nonempty = std::filesystem::exists(file_path) &&
                                      std::filesystem::file_size(file_path) > 0;
    file_.open(file_path, std::ios::out | std::ios::app);
    if (!file_) {
        throw std::runtime_error("Cannot open CSV log file: " + path);
    }
    if (!exists_and_nonempty) {
        file_ << "sequence,timestamp_ms,speed_kmh,voltage_v,temperature_c,soc_percent,"
                 "accel_x_mps2,accel_y_mps2,accel_z_mps2,severity,issues\n";
        file_.flush();
    }
}

void CsvLogger::write(const SensorData& data, const DiagnosticResult& result) {
    file_ << data.sequence << ',' << data.timestamp_ms << ','
          << std::fixed << std::setprecision(3)
          << data.speed_kmh << ',' << data.voltage_v << ',' << data.temperature_c << ','
          << data.soc_percent << ',' << data.accel_x_mps2 << ','
          << data.accel_y_mps2 << ',' << data.accel_z_mps2 << ','
          << to_string(result.severity) << ',' << csv_quote(join_issues(result.issues)) << '\n';
    file_.flush();
    if (!file_) {
        throw std::runtime_error("Failed while writing sensor CSV log");
    }
}

}  // namespace autosense
