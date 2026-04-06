#include <utility>

#include "sightread/chartparser.hpp"
#include "sightread/detail/chart.hpp"
#include "sightread/detail/chartconverter.hpp"
#include "sightread/detail/stringutil.hpp"

SightRead::ChartParser::ChartParser(SightRead::Metadata metadata)
    : m_metadata {std::move(metadata)}
    , m_permitted_instruments {SightRead::all_instruments()}
    , m_permit_solos {true}
    , m_allow_open_chords {false}
{
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

SightRead::ChartParser&
SightRead::ChartParser::allow_open_chords(bool allow_open_chords)
{
    m_allow_open_chords = allow_open_chords;
    return *this;
}

SightRead::Song SightRead::ChartParser::parse(std::string_view data) const
{
    const auto utf8_string = SightRead::Detail::to_utf8_string(data);
    const auto chart = SightRead::Detail::parse_chart(utf8_string);
    const auto converter = SightRead::Detail::ChartConverter(m_metadata)
                               .permit_instruments(m_permitted_instruments)
                               .parse_solos(m_permit_solos)
                               .allow_open_chords(m_allow_open_chords);
    return converter.convert(chart);
}
