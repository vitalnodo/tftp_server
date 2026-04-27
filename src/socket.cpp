#include "socket.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

UdpSocket::UdpSocket() {
    fd_ = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd_ < 0)
        throw std::runtime_error("socket() failed");

    int opt = 0;
    ::setsockopt(fd_, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
}

UdpSocket::~UdpSocket() {
    ::close(fd_);
}

void UdpSocket::bind(uint16_t port) {
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr   = in6addr_any;
    addr.sin6_port   = htons(port);
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
}

ssize_t UdpSocket::sendto(const void* buf, size_t len, const sockaddr_storage& addr) {
    return ::sendto(fd_, buf, len, 0,
                    reinterpret_cast<const sockaddr*>(&addr), sizeof(sockaddr_in6));
}

ssize_t UdpSocket::recvfrom(void* buf, size_t len, sockaddr_storage& addr) {
    socklen_t addrlen = sizeof(addr);
    return ::recvfrom(fd_, buf, len, 0,
                      reinterpret_cast<sockaddr*>(&addr), &addrlen);
}

void UdpSocket::set_recv_timeout(int seconds) {
    struct timeval tv{};
    tv.tv_sec  = seconds;
    tv.tv_usec = 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        throw std::runtime_error("setsockopt(SO_RCVTIMEO) failed");
}
