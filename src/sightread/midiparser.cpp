#include <utility>

#include "sightread/detail/midiconverter.hpp"
#include "sightread/midiparser.hpp"

SightRead::MidiParser::MidiParser(SightRead::Metadata metadata)
    : m_metadata {std::move(metadata)}
    , m_hopo_threshold {SightRead::HopoThresholdType::Resolution,
                        SightRead::Tick {0}}
    , m_permitted_instruments {SightRead::all_instruments()}
    , m_permit_solos {true}
{
}

SightRead::MidiParser&
SightRead::MidiParser::hopo_threshold(SightRead::HopoThreshold hopo_threshold)
{
    m_hopo_threshold = hopo_threshold;
    return *this;
}

SightRead::MidiParser& SightRead::MidiParser::permit_instruments(
    std::set<SightRead::Instrument> permitted_instruments)
{
    m_permitted_instruments = std::move(permitted_instruments);
    return *this;
}

SightRead::MidiParser& SightRead::MidiParser::parse_solos(bool permit_solos)
{
    m_permit_solos = permit_solos;
    return *this;
}

SightRead::Song
SightRead::MidiParser::parse(std::span<const std::uint8_t> data) const
{
    const auto midi = SightRead::Detail::parse_midi(data);

    const auto converter = SightRead::Detail::MidiConverter(m_metadata)
                               .hopo_threshold(m_hopo_threshold)
                               .permit_instruments(m_permitted_instruments)
                               .parse_solos(m_permit_solos);
    return converter.convert(midi);
}
