#include "cansat_comms/common.hpp"

namespace cansat {

const char* to_string(Status status) {
    switch (status) {
        case Status::Ok: return "Ok";
        case Status::InvalidArgument: return "InvalidArgument";
        case Status::BufferTooSmall: return "BufferTooSmall";
        case Status::PayloadTooLarge: return "PayloadTooLarge";
        case Status::Incomplete: return "Incomplete";
        case Status::BadStartDelimiter: return "BadStartDelimiter";
        case Status::LengthMismatch: return "LengthMismatch";
        case Status::BadChecksum: return "BadChecksum";
        case Status::UnexpectedFrameType: return "UnexpectedFrameType";
        case Status::MalformedFrame: return "MalformedFrame";
        case Status::UnknownMessageType: return "UnknownMessageType";
        case Status::UnexpectedMessageType: return "UnexpectedMessageType";
        case Status::BadPayloadLength: return "BadPayloadLength";
        case Status::InvalidFieldValue: return "InvalidFieldValue";
    }
    return "Unknown";
}

}  // namespace cansat
