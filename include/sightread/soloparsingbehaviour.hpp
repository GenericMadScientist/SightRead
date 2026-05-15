#ifndef SIGHTREAD_SOLOPARSINGBEHAVIOUR_HPP
#define SIGHTREAD_SOLOPARSINGBEHAVIOUR_HPP

#include <cstdint>

namespace SightRead {
enum class SoloParsingBehaviour : std::uint8_t {
    PreferLaterStarts, // Default behaviour, Clone Hero v1.1
    PreferEarlierStarts, // YARG's behaviour
    NoSolos
};
}

#endif
