#ifndef SIGHTREAD_MIDIPARSER_HPP
#define SIGHTREAD_MIDIPARSER_HPP

#include <cstdint>
#include <set>
#include <span>

#include "sightread/metadata.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead {
class MidiParser {
private:
    SightRead::Metadata m_metadata;
    std::set<SightRead::Instrument> m_permitted_instruments;
    bool m_permit_solos;

public:
    explicit MidiParser(SightRead::Metadata metadata);
    MidiParser&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    MidiParser& parse_solos(bool permit_solos);
    [[nodiscard]] SightRead::Song
    parse(std::span<const std::uint8_t> data) const;
};
}

#endif
