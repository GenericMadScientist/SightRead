#include <stdexcept>

#include "sightread/detail/stringutil.hpp"
#include "sightread/metadata.hpp"

namespace SightRead {
using namespace SightRead::Detail;

SightRead::Tick
SightRead::HopoThreshold::chart_max_hopo_gap(int resolution) const
{
    constexpr int DEFAULT_HOPO_GAP = 65;
    constexpr int DEFAULT_RESOLUTION = 192;

    switch (threshold_type) {
    case HopoThresholdType::HopoFrequency:
        return hopo_frequency;
    case HopoThresholdType::EighthNote:
        return SightRead::Tick {(resolution + 3) / 2};
    case HopoThresholdType::Resolution:
        return SightRead::Tick {(DEFAULT_HOPO_GAP * resolution)
                                / DEFAULT_RESOLUTION};
    }

    throw std::runtime_error("Invalid threshold type");
}

SightRead::Tick
SightRead::HopoThreshold::midi_max_hopo_gap(int resolution) const
{
    switch (threshold_type) {
    case HopoThresholdType::HopoFrequency:
        return hopo_frequency;
    case HopoThresholdType::EighthNote:
        return SightRead::Tick {(resolution + 3) / 2};
    case HopoThresholdType::Resolution:
        return SightRead::Tick {resolution / 3 + 1};
    }

    throw std::runtime_error("Invalid threshold type");
}

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
