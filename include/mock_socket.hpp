#pragma once

#include "socket.hpp"
#include <cstdint>
#include <cstring>
#include <queue>
#include <utility>
#include <vector>

struct SocketEvent {
    enum class Type { Bind, Send, Recv };
    Type type;
    std::vector<uint8_t> data;
    sockaddr_storage addr;
    uint16_t port;
};

struct Packet {
    std::vector<uint8_t> data;
    sockaddr_storage addr;
};

class MockSocket : public ISocket {
public:
    std::vector<SocketEvent> events;
    std::queue<Packet> rx_queue;

    void bind(uint16_t port) override {
        events.push_back({SocketEvent::Type::Bind, {}, {}, port});
    }

    ssize_t sendto(const void* buf, size_t len, const sockaddr_storage& addr) override {
        auto* p = static_cast<const uint8_t*>(buf);
        events.push_back({SocketEvent::Type::Send, {p, p + len}, addr, 0});
        return static_cast<ssize_t>(len);
    }

    ssize_t recvfrom(void* buf, size_t len, sockaddr_storage& addr) override {
        if (rx_queue.empty())
            return -1;
        auto pkt = rx_queue.front();
        rx_queue.pop();
        size_t n = std::min(len, pkt.data.size());
        std::memcpy(buf, pkt.data.data(), n);
        addr = pkt.addr;
        events.push_back({SocketEvent::Type::Recv, pkt.data, pkt.addr, 0});
        return static_cast<ssize_t>(n);
    }

    void push_recv(std::vector<uint8_t> data, sockaddr_storage addr = {}) {
        rx_queue.push({std::move(data), addr});
    }
};
