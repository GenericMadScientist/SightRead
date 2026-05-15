#ifndef SIGHTREAD_DETAIL_CHARTCONVERTER_HPP
#define SIGHTREAD_DETAIL_CHARTCONVERTER_HPP

#include <set>
#include <string>

#include "sightread/detail/chart.hpp"
#include "sightread/metadata.hpp"
#include "sightread/soloparsingbehaviour.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead::Detail {
class ChartConverter {
private:
    std::string m_song_name;
    std::string m_artist;
    std::string m_charter;
    SightRead::HopoThreshold m_hopo_threshold;
    std::set<SightRead::Instrument> m_permitted_instruments;
    SightRead::SoloParsingBehaviour m_solo_parsing_behaviour;
    bool m_allow_open_chords;

public:
    explicit ChartConverter(SightRead::Metadata metadata);
    ChartConverter&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    ChartConverter&
    solo_parsing_behaviour(SightRead::SoloParsingBehaviour behaviour);
    ChartConverter& allow_open_chords(bool allow_open_chords);
    [[nodiscard]] SightRead::Song
    convert(const SightRead::Detail::Chart& chart) const;
};
}

#endif
