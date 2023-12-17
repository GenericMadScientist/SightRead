#ifndef SIGHTREAD_HOPOTHRESHOLD_HPP
#define SIGHTREAD_HOPOTHRESHOLD_HPP

#include <stdexcept>

#include "sightread/time.hpp"

namespace SightRead {
enum class HopoThresholdType { Resolution, HopoFrequency, EighthNote };

struct HopoThreshold {
    HopoThresholdType threshold_type;
    SightRead::Tick hopo_frequency;

    SightRead::Tick chart_max_hopo_gap(int resolution) const
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

    SightRead::Tick midi_max_hopo_gap(int resolution) const
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
};
}

#endif
