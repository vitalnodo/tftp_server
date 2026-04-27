#include "socket.hpp"

#include <arpa/inet.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

UdpSocket::UdpSocket() {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0)
        throw std::runtime_error("socket() failed");
}

UdpSocket::~UdpSocket() {
    ::close(fd_);
}

void UdpSocket::bind(uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
}

ssize_t UdpSocket::sendto(const void* buf, size_t len, const sockaddr_in& addr) {
    return ::sendto(fd_, buf, len, 0,
                    reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

ssize_t UdpSocket::recvfrom(void* buf, size_t len, sockaddr_in& addr) {
    socklen_t addrlen = sizeof(addr);
    return ::recvfrom(fd_, buf, len, 0,
                      reinterpret_cast<sockaddr*>(&addr), &addrlen);
}
