#include <array>
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

void assign_value(SightRead::Metadata& metadata, std::string_view key,
                  std::string_view value)
{
    if (key == "name") {
        metadata.name = value;
    } else if (key == "artist") {
        metadata.artist = value;
    } else if (key == "charter" || key == "frets") {
        if (!value.empty()) {
            metadata.charter = value;
        }
    } else if (key == "hopo_frequency") {
        const auto int_value = string_view_to_int(value);
        if (int_value.has_value()) {
            metadata.hopo_threshold.threshold_type
                = SightRead::HopoThresholdType::HopoFrequency;
            metadata.hopo_threshold.hopo_frequency
                = SightRead::Tick {*int_value};
        }
    } else if (key == "eighthnote_hopo") {
        const auto int_value = string_view_to_int(value);
        if (!int_value.has_value()) {
            return;
        }
        if (*int_value == 0) {
            metadata.hopo_threshold.threshold_type
                = SightRead::HopoThresholdType::Resolution;
        } else {
            metadata.hopo_threshold.threshold_type
                = SightRead::HopoThresholdType::EighthNote;
        }
    }
}

SightRead::Metadata parse_ini(std::string_view data)
{
    constexpr std::array<std::string_view, 6> INI_KEYS {
        "artist", "charter",        "eighthnote_hopo",
        "frets",  "hopo_frequency", "name"};

    std::string u8_string = to_utf8_string(data);
    data = u8_string;

    SightRead::Metadata metadata;
    metadata.name = "Unknown Song";
    metadata.artist = "Unknown Artist";
    metadata.charter = "Unknown Charter";
    while (!data.empty()) {
        const auto line = break_off_newline(data);
        for (const auto key : INI_KEYS) {
            if (!line.starts_with(key)) {
                continue;
            }
            auto value = skip_whitespace(line.substr(key.size()));
            if (value.at(0) != '=') {
                continue;
            }
            value = skip_whitespace(value.substr(1));
            assign_value(metadata, key, value);
        }
    }

    return metadata;
}
}
