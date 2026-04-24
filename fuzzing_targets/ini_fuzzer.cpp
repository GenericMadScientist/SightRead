#include <cstddef>
#include <string_view>

#include "sightread/metadata.hpp"
#include "sightread/tempomap.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size)
{
    const std::string_view ini {data, size};

    try {
        SightRead::parse_ini(ini);
        return 0;
    } catch (const SightRead::ParseError&) {
        return 0;
    }
}
