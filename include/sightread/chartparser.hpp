#ifndef SIGHTREAD_CHARTPARSER_HPP
#define SIGHTREAD_CHARTPARSER_HPP

#include <set>
#include <string_view>

#include "sightread/metadata.hpp"
#include "sightread/soloparsingbehaviour.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead {
class ChartParser {
private:
    SightRead::Metadata m_metadata;
    std::set<SightRead::Instrument> m_permitted_instruments;
    SightRead::SoloParsingBehaviour m_solo_parsing_behaviour;
    bool m_allow_open_chords;

public:
    explicit ChartParser(SightRead::Metadata metadata);
    ChartParser&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    ChartParser&
    solo_parsing_behaviour(SightRead::SoloParsingBehaviour behaviour);
    ChartParser& allow_open_chords(bool allow_open_chords);
    [[nodiscard]] SightRead::Song parse(std::string_view data) const;
};
}

#endif
