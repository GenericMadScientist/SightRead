#ifndef SIGHTREAD_DETAIL_MIDICONVERTER_HPP
#define SIGHTREAD_DETAIL_MIDICONVERTER_HPP

#include <set>
#include <string>

#include "sightread/detail/midi.hpp"
#include "sightread/hopothreshold.hpp"
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
    std::set<SightRead::Instrument> m_permitted_instruments;
    bool m_permit_solos;

    void process_instrument_track(const std::string& track_name,
                                  const SightRead::Detail::MidiTrack& track,
                                  SightRead::Song& song) const;

public:
    explicit MidiConverter(SightRead::Metadata metadata);
    MidiConverter& hopo_threshold(SightRead::HopoThreshold hopo_threshold);
    MidiConverter&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    MidiConverter& parse_solos(bool permit_solos);
    SightRead::Song convert(const SightRead::Detail::Midi& midi) const;
};
}

#endif
