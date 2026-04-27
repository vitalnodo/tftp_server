#pragma once

#include <istream>
#include <memory>
#include <optional>
#include <ostream>
#include <vector>

#include "file_provider.hpp"
#include "tftp.hpp"

class TftpSession {
public:
    enum class State { Idle, SendingFile, ReceivingFile, Done };

    explicit TftpSession(IFileProvider& files) : files_(files) {}

    State state() const { return state_; }

    std::optional<std::vector<uint8_t>> handle(const TftpPacket& pkt);

private:
    IFileProvider&                 files_;
    State                          state_ = State::Idle;
    std::unique_ptr<std::istream>  read_stream_;
    std::unique_ptr<std::ostream>  write_stream_;
    uint16_t                       block_ = 0;
    bool                           final_block_sent_ = false;

    std::optional<std::vector<uint8_t>> on_rrq(const RrqPacket& pkt);
    std::optional<std::vector<uint8_t>> on_ack(const AckPacket& pkt);
    std::optional<std::vector<uint8_t>> on_wrq(const WrqPacket& pkt);
    std::optional<std::vector<uint8_t>> on_data(const DataPacket& pkt);
    std::vector<uint8_t> read_block();
    std::vector<uint8_t> err(ErrorCode code, const std::string& msg);
};
