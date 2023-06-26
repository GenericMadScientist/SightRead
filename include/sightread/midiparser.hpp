#ifndef SIGHTREAD_MIDIPARSER_HPP
#define SIGHTREAD_MIDIPARSER_HPP

#include <cstdint>
#include <set>
#include <span>

#include "sightread/hopothreshold.hpp"
#include "sightread/metadata.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead {
class MidiParser {
private:
    SightRead::Metadata m_metadata;
    SightRead::HopoThreshold m_hopo_threshold;
    std::set<SightRead::Instrument> m_permitted_instruments;
    bool m_permit_solos;

public:
    explicit MidiParser(SightRead::Metadata metadata);
    MidiParser& hopo_threshold(SightRead::HopoThreshold hopo_threshold);
    MidiParser&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    MidiParser& parse_solos(bool permit_solos);
    SightRead::Song parse(std::span<const std::uint8_t> data) const;
};
}

#endif
