#ifndef SIGHTREAD_DETAIL_QBMIDICONVERTER_HPP
#define SIGHTREAD_DETAIL_QBMIDICONVERTER_HPP

#include <string>
#include <string_view>

#include "sightread/detail/qbmidi.hpp"
#include "sightread/metadata.hpp"
#include "sightread/song.hpp"

namespace SightRead::Detail {
class QbMidiConverter {
private:
    std::string m_song_name;
    std::string m_artist;
    std::string m_charter;
    std::uint32_t m_short_name_crc;

public:
    QbMidiConverter(SightRead::Metadata metadata, std::string_view short_name);
    SightRead::Song convert(const SightRead::Detail::QbMidi& midi) const;
};
}

#endif
