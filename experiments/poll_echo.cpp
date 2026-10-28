#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

int main() {
    const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::perror("socket");
        return 1;
    }

    int yes = 1;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9100);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(listen_fd);
        return 1;
    }
    if (::listen(listen_fd, 16) < 0) {
        std::perror("listen");
        ::close(listen_fd);
        return 1;
    }

    std::vector<pollfd> fds;
    fds.push_back(pollfd{listen_fd, POLLIN, 0});
    std::cout << "poll echo server on 127.0.0.1:9100\n";

    while (true) {
        const int ready = ::poll(fds.data(), fds.size(), -1);
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::perror("poll");
            break;
        }

        for (std::size_t i = 0; i < fds.size(); ++i) {
            if ((fds[i].revents & POLLIN) == 0) continue;

            if (fds[i].fd == listen_fd) {
                sockaddr_in client{};
                socklen_t len = sizeof(client);
                const int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&client), &len);
                if (cfd >= 0) {
                    std::cout << "client connected: fd=" << cfd << '\n';
                    fds.push_back(pollfd{cfd, POLLIN, 0});
                }
                continue;
            }

            char buffer[1024];
            const ssize_t n = ::recv(fds[i].fd, buffer, sizeof(buffer), 0);
            if (n <= 0) {
                std::cout << "client disconnected: fd=" << fds[i].fd << '\n';
                ::close(fds[i].fd);
                fds.erase(fds.begin() + static_cast<long>(i));
                --i;
                continue;
            }
            (void)::send(fds[i].fd, buffer, static_cast<std::size_t>(n), 0);
        }
    }

    for (const auto& p : fds) {
        ::close(p.fd);
    }
    return 0;
}
