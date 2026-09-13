#ifndef SIGHTREAD_DETAIL_DRUMTRACKTYPE_HPP
#define SIGHTREAD_DETAIL_DRUMTRACKTYPE_HPP

#include <cstdint>

namespace SightRead::Detail {
enum class DrumTrackType : std::uint8_t { FourLane, FourLanePro, FiveLane };
}

#endif
