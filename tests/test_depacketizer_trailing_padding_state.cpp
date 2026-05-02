#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <st2110/depacketizer.hpp>
#include <st2110/packet_view.hpp>

static std::vector<uint8_t> make_packet(uint16_t seq, uint32_t rtp_timestamp, bool marker, uint16_t srd_length,
                                        const std::vector<uint8_t> &segment_bytes,
                                        const std::vector<uint8_t> &trailing_bytes) {
    std::vector<uint8_t> p;

    // RTP header
    p.push_back(0x80);                                              // V=2
    p.push_back(static_cast<uint8_t>((marker ? 0x80 : 0x00) | 96)); // M + PT
    p.push_back(static_cast<uint8_t>((seq >> 8) & 0xFF));
    p.push_back(static_cast<uint8_t>(seq & 0xFF));

    p.push_back(static_cast<uint8_t>((rtp_timestamp >> 24) & 0xFF));
    p.push_back(static_cast<uint8_t>((rtp_timestamp >> 16) & 0xFF));
    p.push_back(static_cast<uint8_t>((rtp_timestamp >> 8) & 0xFF));
    p.push_back(static_cast<uint8_t>(rtp_timestamp & 0xFF));

    p.push_back(0x00);
    p.push_back(0x00);
    p.push_back(0x00);
    p.push_back(0x01); // ssrc

    // ST 2110-20 payload header: ext seq hi16 + 1 SRD
    p.push_back(0x00);
    p.push_back(0x00); // ext seq hi16

    p.push_back(static_cast<uint8_t>((srd_length >> 8) & 0xFF));
    p.push_back(static_cast<uint8_t>(srd_length & 0xFF));

    p.push_back(0x00);
    p.push_back(0x00); // F=0, row=0
    p.push_back(0x00);
    p.push_back(0x00); // C=0, offset=0

    p.insert(p.end(), segment_bytes.begin(), segment_bytes.end());
    p.insert(p.end(), trailing_bytes.begin(), trailing_bytes.end());

    return p;
}

static st2110::PacketView parse_packet(uint16_t seq, uint32_t rtp_timestamp, bool marker,
                                       const std::vector<uint8_t> &segment_bytes,
                                       const std::vector<uint8_t> &trailing_bytes) {
    const uint16_t srd_length = static_cast<uint16_t>(segment_bytes.size());

    const std::vector<uint8_t> bytes =
        make_packet(seq, rtp_timestamp, marker, srd_length, segment_bytes, trailing_bytes);

    auto parsed = st2110::PacketView::from_udp_datagram(st2110::ByteSpan(bytes.data(), bytes.size()));
    assert(parsed.has_value());
    return *parsed;
}

static st2110::Depacketizer make_depacketizer() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 4; // UYVY => active row = 8 bytes
    cfg.height = 1;
    cfg.format = st2110::PixelFormat::UYVY;
    cfg.partial_frame_policy = st2110::PartialFramePolicy::EmitWithFlag;
    cfg.scan_mode = st2110::VideoScanMode::Progressive;
    return st2110::Depacketizer(cfg);
}

static void test_invalid_first_packet_does_not_open_unit() {
    st2110::Depacketizer dep = make_depacketizer();

    // invalid: non-marker packet with trailing padding
    const st2110::PacketView pkt = parse_packet(1, 100, false, {1, 2, 3, 4}, {0, 0});

    bool thrown = false;
    try {
        (void)dep.push(pkt);
    } catch (const std::invalid_argument &) {
        thrown = true;
    }

    assert(thrown);
    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_key().has_value());
    assert(!dep.current_unit_rtp_timestamp().has_value());

    const st2110::DepacketizerStats &stats = dep.stats();
    assert(stats.packets_in == 1);
    assert(stats.packets_used == 0);
    assert(stats.units_ok == 0);
    assert(stats.units_partial == 0);
    assert(stats.units_dropped == 0);
}

static void test_invalid_key_transition_packet_does_not_close_previous_unit() {
    st2110::Depacketizer dep = make_depacketizer();

    // Start first unit with valid non-marker packet
    {
        const st2110::PacketView first = parse_packet(1, 100, false, {1, 2, 3, 4}, {});
        auto out = dep.push(first);
        assert(out.empty());
    }

    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_key().has_value());
    assert(dep.current_unit_key()->rtp_timestamp == 100u);
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 100u);

    // invalid transition: new timestamp + trailing padding on non-marker packet
    {
        const st2110::PacketView bad_transition = parse_packet(2, 200, false, {5, 6, 7, 8}, {0, 0});

        bool thrown = false;
        try {
            (void)dep.push(bad_transition);
        } catch (const std::invalid_argument &) {
            thrown = true;
        }

        assert(thrown);
    }

    // Old unit must still be alive
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_key().has_value());
    assert(dep.current_unit_key()->rtp_timestamp == 100u);
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 100u);

    // Finish the original unit with a valid later fragment of the same row/key
    {
        const st2110::PacketView final_pkt = parse_packet(3, 100, true, {5, 6, 7, 8}, {});
        auto final = final_pkt;
        final.segments[0].header.offset = 2; // advance within the same row

        auto out = dep.push(final);

        assert(out.size() == 1);
        assert(out[0].rtp_timestamp == 100u);
        assert(out[0].complete);
        assert(out[0].marker_seen);
    }

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_key().has_value());
    assert(!dep.current_unit_rtp_timestamp().has_value());

    const st2110::DepacketizerStats &stats = dep.stats();
    assert(stats.packets_in == 3);
    assert(stats.packets_used == 2); // bad transition packet must not be used
    assert(stats.units_ok == 1);
    assert(stats.units_partial == 0);
    assert(stats.units_dropped == 0);
}

int main() {
    test_invalid_first_packet_does_not_open_unit();
    test_invalid_key_transition_packet_does_not_close_previous_unit();
    return 0;
}