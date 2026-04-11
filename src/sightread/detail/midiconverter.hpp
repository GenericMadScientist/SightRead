#ifndef SIGHTREAD_DETAIL_MIDICONVERTER_HPP
#define SIGHTREAD_DETAIL_MIDICONVERTER_HPP

#include <optional>
#include <set>
#include <string>

#include "sightread/detail/midi.hpp"
#include "sightread/metadata.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead::Detail {
class MidiConverter {
private:
    std::string m_song_name;
    std::string m_artist;
    std::string m_charter;
    SightRead::HopoThreshold m_hopo_threshold;
    std::optional<int> m_sustain_cutoff_threshold;
    std::set<SightRead::Instrument> m_permitted_instruments;
    bool m_permit_solos;
    bool m_allow_open_chords;
    bool m_use_sustain_cutoff_threshold;

    [[nodiscard]] std::optional<SightRead::Instrument>
    midi_section_instrument(const std::string& track_name) const;
    void process_instrument_track(
        const std::string& track_name,
        const SightRead::Detail::MidiTrack& track, SightRead::Song& song,
        std::optional<SightRead::Tick> coda_event_time) const;
    [[nodiscard]] std::optional<int> sustain_cutoff_threshold() const;

public:
    explicit MidiConverter(SightRead::Metadata metadata);
    MidiConverter&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    MidiConverter& parse_solos(bool permit_solos);
    MidiConverter& allow_open_chords(bool allow_open_chords);
    MidiConverter&
    use_sustain_cutoff_threshold(bool use_sustain_cutoff_threshold);
    [[nodiscard]] SightRead::Song
    convert(const SightRead::Detail::Midi& midi) const;
};
}

#endif
