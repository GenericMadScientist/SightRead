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
    bool m_allow_open_chords;
    bool m_use_sustain_cutoff_threshold;
    bool m_normalise_disjoint_sustains;
    bool m_consecutive_note_ons_cancel;

public:
    explicit MidiParser(SightRead::Metadata metadata);
    MidiParser&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    MidiParser& parse_solos(bool permit_solos);
    MidiParser& allow_open_chords(bool allow_open_chords);
    MidiParser& use_sustain_cutoff_threshold(bool use_sustain_cutoff_threshold);
    MidiParser& normalise_disjoint_sustains(bool normalise_disjoint_sustains);
    MidiParser& consecutive_note_ons_cancel(bool consecutive_note_ons_cancel);
    [[nodiscard]] SightRead::Song
    parse(std::span<const std::uint8_t> data) const;
};
}

#endif
