#include <cstddef>
#include <string_view>

#include "sightread/chartparser.hpp"
#include "sightread/metadata.hpp"

extern "C" int LLVMFuzzerTestOneInput(const char* data, size_t size)
{
    const std::string_view chart {data, size};
    const SightRead::Metadata metadata;
    const SightRead::ChartParser parser {metadata};
    try {
        (void)parser.parse(chart);
        return 0;
    } catch (const SightRead::ParseError&) {
        return 0;
    }
}
