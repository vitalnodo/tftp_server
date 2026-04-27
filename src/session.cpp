#include "session.hpp"

#include <array>

std::optional<std::vector<uint8_t>> TftpSession::handle(const TftpPacket& pkt) {
    if (auto* p = std::get_if<RrqPacket>(&pkt))
        return on_rrq(*p);
    if (auto* p = std::get_if<WrqPacket>(&pkt))
        return on_wrq(*p);
    if (auto* p = std::get_if<AckPacket>(&pkt))
        return on_ack(*p);
    if (auto* p = std::get_if<DataPacket>(&pkt))
        return on_data(*p);
    if (std::get_if<ErrorPacket>(&pkt)) {
        state_ = State::Done;
        return std::nullopt;
    }
    return err(ErrorCode::IllegalOperation, "unexpected packet");
}

std::optional<std::vector<uint8_t>> TftpSession::on_wrq(const WrqPacket& req) {
    if (state_ != State::Idle)
        return err(ErrorCode::IllegalOperation, "transfer already in progress");

    write_stream_ = files_.create(req.filename);
    if (!write_stream_)
        return err(ErrorCode::FileAlreadyExists, "file already exists or access denied");

    state_ = State::ReceivingFile;
    block_ = 0;
    return serialize(AckPacket{0});
}

std::optional<std::vector<uint8_t>> TftpSession::on_data(const DataPacket& pkt) {
    if (state_ != State::ReceivingFile)
        return err(ErrorCode::IllegalOperation, "unexpected DATA");

    if (pkt.block != static_cast<uint16_t>(block_ + 1))
        return std::nullopt; // out-of-order, ignore

    ++block_;
    write_stream_->write(reinterpret_cast<const char*>(pkt.data.data()),
                         static_cast<std::streamsize>(pkt.data.size()));

    if (pkt.data.size() < BLOCK_SIZE) {
        write_stream_.reset(); // flush + commit (triggers CaptureStream destructor in mock)
        state_ = State::Done;
    }

    return serialize(AckPacket{block_});
}

std::optional<std::vector<uint8_t>> TftpSession::on_rrq(const RrqPacket& req) {
    if (state_ != State::Idle)
        return err(ErrorCode::IllegalOperation, "transfer already in progress");

    read_stream_ = files_.open(req.filename);
    if (!read_stream_)
        return err(ErrorCode::FileNotFound, "file not found");

    state_ = State::SendingFile;
    block_ = 1;
    auto data = read_block();
    final_block_sent_ = data.size() < BLOCK_SIZE;
    return serialize(DataPacket{block_, std::move(data)});
}

std::optional<std::vector<uint8_t>> TftpSession::on_ack(const AckPacket& ack) {
    if (state_ != State::SendingFile)
        return err(ErrorCode::IllegalOperation, "unexpected ACK");

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

std::vector<uint8_t> TftpSession::err(ErrorCode code, const std::string& msg) {
    state_ = State::Done;
    return serialize(ErrorPacket{code, msg});
}

std::vector<uint8_t> TftpSession::read_block() {
    std::array<uint8_t, BLOCK_SIZE> buf;
    read_stream_->read(reinterpret_cast<char*>(buf.data()), BLOCK_SIZE);
    size_t n = static_cast<size_t>(read_stream_->gcount());
    return {buf.data(), buf.data() + n};
}
