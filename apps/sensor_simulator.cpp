#include "autosense/protocol.hpp"
#include "autosense/types.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {

bool parse_positive_int(const char* text, int& value, int min_value, int max_value) {
    try {
        std::size_t used = 0;
        const long parsed = std::stol(text, &used);
        if (used != std::strlen(text) || parsed < min_value || parsed > max_value) return false;
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

int connect_to_server(const std::string& host, const std::string& port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;
    const int lookup = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
    if (lookup != 0) {
        std::cerr << "getaddrinfo failed: " << gai_strerror(lookup) << '\n';
        return -1;
    }

    int connected_fd = -1;
    for (addrinfo* item = results; item != nullptr; item = item->ai_next) {
        int fd = ::socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (fd < 0) continue;
#ifdef SO_NOSIGPIPE
        int enabled = 1;
        (void)::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
        if (::connect(fd, item->ai_addr, item->ai_addrlen) == 0) {
            connected_fd = fd;
            break;
        }
        ::close(fd);
    }
    ::freeaddrinfo(results);
    if (connected_fd < 0) {
        std::cerr << "Could not connect to " << host << ':' << port
                  << ". Start autosense_server first.\n";
    }
    return connected_fd;
}

bool send_all(int fd, const std::string& message) {
    std::size_t sent = 0;
    while (sent < message.size()) {
#ifdef MSG_NOSIGNAL
        const ssize_t count = ::send(fd, message.data() + sent, message.size() - sent, MSG_NOSIGNAL);
#else
        const ssize_t count = ::send(fd, message.data() + sent, message.size() - sent, 0);
#endif
        if (count < 0) {
            if (errno == EINTR) continue;
            std::cerr << "send() failed: " << std::strerror(errno) << '\n';
            return false;
        }
        if (count == 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 1 || argc > 6) {
        std::cerr << "Usage: " << argv[0]
                  << " [host=127.0.0.1] [port=9000] [count=30] [interval_ms=500] [--faults]\n";
        return 2;
    }

    const std::string host = argc >= 2 ? argv[1] : "127.0.0.1";
    int port = 9000;
    int count = 30;
    int interval_ms = 500;
    bool inject_faults = false;

    if (argc >= 3 && !parse_positive_int(argv[2], port, 1, 65535)) {
        std::cerr << "Invalid port. Expected 1-65535.\n";
        return 2;
    }
    if (argc >= 4 && !parse_positive_int(argv[3], count, 1, 1000000)) {
        std::cerr << "Invalid count. Expected 1-1000000.\n";
        return 2;
    }
    if (argc >= 5 && !parse_positive_int(argv[4], interval_ms, 1, 60000)) {
        std::cerr << "Invalid interval. Expected 1-60000 milliseconds.\n";
        return 2;
    }
    if (argc >= 6) {
        inject_faults = std::string(argv[5]) == "--faults";
        if (!inject_faults) {
            std::cerr << "Unknown option: " << argv[5] << " (supported: --faults)\n";
            return 2;
        }
    }

    const int socket_fd = connect_to_server(host, std::to_string(port));
    if (socket_fd < 0) return 1;

    std::cout << "Connected to " << host << ':' << port
              << ". Sending " << count << " samples; fault injection="
              << (inject_faults ? "ON" : "OFF") << '\n';

    for (int i = 0; i < count; ++i) {
        autosense::SensorData data;
        // Create a sequence gap every 10th sample when fault injection is enabled.
        data.sequence = static_cast<std::uint64_t>(i + 1);
        if (inject_faults && i >= 10) {
            data.sequence += static_cast<std::uint64_t>(1 + (i - 10) / 10);
        }
        data.timestamp_ms = now_ms();
        data.speed_kmh = 45.0 + 35.0 * std::sin(i * 0.23);
        data.voltage_v = 365.0 + 12.0 * std::sin(i * 0.11);
        data.temperature_c = 34.0 + 3.0 * std::sin(i * 0.17);
        data.soc_percent = 82 - i / 5;
        data.accel_x_mps2 = 0.7 * std::sin(i * 0.4);
        data.accel_y_mps2 = 0.5 * std::cos(i * 0.3);
        data.accel_z_mps2 = 9.81 + 0.15 * std::sin(i * 0.2);

        if (inject_faults && (i + 1) % 7 == 0) {
            data.temperature_c = 65.0;  // Demonstrate the critical temperature alarm.
        }
        if (inject_faults && (i + 1) % 11 == 0) {
            data.soc_percent = 105;  // Demonstrate invalid sensor values.
        }

        const std::string packet = autosense::serialize_sample(data);
        if (!send_all(socket_fd, packet)) {
            ::close(socket_fd);
            return 1;
        }
        std::cout << "Sent sample " << (i + 1) << '/' << count
                  << " (seq=" << data.sequence << ")\n";
        if (i + 1 < count) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }

    ::shutdown(socket_fd, SHUT_WR);
    ::close(socket_fd);
    std::cout << "Finished sending samples. Check logs/sensor_data.csv on the server.\n";
    return 0;
}
