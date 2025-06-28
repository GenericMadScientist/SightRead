#ifndef SIGHTREAD_DETAIL_QBMIDICONVERTER_HPP
#define SIGHTREAD_DETAIL_QBMIDICONVERTER_HPP

#include <string_view>

#include "sightread/detail/qbmidi.hpp"
#include "sightread/metadata.hpp"
#include "sightread/song.hpp"

namespace SightRead::Detail {
class QbMidiConverter {
private:
    std::uint32_t m_short_name_crc;

public:
    explicit QbMidiConverter(std::string_view short_name);
    SightRead::Song convert(const SightRead::Detail::QbMidi& midi) const;
};
}

#endif
