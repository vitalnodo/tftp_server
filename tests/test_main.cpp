#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "file_provider.hpp"
#include "session.hpp"
#include "tftp.hpp"

// --- helpers ---

static uint16_t pkt_opcode(const std::vector<uint8_t>& pkt) {
    return (uint16_t(pkt[0]) << 8) | pkt[1];
}

static uint16_t pkt_block(const std::vector<uint8_t>& pkt) {
    return (uint16_t(pkt[2]) << 8) | pkt[3];
}

static std::vector<uint8_t> pkt_data(const std::vector<uint8_t>& pkt) {
    return {pkt.begin() + 4, pkt.end()};
}

static std::vector<uint8_t> bytes(const std::string& s) {
    return {s.begin(), s.end()};
}

// --- session RRQ tests ---

static void test_rrq_returns_data() {
    MockFileProvider files;
    files.add_file("hello.txt", bytes("hello"));
    TftpSession session(files);

    auto resp = session.handle(RrqPacket{"hello.txt", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 3); // DATA
    assert(pkt_block(*resp) == 1);
    assert(pkt_data(*resp) == bytes("hello"));
}

static void test_rrq_file_not_found() {
    MockFileProvider files;
    TftpSession session(files);

    auto resp = session.handle(RrqPacket{"missing.txt", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 5); // ERROR
    assert(pkt_block(*resp) == 1);  // ErrorCode::FileNotFound
}

static void test_rrq_path_traversal() {
    MockFileProvider files;
    TftpSession session(files);

    auto resp = session.handle(RrqPacket{"../etc/passwd", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 5); // ERROR
}

static void test_rrq_exact_block_boundary() {
    // 512 bytes → server sends DATA(512) then DATA(0) to signal EOF
    MockFileProvider files;
    files.add_file("block.bin", std::vector<uint8_t>(512, 0xAB));
    TftpSession session(files);

    auto resp = session.handle(RrqPacket{"block.bin", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 3);
    assert(pkt_block(*resp) == 1);
    assert(pkt_data(*resp).size() == 512);
    assert(session.state() == TftpSession::State::SendingFile);

    // ACK block 1 → server should send empty DATA(2)
    resp = session.handle(AckPacket{1});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 3);
    assert(pkt_block(*resp) == 2);
    assert(pkt_data(*resp).empty());

    // ACK block 2 → done
    resp = session.handle(AckPacket{2});
    assert(!resp.has_value());
    assert(session.state() == TftpSession::State::Done);
}

static void test_rrq_multiblock() {
    std::vector<uint8_t> content(1024 + 3, 0x42); // 2 full blocks + 3 bytes
    MockFileProvider files;
    files.add_file("big.bin", content);
    TftpSession session(files);

    auto resp = session.handle(RrqPacket{"big.bin", "octet"});
    assert(resp.has_value());
    assert(pkt_block(*resp) == 1);
    assert(pkt_data(*resp).size() == 512);

    resp = session.handle(AckPacket{1});
    assert(resp.has_value());
    assert(pkt_block(*resp) == 2);
    assert(pkt_data(*resp).size() == 512);

    resp = session.handle(AckPacket{2});
    assert(resp.has_value());
    assert(pkt_block(*resp) == 3);
    assert(pkt_data(*resp).size() == 3);

    resp = session.handle(AckPacket{3});
    assert(!resp.has_value());
    assert(session.state() == TftpSession::State::Done);
}

static void test_rrq_out_of_order_ack_ignored() {
    MockFileProvider files;
    files.add_file("hello.txt", bytes("hello"));
    TftpSession session(files);

    session.handle(RrqPacket{"hello.txt", "octet"}); // DATA(1)
    auto resp = session.handle(AckPacket{99});        // wrong block
    assert(!resp.has_value());
    assert(session.state() == TftpSession::State::SendingFile);
}

// --- session WRQ tests ---

static void test_wrq_sends_ack0() {
    MockFileProvider files;
    TftpSession session(files);

    auto resp = session.handle(WrqPacket{"new.txt", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 4); // ACK
    assert(pkt_block(*resp) == 0);
    assert(session.state() == TftpSession::State::ReceivingFile);
}

static void test_wrq_file_already_exists() {
    MockFileProvider files;
    files.add_file("exists.txt", bytes("old content"));
    TftpSession session(files);

    auto resp = session.handle(WrqPacket{"exists.txt", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 5); // ERROR
    assert(pkt_block(*resp) == 6);  // ErrorCode::FileAlreadyExists
}

static void test_wrq_path_traversal() {
    MockFileProvider files;
    TftpSession session(files);

    auto resp = session.handle(WrqPacket{"../evil.txt", "octet"});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 5); // ERROR
}

static void test_wrq_receives_data() {
    MockFileProvider files;
    TftpSession session(files);

    session.handle(WrqPacket{"upload.txt", "octet"}); // ACK(0)

    auto resp = session.handle(DataPacket{1, bytes("hello")});
    assert(resp.has_value());
    assert(pkt_opcode(*resp) == 4); // ACK
    assert(pkt_block(*resp) == 1);
    assert(session.state() == TftpSession::State::Done);
}

static void test_wrq_multiblock() {
    MockFileProvider files;
    TftpSession session(files);

    session.handle(WrqPacket{"upload.bin", "octet"}); // ACK(0)

    // send two full blocks + final partial
    auto resp = session.handle(DataPacket{1, std::vector<uint8_t>(512, 0x01)});
    assert(pkt_block(*resp) == 1);
    assert(session.state() == TftpSession::State::ReceivingFile);

    resp = session.handle(DataPacket{2, std::vector<uint8_t>(512, 0x02)});
    assert(pkt_block(*resp) == 2);
    assert(session.state() == TftpSession::State::ReceivingFile);

    resp = session.handle(DataPacket{3, bytes("end")});
    assert(pkt_block(*resp) == 3);
    assert(session.state() == TftpSession::State::Done);
}

static void test_rrq_block_wraparound() {
    // 65536 full blocks + 3 bytes crosses the block-number wrap (65535 → 0)
    std::vector<uint8_t> content(65536 * 512 + 3, 0x55);
    MockFileProvider files;
    files.add_file("big.bin", content);
    TftpSession session(files);

    auto resp = session.handle(RrqPacket{"big.bin", "octet"});

    for (uint32_t i = 1; i <= 65536; ++i) {
        assert(resp.has_value());
        assert(pkt_data(*resp).size() == 512);
        uint16_t blk = static_cast<uint16_t>(i); // wraps at 65536 → 0
        assert(pkt_block(*resp) == blk);
        resp = session.handle(AckPacket{blk});
    }
    // final partial block (block 1 again after the wrap)
    assert(resp.has_value());
    assert(pkt_block(*resp) == 1);
    assert(pkt_data(*resp).size() == 3);
    resp = session.handle(AckPacket{1});
    assert(!resp.has_value());
    assert(session.state() == TftpSession::State::Done);
}

static void test_wrq_block_wraparound() {
    MockFileProvider files;
    TftpSession session(files);

    session.handle(WrqPacket{"big.bin", "octet"}); // ACK(0)

    std::vector<uint8_t> full_block(512, 0x42);

    for (uint32_t i = 1; i <= 65536; ++i) {
        uint16_t blk = static_cast<uint16_t>(i); // wraps at 65536 → 0
        auto resp = session.handle(DataPacket{blk, full_block});
        assert(resp.has_value());
        assert(pkt_block(*resp) == blk);
        assert(session.state() == TftpSession::State::ReceivingFile);
    }
    // final partial block (block 1 again after the wrap)
    auto resp = session.handle(DataPacket{1, bytes("end")});
    assert(resp.has_value());
    assert(pkt_block(*resp) == 1);
    assert(session.state() == TftpSession::State::Done);
}

// --- parse / serialize tests ---

static void test_parse_rrq() {
    std::vector<uint8_t> raw = {0, 1, 'f', 'i', 'l', 'e', 0, 'o', 'c', 't', 'e', 't', 0};
    auto pkt = parse(raw.data(), raw.size());
    auto* rrq = std::get_if<RrqPacket>(&pkt);
    assert(rrq != nullptr);
    assert(rrq->filename == "file");
    assert(rrq->mode == "octet");
}

static void test_parse_ack() {
    std::vector<uint8_t> raw = {0, 4, 0, 7};
    auto pkt = parse(raw.data(), raw.size());
    auto* ack = std::get_if<AckPacket>(&pkt);
    assert(ack != nullptr);
    assert(ack->block == 7);
}

static void test_serialize_data_roundtrip() {
    DataPacket orig{42, {1, 2, 3, 4}};
    auto raw = serialize(orig);
    auto pkt = parse(raw.data(), raw.size());
    auto* data = std::get_if<DataPacket>(&pkt);
    assert(data != nullptr);
    assert(data->block == 42);
    assert(data->data == orig.data);
}

static void test_parse_malformed_throws() {
    std::vector<uint8_t> raw = {0}; // too short
    bool threw = false;
    try {
        parse(raw.data(), raw.size());
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

// --- runner ---

#define RUN(test) \
    do { \
        test(); \
        std::cout << "PASS  " #test "\n"; \
    } while (0)

int main() {
    RUN(test_rrq_returns_data);
    RUN(test_rrq_file_not_found);
    RUN(test_rrq_path_traversal);
    RUN(test_rrq_exact_block_boundary);
    RUN(test_rrq_multiblock);
    RUN(test_rrq_out_of_order_ack_ignored);

    RUN(test_wrq_sends_ack0);
    RUN(test_wrq_file_already_exists);
    RUN(test_wrq_path_traversal);
    RUN(test_wrq_receives_data);
    RUN(test_wrq_multiblock);
    RUN(test_rrq_block_wraparound);
    RUN(test_wrq_block_wraparound);

    RUN(test_parse_rrq);
    RUN(test_parse_ack);
    RUN(test_serialize_data_roundtrip);
    RUN(test_parse_malformed_throws);

    std::cout << "\nAll tests passed.\n";
}
