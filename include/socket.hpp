#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

class ISocket {
public:
    virtual ~ISocket() = default;
    virtual void bind(uint16_t port) = 0;
    virtual ssize_t sendto(const void* buf, size_t len, const sockaddr_storage& addr) = 0;
    virtual ssize_t recvfrom(void* buf, size_t len, sockaddr_storage& addr) = 0;
};

class UdpSocket : public ISocket {
public:
    UdpSocket();
    ~UdpSocket();

    void bind(uint16_t port) override;
    ssize_t sendto(const void* buf, size_t len, const sockaddr_storage& addr) override;
    ssize_t recvfrom(void* buf, size_t len, sockaddr_storage& addr) override;

private:
    int fd_;
};
