#ifndef SIGHTREAD_CHARTPARSER_HPP
#define SIGHTREAD_CHARTPARSER_HPP

#include <set>
#include <string_view>

#include "sightread/metadata.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead {
class ChartParser {
private:
    SightRead::Metadata m_metadata;
    std::set<SightRead::Instrument> m_permitted_instruments;
    bool m_permit_solos;

public:
    explicit ChartParser(SightRead::Metadata metadata);
    ChartParser&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    ChartParser& parse_solos(bool permit_solos);
    [[nodiscard]] SightRead::Song parse(std::string_view data) const;
};
}

#endif
