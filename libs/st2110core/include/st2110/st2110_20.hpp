#ifndef ST2110_OBS_PLUGIN_ST2110_20_HPP
#define ST2110_OBS_PLUGIN_ST2110_20_HPP

#include <cstdint>

namespace st2110 {
    struct ExtendedSequenceNumber {
        uint16_t hi16;
    };

    struct SrdHeader {
        uint16_t length;
        uint16_t row_number;
        uint16_t offset;
        bool field_id;
        bool continuation;
    };

    struct St2110PayloadHeaderView {
        ExtendedSequenceNumber ext_seq;
        SrdHeader srd[3];
        uint8_t srd_count;
    };

}
#endif //ST2110_OBS_PLUGIN_ST2110_20_HPP
