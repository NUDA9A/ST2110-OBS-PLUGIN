#include <cassert>
#include <cstdint>
#include <vector>

#include <st2110/depacketizer.hpp>

static st2110::PacketView make_packet(uint32_t rtp_timestamp, uint32_t ext_seq) {
    st2110::PacketView pkt{};
    pkt.rtp.version = 2;
    pkt.rtp.payload_type = 112;
    pkt.rtp.seq_number = static_cast<uint16_t>(ext_seq & 0xFFFFu);
    pkt.rtp.timestamp = rtp_timestamp;
    pkt.rtp.ssrc = 0x12345678;
    pkt.extended_seq = ext_seq;
    pkt.segment_count = 0;
    pkt.payload_data = st2110::ByteSpan{};
    return pkt;
}

static void test_first_packet_starts_unit_in_progress() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);

    auto out = dep.push(make_packet(1000, 1));

    assert(out.empty());
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 1000u);
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_same_timestamp_keeps_current_unit() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 1280;
    cfg.height = 720;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);

    auto out1 = dep.push(make_packet(2000, 10));
    auto out2 = dep.push(make_packet(2000, 11));

    assert(out1.empty());
    assert(out2.empty());
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 2000u);
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_new_timestamp_closes_previous_and_starts_new_unit() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 640;
    cfg.height = 480;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);

    auto out1 = dep.push(make_packet(3000, 20));
    auto out2 = dep.push(make_packet(3001, 21));

    assert(out1.empty());
    assert(out2.empty());

    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 3001u);
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_multiple_timestamp_transitions_follow_latest_timestamp() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 320;
    cfg.height = 240;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);

    assert(dep.push(make_packet(4000, 1)).empty());
    assert(dep.push(make_packet(4000, 2)).empty());
    assert(dep.push(make_packet(4001, 3)).empty());
    assert(dep.push(make_packet(4002, 4)).empty());

    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 4002u);
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

static void test_reset_clears_timestamp_grouping_state() {
    st2110::DepacketizerConfig cfg{};
    cfg.width = 800;
    cfg.height = 600;
    cfg.format = st2110::PixelFormat::UYVY;

    st2110::Depacketizer dep(cfg);

    assert(dep.push(make_packet(5000, 1)).empty());
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());

    dep.reset();

    assert(!dep.has_unit_in_progress());
    assert(!dep.current_unit_rtp_timestamp().has_value());

    assert(dep.push(make_packet(6000, 2)).empty());
    assert(dep.has_unit_in_progress());
    assert(dep.current_unit_rtp_timestamp().has_value());
    assert(*dep.current_unit_rtp_timestamp() == 6000u);
    assert(dep.assembly_unit_kind() == st2110::VideoAssemblyUnitKind::Frame);
}

int main() {
    test_first_packet_starts_unit_in_progress();
    test_same_timestamp_keeps_current_unit();
    test_new_timestamp_closes_previous_and_starts_new_unit();
    test_multiple_timestamp_transitions_follow_latest_timestamp();
    test_reset_clears_timestamp_grouping_state();
    return 0;
}