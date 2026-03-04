#include "sightread/metadata.hpp"
#include "sightread/detail/stringutil.hpp"

namespace SightRead {
using namespace SightRead::Detail;

SightRead::Metadata parse_ini(std::string_view data)
{
    constexpr auto ARTIST_SIZE = 6;
    constexpr auto CHARTER_SIZE = 7;
    constexpr auto FRETS_SIZE = 5;
    constexpr auto NAME_SIZE = 4;

    std::string u8_string = to_utf8_string(data);
    data = u8_string;

    SightRead::Metadata metadata;
    metadata.name = "Unknown Song";
    metadata.artist = "Unknown Artist";
    metadata.charter = "Unknown Charter";
    while (!data.empty()) {
        const auto line = break_off_newline(data);
        if (line.starts_with("name")) {
            auto value = skip_whitespace(line.substr(NAME_SIZE));
            if (value[0] != '=') {
                continue;
            }
            value = skip_whitespace(value.substr(1));
            metadata.name = value;
        } else if (line.starts_with("artist")) {
            auto value = skip_whitespace(line.substr(ARTIST_SIZE));
            if (value[0] != '=') {
                continue;
            }
            value = skip_whitespace(value.substr(1));
            metadata.artist = value;
        } else if (line.starts_with("charter")) {
            auto value = skip_whitespace(line.substr(CHARTER_SIZE));
            if (value[0] != '=') {
                continue;
            }
            value = skip_whitespace(value.substr(1));
            if (!value.empty()) {
                metadata.charter = value;
            }
        } else if (line.starts_with("frets")) {
            auto value = skip_whitespace(line.substr(FRETS_SIZE));
            if (value[0] != '=') {
                continue;
            }
            value = skip_whitespace(value.substr(1));
            if (!value.empty()) {
                metadata.charter = value;
            }
        }
    }

    return metadata;
}
}
