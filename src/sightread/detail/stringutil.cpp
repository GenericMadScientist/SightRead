#include <charconv>

#include <boost/locale.hpp>

#include "sightread/detail/stringutil.hpp"
#include "sightread/tempomap.hpp"

namespace {
std::string utf16_to_utf8_string(std::string_view input)
{
    if (input.size() % 2 != 0) {
        throw SightRead::ParseError("UTF-16 strings must have even length");
    }

    // I'm pretty sure I really do need the reinterpret_cast here.
    std::u16string_view utf16_string_view {
        reinterpret_cast<const char16_t*>(input.data()), // NOLINT
        input.size() / 2};

    return boost::locale::conv::utf_to_utf<char>(
        utf16_string_view.data(),
        utf16_string_view.data() + utf16_string_view.size());
}
}

namespace SightRead::Detail {
std::string_view skip_whitespace(std::string_view input)
{
    const auto first_non_ws_location = input.find_first_not_of(" \f\n\r\t\v");
    input.remove_prefix(std::min(first_non_ws_location, input.size()));
    return input;
}

std::string_view break_off_newline(std::string_view& input)
{
    if (input.empty()) {
        throw SightRead::ParseError("No lines left");
    }

    const auto newline_location
        = std::min(input.find('\n'), input.find("\r\n"));
    if (newline_location == std::string_view::npos) {
        const auto line = input;
        input.remove_prefix(input.size());
        return line;
    }

    const auto line = input.substr(0, newline_location);
    input.remove_prefix(newline_location);
    input = skip_whitespace(input);
    return line;
}

std::string to_utf8_string(std::string_view input)
{
    if (input.starts_with("\xEF\xBB\xBF")) {
        // Trim off UTF-8 BOM.
        input.remove_prefix(3);
        return std::string(input);
    }

    if (input.starts_with("\xFF\xFE")) {
        // Trim off UTF-16le BOM.
        input.remove_prefix(2);
        return utf16_to_utf8_string(input);
    }

    auto utf8_copy = boost::locale::conv::utf_to_utf<char>(
        input.data(), input.data() + input.size());
    if (std::ranges::equal(utf8_copy, input)) {
        return utf8_copy;
    }
    return boost::locale::conv::to_utf<char>(
        input.data(), input.data() + input.size(), "Latin1");
}

// Convert a string_view to an int. If there are any problems with the input,
// this function returns std::nullopt.
std::optional<int> string_view_to_int(std::string_view input)
{
    int result = 0;
    const char* last = input.data() + input.size();
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    const auto [p, ec] = std::from_chars(input.data(), last, result);
    if ((ec != std::errc()) || (p != last)) {
        return std::nullopt;
    }
    return result;
}
}
