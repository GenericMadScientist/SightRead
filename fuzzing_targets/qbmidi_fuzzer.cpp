#include <cstddef>
#include <span>

#include "sightread/metadata.hpp"
#include "sightread/qbmidiparser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size)
{
    // NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)
    const std::span<const std::uint8_t> qb_midi {
        reinterpret_cast<const std::uint8_t*>(data), size};
    // NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast))
    const SightRead::Metadata metadata;
    const SightRead::QbMidiParser parser {metadata, "name",
                                          SightRead::Console::PC};
    try {
        (void)parser.parse(qb_midi);
        return 0;
    } catch (const SightRead::ParseError&) {
        return 0;
    }
}
