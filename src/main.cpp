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

static void usage(const char* prog) {
    std::cout << "Usage: " << prog << " [root_dir] [port]\n"
              << "\n"
              << "  root_dir  directory to serve files from (default: .)\n"
              << "  port      UDP port to listen on       (default: 69, requires root)\n"
              << "\n"
              << "Examples:\n"
              << "  " << prog << "                      # serve . on port 69\n"
              << "  " << prog << " /srv/tftp             # serve /srv/tftp on port 69\n"
              << "  " << prog << " ./files 6969          # serve ./files on port 6969\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        usage(argv[0]);
        return 0;
    }

    std::string root_dir  = argc > 1 ? argv[1] : ".";
    uint16_t    port      = argc > 2 ? static_cast<uint16_t>(std::stoi(argv[2])) : 69;

    LocalFileProvider files(root_dir);
    UdpSocket sock;
    try {
        sock.bind(port);
    } catch (const std::runtime_error&) {
        std::cerr << "error: cannot bind to port " << port;
        if (port < 1024)
            std::cerr << " (ports below 1024 require root, try: sudo " << argv[0] << ")";
        std::cerr << "\n";
        return 1;
    }

    std::cout << "TFTP server listening on port " << port << ", serving: " << root_dir << "\n";

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
