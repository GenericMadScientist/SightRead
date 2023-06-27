#ifndef SIGHTREAD_CHARTPARSER_HPP
#define SIGHTREAD_CHARTPARSER_HPP

#include <set>
#include <string_view>

#include "sightread/hopothreshold.hpp"
#include "sightread/metadata.hpp"
#include "sightread/song.hpp"
#include "sightread/songparts.hpp"

namespace SightRead {
class ChartParser {
private:
    SightRead::Metadata m_metadata;
    SightRead::HopoThreshold m_hopo_threshold;
    std::set<SightRead::Instrument> m_permitted_instruments;
    bool m_permit_solos;

public:
    explicit ChartParser(SightRead::Metadata metadata);
    ChartParser& hopo_threshold(SightRead::HopoThreshold hopo_threshold);
    ChartParser&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    ChartParser& parse_solos(bool permit_solos);
    SightRead::Song parse(std::string_view data) const;
};
}

#endif
