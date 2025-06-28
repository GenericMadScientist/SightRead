#include "sightread/qbmidiparser.hpp"
#include "sightread/detail/qbmidiconverter.hpp"

namespace {
SightRead::Detail::Endianness endianness(SightRead::Console console)
{
    if (console == SightRead::Console::PS2) {
        return SightRead::Detail::Endianness::LittleEndian;
    }

    return SightRead::Detail::Endianness::BigEndian;
}
}

SightRead::QbMidiParser::QbMidiParser(std::string_view short_name,
                                      SightRead::Console console)
    : m_console {console}
    , m_short_name {short_name}
{
}

SightRead::Song
SightRead::QbMidiParser::parse(std::span<const std::uint8_t> data) const
{
    const auto qb_midi
        = SightRead::Detail::parse_qb(data, endianness(m_console));
    const auto converter = SightRead::Detail::QbMidiConverter(m_short_name);
    return converter.convert(qb_midi);
}
