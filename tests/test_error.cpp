#include <array>
#include <cassert>
#include <cstring>

#include <st2110/foundation/error.hpp>

namespace {
using st2110::Error;

constexpr std::array<Error, 14> kKnownErrors = {
    Error::Ok,
    Error::BufferTooSmall,
    Error::InvalidValue,
    Error::Unsupported,
    Error::ShortPacket,
    Error::BadRTPVersion,
    Error::InvalidBackendState,
    Error::SystemFailure,
    Error::BindFailed,
    Error::MulticastJoinFailed,
    Error::MulticastLeaveFailed,
    Error::ReceiveFailed,
    Error::ReceiveInterrupted,
    Error::ReceiveAborted,
};

void test_non_empty_strings_for_all_known_errors() {
    for (Error error : kKnownErrors) {
        const char *text = st2110::to_string(error);
        assert(text != nullptr);
        assert(std::strlen(text) > 0);
    }
}

void test_distinct_strings_for_all_known_errors() {
    for (std::size_t i = 0; i < kKnownErrors.size(); ++i) {
        for (std::size_t j = i + 1; j < kKnownErrors.size(); ++j) {
            const char *lhs = st2110::to_string(kKnownErrors[i]);
            const char *rhs = st2110::to_string(kKnownErrors[j]);
            assert(std::strcmp(lhs, rhs) != 0);
        }
    }
}

void test_backend_runtime_error_classification() {
    assert(!st2110::is_backend_runtime_error(Error::Ok));
    assert(!st2110::is_backend_runtime_error(Error::BufferTooSmall));
    assert(!st2110::is_backend_runtime_error(Error::InvalidValue));
    assert(!st2110::is_backend_runtime_error(Error::Unsupported));
    assert(!st2110::is_backend_runtime_error(Error::ShortPacket));
    assert(!st2110::is_backend_runtime_error(Error::BadRTPVersion));

    assert(st2110::is_backend_runtime_error(Error::InvalidBackendState));
    assert(st2110::is_backend_runtime_error(Error::SystemFailure));
    assert(st2110::is_backend_runtime_error(Error::BindFailed));
    assert(st2110::is_backend_runtime_error(Error::MulticastJoinFailed));
    assert(st2110::is_backend_runtime_error(Error::MulticastLeaveFailed));
    assert(st2110::is_backend_runtime_error(Error::ReceiveFailed));
    assert(st2110::is_backend_runtime_error(Error::ReceiveInterrupted));
    assert(st2110::is_backend_runtime_error(Error::ReceiveAborted));
}

void test_unknown_error_does_not_render_as_ok() {
    const auto unknown = static_cast<Error>(9999);

    const char *unknown_text = st2110::to_string(unknown);
    const char *ok_text = st2110::to_string(Error::Ok);

    assert(unknown_text != nullptr);
    assert(std::strlen(unknown_text) > 0);
    assert(std::strcmp(unknown_text, ok_text) != 0);

    assert(!st2110::is_backend_runtime_error(unknown));
}
} // namespace

int main() {
    test_non_empty_strings_for_all_known_errors();
    test_distinct_strings_for_all_known_errors();
    test_backend_runtime_error_classification();
    test_unknown_error_does_not_render_as_ok();
    return 0;
}