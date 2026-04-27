#include "session.hpp"

#include <array>

std::optional<std::vector<uint8_t>> TftpSession::handle(const TftpPacket& pkt) {
    if (auto* p = std::get_if<RrqPacket>(&pkt))
        return on_rrq(*p);
    if (auto* p = std::get_if<AckPacket>(&pkt))
        return on_ack(*p);
    if (std::get_if<ErrorPacket>(&pkt)) {
        state_ = State::Done;
        return std::nullopt;
    }
    return serialize(ErrorPacket{ErrorCode::IllegalOperation, "unexpected packet"});
}

std::optional<std::vector<uint8_t>> TftpSession::on_rrq(const RrqPacket& req) {
    if (state_ != State::Idle)
        return serialize(ErrorPacket{ErrorCode::IllegalOperation, "transfer already in progress"});

    stream_ = files_.open(req.filename);
    if (!stream_)
        return serialize(ErrorPacket{ErrorCode::FileNotFound, "file not found"});

    state_ = State::SendingFile;
    block_ = 1;
    auto data = read_block();
    final_block_sent_ = data.size() < BLOCK_SIZE;
    return serialize(DataPacket{block_, std::move(data)});
}

std::optional<std::vector<uint8_t>> TftpSession::on_ack(const AckPacket& ack) {
    if (state_ != State::SendingFile)
        return serialize(ErrorPacket{ErrorCode::IllegalOperation, "unexpected ACK"});

    if (ack.block != block_)
        return std::nullopt; // ignore out-of-order ACK

    if (final_block_sent_) {
        state_ = State::Done;
        return std::nullopt;
    }

    ++block_;
    auto data = read_block();
    final_block_sent_ = data.size() < BLOCK_SIZE;
    return serialize(DataPacket{block_, std::move(data)});
}

std::vector<uint8_t> TftpSession::read_block() {
    std::array<uint8_t, BLOCK_SIZE> buf;
    stream_->read(reinterpret_cast<char*>(buf.data()), BLOCK_SIZE);
    size_t n = static_cast<size_t>(stream_->gcount());
    return {buf.data(), buf.data() + n};
}
