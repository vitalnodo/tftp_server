#include <array>
#include <iostream>

#include "file_provider.hpp"
#include "session.hpp"
#include "socket.hpp"
#include "tftp.hpp"

static constexpr size_t RECV_BUF_SIZE = 1024;

static void run_session(ISocket& sock, const sockaddr_in& client,
                        const TftpPacket& initial, IFileProvider& files) {
    TftpSession session(files);
    std::array<uint8_t, RECV_BUF_SIZE> buf;

    auto response = session.handle(initial);

    while (response) {
        sock.sendto(response->data(), response->size(), client);

        if (session.state() == TftpSession::State::Done)
            break;

        sockaddr_in from{};
        ssize_t n = sock.recvfrom(buf.data(), buf.size(), from);
        if (n < 0)
            break;

        try {
            auto pkt = parse(buf.data(), static_cast<size_t>(n));
            response = session.handle(pkt);
        } catch (const std::exception& e) {
            std::cerr << "parse error: " << e.what() << "\n";
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string root_dir = argc > 1 ? argv[1] : ".";

    LocalFileProvider files(root_dir);
    UdpSocket sock;
    sock.bind(69);

    std::cout << "TFTP server listening on port 69, serving: " << root_dir << "\n";

    std::array<uint8_t, RECV_BUF_SIZE> buf;
    sockaddr_in client{};

    while (true) {
        ssize_t n = sock.recvfrom(buf.data(), buf.size(), client);
        if (n < 0)
            continue;

        try {
            auto pkt = parse(buf.data(), static_cast<size_t>(n));
            run_session(sock, client, pkt, files);
        } catch (const std::exception& e) {
            std::cerr << "error: " << e.what() << "\n";
        }
    }
}
