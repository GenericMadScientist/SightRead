#ifndef SIGHTREAD_QBMIDIPARSER_HPP
#define SIGHTREAD_QBMIDIPARSER_HPP

#include <cstdint>
#include <span>
#include <string_view>

#include "sightread/metadata.hpp"
#include "sightread/song.hpp"

namespace SightRead {
enum class Console : std::uint8_t { PC, PS2, PS3, Wii, Xbox360 };

class QbMidiParser {
private:
    SightRead::Metadata m_metadata;
    std::string_view m_short_name;
    Console m_console;

public:
    QbMidiParser(SightRead::Metadata metadata, std::string_view short_name,
                 Console console);
    [[nodiscard]] SightRead::Song
    parse(std::span<const std::uint8_t> data) const;
};
}

#endif
