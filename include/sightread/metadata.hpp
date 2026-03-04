#ifndef SIGHTREAD_METADATA_HPP
#define SIGHTREAD_METADATA_HPP

#include <string>
#include <string_view>

namespace SightRead {
struct Metadata {
    std::string name;
    std::string artist;
    std::string charter;
};

Metadata parse_ini(std::string_view data);
}

#endif
