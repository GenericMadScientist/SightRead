#include <utility>

#include "sightread/chartparser.hpp"
#include "sightread/detail/chart.hpp"
#include "sightread/detail/chartconverter.hpp"

SightRead::ChartParser::ChartParser(SightRead::Metadata metadata)
    : m_metadata {std::move(metadata)}
    , m_hopo_threshold {SightRead::HopoThresholdType::Resolution,
                        SightRead::Tick {0}}
    , m_permitted_instruments {SightRead::all_instruments()}
    , m_permit_solos {true}
{
}

SightRead::ChartParser&
SightRead::ChartParser::hopo_threshold(SightRead::HopoThreshold hopo_threshold)
{
    m_hopo_threshold = hopo_threshold;
    return *this;
}

SightRead::ChartParser& SightRead::ChartParser::permit_instruments(
    std::set<SightRead::Instrument> permitted_instruments)
{
    m_permitted_instruments = std::move(permitted_instruments);
    return *this;
}

SightRead::ChartParser& SightRead::ChartParser::parse_solos(bool permit_solos)
{
    m_permit_solos = permit_solos;
    return *this;
}

SightRead::Song SightRead::ChartParser::parse(std::string_view data) const
{
    const auto chart = SightRead::Detail::parse_chart(data);

    const auto converter = SightRead::Detail::ChartConverter(m_metadata)
                               .hopo_threshold(m_hopo_threshold)
                               .permit_instruments(m_permitted_instruments)
                               .parse_solos(m_permit_solos);
    return converter.convert(chart);
}
