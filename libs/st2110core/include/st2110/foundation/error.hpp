#ifndef ST2110_OBS_PLUGIN_ERROR_HPP
#define ST2110_OBS_PLUGIN_ERROR_HPP

namespace st2110 {
enum class Error {
    Ok,
    BufferTooSmall,
    InvalidValue,
    Unsupported,
    ShortPacket,
    BadRTPVersion,
    InvalidBackendState,
    SystemFailure,
    OperationInterrupted,
    OperationAborted,
};

inline const char *to_string(Error error) {
    switch (error) {
    case Error::BufferTooSmall:
        return "ERROR: Buffer too small!";
    case Error::InvalidValue:
        return "ERROR: The value is invalid!";
    case Error::Unsupported:
        return "ERROR: Unsupported functionality!";
    case Error::ShortPacket:
        return "ERROR: The packet is too small!";
    case Error::BadRTPVersion:
        return "ERROR: Invalid RTP Header version!";
    case Error::InvalidBackendState:
        return "ERROR: Invalid backend state!";
    case Error::SystemFailure:
        return "ERROR: System failure!";
    case Error::OperationInterrupted:
        return "ERROR: Operation interrupted!";
    case Error::OperationAborted:
        return "ERROR: Operation aborted!";
    case Error::Ok:
        return "OK";
    default:
        return "ERROR: Unknown error!";
    }
}

inline bool is_backend_runtime_error(Error error) noexcept {
    switch (error) {
    case Error::InvalidBackendState:
    case Error::SystemFailure:
    case Error::OperationInterrupted:
    case Error::OperationAborted:
        return true;
    default:
        return false;
    }
}
} // namespace st2110

#endif // ST2110_OBS_PLUGIN_ERROR_HPP