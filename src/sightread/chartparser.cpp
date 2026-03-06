#include <utility>

#include "sightread/chartparser.hpp"
#include "sightread/detail/chart.hpp"
#include "sightread/detail/chartconverter.hpp"

namespace {
std::string_view remove_utf8_bom(std::string_view data)
{
    if (data.starts_with("\xEF\xBB\xBF")) {
        data.remove_prefix(3);
    }
    return data;
}
}

SightRead::ChartParser::ChartParser(SightRead::Metadata metadata)
    : m_metadata {std::move(metadata)}
    , m_permitted_instruments {SightRead::all_instruments()}
    , m_permit_solos {true}
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

SightRead::Song SightRead::ChartParser::parse(std::string_view data) const
{
    data = remove_utf8_bom(data);

    const auto chart = SightRead::Detail::parse_chart(data);

    const auto converter = SightRead::Detail::ChartConverter(m_metadata)
                               .permit_instruments(m_permitted_instruments)
                               .parse_solos(m_permit_solos);
    return converter.convert(chart);
}
