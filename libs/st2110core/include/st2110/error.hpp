#ifndef ST2110_OBS_PLUGIN_ERROR_HPP
#define ST2110_OBS_PLUGIN_ERROR_HPP

namespace st2110 {
enum class Error {
    Ok, BufferTooSmall, InvalidValue, Unsupported, ShortPacket, BadRTPVersion
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
    default:
      return "OK";
  }
}

}

#endif // ST2110_OBS_PLUGIN_ERROR_HPP
