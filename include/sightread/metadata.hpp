#ifndef SIGHTREAD_METADATA_HPP
#define SIGHTREAD_METADATA_HPP

#include <optional>
#include <string>
#include <string_view>

#include "sightread/time.hpp"

namespace SightRead {
enum class HopoThresholdType : std::uint8_t {
    Resolution,
    HopoFrequency,
    EighthNote
};

struct HopoThreshold {
    HopoThresholdType threshold_type = HopoThresholdType::Resolution;
    SightRead::Tick hopo_frequency {0};

    [[nodiscard]] Tick chart_max_hopo_gap(int resolution) const;
    [[nodiscard]] Tick midi_max_hopo_gap(int resolution) const;
};

struct Metadata {
    std::string name;
    std::string artist;
    std::string charter;
    HopoThreshold hopo_threshold;
    std::optional<int> sustain_cutoff_threshold;
};

Metadata parse_ini(std::string_view data);
}

#endif
