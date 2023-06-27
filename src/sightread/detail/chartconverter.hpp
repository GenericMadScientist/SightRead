#ifndef SIGHTREAD_DETAIL_CHARTCONVERTER_HPP
#define SIGHTREAD_DETAIL_CHARTCONVERTER_HPP

#include <set>
#include <string>

#include "sightread/detail/chart.hpp"
#include "sightread/hopothreshold.hpp"
#include "sightread/metadata.hpp"
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
    bool m_permit_solos;

public:
    explicit ChartConverter(SightRead::Metadata metadata);
    ChartConverter& hopo_threshold(SightRead::HopoThreshold hopo_threshold);
    ChartConverter&
    permit_instruments(std::set<SightRead::Instrument> permitted_instruments);
    ChartConverter& parse_solos(bool permit_solos);
    SightRead::Song convert(const SightRead::Detail::Chart& chart) const;
};
}

#endif
