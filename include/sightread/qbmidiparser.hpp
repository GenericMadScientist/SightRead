#ifndef SIGHTREAD_QBMIDIPARSER_HPP
#define SIGHTREAD_QBMIDIPARSER_HPP

#include <cstdint>
#include <span>
#include <string_view>

#include "sightread/song.hpp"

namespace SightRead {
enum class Console { PC, PS2, PS3, Wii, Xbox360 };

class QbMidiParser {
private:
    Console m_console;
    std::string_view m_short_name;

public:
    QbMidiParser(std::string_view short_name, Console console);
    SightRead::Song parse(std::span<const std::uint8_t> data) const;
};
}

#endif
