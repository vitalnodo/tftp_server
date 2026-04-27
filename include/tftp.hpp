#pragma once

// TFTP Protocol - RFC 1350
// https://datatracker.ietf.org/doc/html/rfc1350
//
// RFC 1350, Section 5 - Packet formats:
//
//  RRQ/WRQ:
//   2 bytes     string    1 byte     string   1 byte
//   ------------------------------------------------
//  | Opcode |  Filename  |   0  |    Mode    |   0  |
//   ------------------------------------------------
//
//  DATA:
//   2 bytes     2 bytes      n bytes
//   ----------------------------------
//  | Opcode |   Block #  |   Data     |
//   ----------------------------------
//
//  ACK:
//   2 bytes     2 bytes
//   ---------------------
//  | Opcode |   Block #  |
//   ---------------------
//
//  ERROR:
//   2 bytes     2 bytes      string    1 byte
//   -----------------------------------------
//  | Opcode |  ErrorCode |   ErrMsg   |   0  |
//   -----------------------------------------

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

// RFC 1350, Section 5: Opcodes
enum class Opcode : uint16_t {
    RRQ   = 1,
    WRQ   = 2,
    DATA  = 3,
    ACK   = 4,
    ERROR = 5,
};

// RFC 1350, Appendix I: Error Codes
enum class ErrorCode : uint16_t {
    NotDefined        = 0,
    FileNotFound      = 1,
    AccessViolation   = 2,
    DiskFull          = 3,
    IllegalOperation  = 4,
    UnknownTransferID = 5,
    FileAlreadyExists = 6,
    NoSuchUser        = 7,
};

// RFC 1350, Section 2: data blocks are 512 bytes
static constexpr size_t BLOCK_SIZE = 512;

// opcode(2) + block(2)
static constexpr size_t DATA_HEADER_SIZE = 4;

// opcode(2) + error_code(2)
static constexpr size_t ERROR_HEADER_SIZE = 4;

struct RrqPacket   { std::string filename; std::string mode; };
struct WrqPacket   { std::string filename; std::string mode; };
struct DataPacket  { uint16_t block; std::vector<uint8_t> data; };
struct AckPacket   { uint16_t block; };
struct ErrorPacket { ErrorCode code; std::string message; };

using TftpPacket = std::variant<RrqPacket, WrqPacket, DataPacket, AckPacket, ErrorPacket>;

inline TftpPacket parse(const uint8_t* buf, size_t len) {
    if (len < 2)
        throw std::runtime_error("packet too short to read opcode");

    auto opcode = static_cast<Opcode>((uint16_t(buf[0]) << 8) | buf[1]);

    switch (opcode) {
    case Opcode::RRQ:
    case Opcode::WRQ: {
        // minimum: opcode(2) + at least one char + null + at least one char + null
        if (len < 6)
            throw std::runtime_error("RRQ/WRQ too short");

        const char* p   = reinterpret_cast<const char*>(buf + 2);
        const char* end = reinterpret_cast<const char*>(buf + len);

        const char* null1 = static_cast<const char*>(std::memchr(p, 0, end - p));
        if (!null1)
            throw std::runtime_error("RRQ/WRQ: missing filename terminator");

        std::string filename(p, null1);
        if (filename.empty())
            throw std::runtime_error("RRQ/WRQ: empty filename");

        const char* mode_start = null1 + 1;
        if (mode_start >= end)
            throw std::runtime_error("RRQ/WRQ: missing mode field");

        const char* null2 = static_cast<const char*>(std::memchr(mode_start, 0, end - mode_start));
        if (!null2)
            throw std::runtime_error("RRQ/WRQ: missing mode terminator");

        std::string mode(mode_start, null2);
        if (mode.empty())
            throw std::runtime_error("RRQ/WRQ: empty mode");

        if (opcode == Opcode::RRQ)
            return RrqPacket{std::move(filename), std::move(mode)};
        return WrqPacket{std::move(filename), std::move(mode)};
    }
    case Opcode::DATA: {
        if (len < DATA_HEADER_SIZE)
            throw std::runtime_error("DATA too short");
        if (len > DATA_HEADER_SIZE + BLOCK_SIZE)
            throw std::runtime_error("DATA block exceeds 512 bytes");
        uint16_t block = (uint16_t(buf[2]) << 8) | buf[3];
        return DataPacket{block, {buf + DATA_HEADER_SIZE, buf + len}};
    }
    case Opcode::ACK: {
        if (len != 4)
            throw std::runtime_error("ACK must be exactly 4 bytes");
        uint16_t block = (uint16_t(buf[2]) << 8) | buf[3];
        return AckPacket{block};
    }
    case Opcode::ERROR: {
        if (len < ERROR_HEADER_SIZE + 1)
            throw std::runtime_error("ERROR too short");
        auto code    = static_cast<ErrorCode>((uint16_t(buf[2]) << 8) | buf[3]);
        const char* msg     = reinterpret_cast<const char*>(buf + ERROR_HEADER_SIZE);
        const char* msg_end = reinterpret_cast<const char*>(buf + len);
        const char* null    = static_cast<const char*>(std::memchr(msg, 0, msg_end - msg));
        if (!null)
            throw std::runtime_error("ERROR: missing message terminator");
        return ErrorPacket{code, std::string(msg, null)};
    }
    default:
        throw std::runtime_error("unknown opcode");
    }
}

inline std::vector<uint8_t> serialize(const DataPacket& pkt) {
    if (pkt.data.size() > BLOCK_SIZE)
        throw std::runtime_error("DATA block exceeds 512 bytes");
    std::vector<uint8_t> buf(DATA_HEADER_SIZE + pkt.data.size());
    buf[0] = 0x00; buf[1] = uint8_t(Opcode::DATA);
    buf[2] = pkt.block >> 8; buf[3] = pkt.block & 0xFF;
    std::memcpy(buf.data() + DATA_HEADER_SIZE, pkt.data.data(), pkt.data.size());
    return buf;
}

inline std::vector<uint8_t> serialize(const AckPacket& pkt) {
    return {0x00, uint8_t(Opcode::ACK),
            uint8_t(pkt.block >> 8), uint8_t(pkt.block & 0xFF)};
}

inline std::vector<uint8_t> serialize(const ErrorPacket& pkt) {
    size_t msgsize = pkt.message.size() + 1;
    std::vector<uint8_t> buf(ERROR_HEADER_SIZE + msgsize);
    buf[0] = 0x00; buf[1] = uint8_t(Opcode::ERROR);
    buf[2] = uint16_t(pkt.code) >> 8; buf[3] = uint16_t(pkt.code) & 0xFF;
    std::memcpy(buf.data() + ERROR_HEADER_SIZE, pkt.message.c_str(), msgsize);
    return buf;
}
