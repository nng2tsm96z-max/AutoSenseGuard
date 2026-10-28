#include "autosense/bounded_queue.hpp"
#include "autosense/csv_logger.hpp"
#include "autosense/diagnostic.hpp"
#include "autosense/protocol.hpp"
#include "autosense/types.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
volatile std::sig_atomic_t stop_requested = 0;

void handle_signal(int) {
    stop_requested = 1;
}

bool parse_port(const char* text, int& port) {
    try {
        std::size_t used = 0;
        const long value = std::stol(text, &used);
        if (used != std::strlen(text) || value < 1 || value > 65535) return false;
        port = static_cast<int>(value);
        return true;
    } catch (...) {
        return false;
    }
}

struct QueuedSample {
    autosense::SensorData data;
    std::chrono::steady_clock::time_point enqueued_at;
};

long long percentile_us(const std::vector<long long>& sorted, double p) {
    if (sorted.empty()) {
        return 0;
    }
    const std::size_t index =
        static_cast<std::size_t>(p * static_cast<double>(sorted.size() - 1));
    return sorted[index];
}

}  // namespace

int main(int argc, char* argv[]) {
    int port = 9000;
    if (argc > 2 || (argc == 2 && !parse_port(argv[1], port))) {
        std::cerr << "Usage: " << argv[0] << " [port 1-65535]\n";
        return 2;
    }

    struct sigaction action{};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;  // Let accept()/recv() return EINTR so shutdown can proceed.
    if (::sigaction(SIGINT, &action, nullptr) != 0 ||
        ::sigaction(SIGTERM, &action, nullptr) != 0) {
        std::cerr << "Could not install signal handlers.\n";
        return 1;
    }

    int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "socket() failed: " << std::strerror(errno) << '\n';
        return 1;
    }

    int reuse = 1;
    (void)::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<std::uint16_t>(port));
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // Bind to this Mac/Linux machine only.

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "bind() failed: " << std::strerror(errno)
                  << " (is port " << port << " already in use?)\n";
        ::close(server_fd);
        return 1;
    }
    if (::listen(server_fd, 4) < 0) {
        std::cerr << "listen() failed: " << std::strerror(errno) << '\n';
        ::close(server_fd);
        return 1;
    }

    try {
        autosense::CsvLogger logger("logs/sensor_data.csv");
        autosense::BoundedQueue<QueuedSample> queue(1024);
        std::uint64_t received_lines = 0;
        std::uint64_t parsed_count = 0;
        std::uint64_t rejected_count = 0;
        std::uint64_t dropped_count = 0;
        std::uint64_t processed_count = 0;
        std::uint64_t warning_count = 0;
        std::uint64_t critical_count = 0;
        std::vector<long long> queue_wait_us;

        std::thread processor([&]() {
            autosense::Diagnostic diagnostic;
            QueuedSample item;
            while (queue.pop(item)) {
                try {
                    const auto now = std::chrono::steady_clock::now();
                    const auto wait_us = std::chrono::duration_cast<std::chrono::microseconds>(
                        now - item.enqueued_at).count();
                    queue_wait_us.push_back(wait_us);

                    const auto& data = item.data;
                    const auto result = diagnostic.evaluate(data);
                    logger.write(data, result);
                    ++processed_count;
                    if (result.severity == autosense::Severity::Warning) ++warning_count;
                    if (result.severity == autosense::Severity::Critical) ++critical_count;

                    std::cout << "[" << autosense::to_string(result.severity) << "] seq="
                              << data.sequence << " speed=" << data.speed_kmh
                              << " km/h, voltage=" << data.voltage_v
                              << " V, temp=" << data.temperature_c
                              << " C, SOC=" << data.soc_percent << "%";
                    if (!result.issues.empty()) {
                        std::cout << " | ";
                        for (std::size_t i = 0; i < result.issues.size(); ++i) {
                            if (i) std::cout << "; ";
                            std::cout << result.issues[i];
                        }
                    }
                    std::cout << '\n';
                } catch (const std::exception& ex) {
                    std::cerr << "Processing/logging error: " << ex.what() << '\n';
                }
            }
        });

        std::cout << "AutoSense Guard server listening on 127.0.0.1:" << port << '\n'
                  << "CSV log: logs/sensor_data.csv\n"
                  << "Start sensor_simulator in another Terminal. Press Ctrl+C to stop.\n";

        while (!stop_requested) {
            sockaddr_in client_address{};
            socklen_t client_length = sizeof(client_address);
            const int client_fd = ::accept(server_fd,
                                           reinterpret_cast<sockaddr*>(&client_address),
                                           &client_length);
            if (client_fd < 0) {
                if (errno == EINTR && stop_requested) break;
                if (errno == EINTR) continue;
                if (stop_requested) break;
                std::cerr << "accept() failed: " << std::strerror(errno) << '\n';
                continue;
            }

            char client_ip[INET_ADDRSTRLEN]{};
            (void)::inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));
            std::cout << "Client connected: " << client_ip << ':'
                      << ntohs(client_address.sin_port) << '\n';

            std::string pending;
            char buffer[2048];
            while (!stop_requested) {
                const ssize_t received = ::recv(client_fd, buffer, sizeof(buffer), 0);
                if (received == 0) {
                    std::cout << "Client disconnected.\n";
                    break;
                }
                if (received < 0) {
                    if (errno == EINTR && stop_requested) break;
                    if (errno == EINTR) continue;
                    std::cerr << "recv() failed: " << std::strerror(errno) << '\n';
                    break;
                }

                pending.append(buffer, static_cast<std::size_t>(received));
                if (pending.size() > 64 * 1024) {
                    std::cerr << "Protocol buffer exceeded 64 KiB; disconnecting client.\n";
                    pending.clear();
                    break;
                }

                std::size_t newline = 0;
                while ((newline = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, newline);
                    pending.erase(0, newline + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;

                    ++received_lines;
                    std::string error;
                    auto sample = autosense::parse_sample(line, &error);
                    if (!sample.has_value()) {
                        ++rejected_count;
                        std::cerr << "[PROTOCOL REJECTED] " << error << " | line=" << line << '\n';
                        continue;
                    }
                    ++parsed_count;
                    const auto push_result = queue.try_push(
                        QueuedSample{*sample, std::chrono::steady_clock::now()});
                    if (push_result == autosense::PushResult::DroppedOldest) {
                        ++dropped_count;
                    } else if (push_result == autosense::PushResult::Closed) {
                        std::cerr << "Processing queue is closed; dropping sample.\n";
                    }
                }
            }
            ::close(client_fd);
        }

        queue.close();
        if (processor.joinable()) processor.join();
        ::close(server_fd);
        std::sort(queue_wait_us.begin(), queue_wait_us.end());
        std::cout << "Server stopped."
                  << " lines=" << received_lines
                  << ", parsed=" << parsed_count
                  << ", rejected=" << rejected_count
                  << ", dropped=" << dropped_count
                  << ", processed=" << processed_count
                  << ", warning=" << warning_count
                  << ", critical=" << critical_count
                  << ", queue_wait_p50=" << percentile_us(queue_wait_us, 0.50) << "us"
                  << ", queue_wait_p99=" << percentile_us(queue_wait_us, 0.99) << "us"
                  << '\n';
    } catch (const std::exception& ex) {
        ::close(server_fd);
        std::cerr << "Startup/runtime error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
