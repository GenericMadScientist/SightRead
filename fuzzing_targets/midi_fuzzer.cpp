#include <cstddef>
#include <span>

#include "sightread/metadata.hpp"
#include "sightread/midiparser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size)
{
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::span<const std::uint8_t> midi {
        reinterpret_cast<const std::uint8_t*>(data), size};
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast))
    const SightRead::Metadata metadata;
    const SightRead::MidiParser parser {metadata};
    try {
        (void)parser.parse(midi);
        return 0;
    } catch (const SightRead::ParseError&) {
        return 0;
    }
}
