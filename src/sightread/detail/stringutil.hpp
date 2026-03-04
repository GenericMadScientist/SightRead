#ifndef SIGHTREAD_DETAIL_STRINGUTIL_HPP
#define SIGHTREAD_DETAIL_STRINGUTIL_HPP

#include <string>
#include <string_view>

namespace SightRead::Detail {
// This returns a string_view from the start of input until a carriage return
// or newline. input is changed to point to the first character past the
// detected newline character that is not a whitespace character.
std::string_view break_off_newline(std::string_view& input);

std::string_view skip_whitespace(std::string_view input);

// Convert a UTF-8 or UTF-16le string to a UTF-8 string.
std::string to_utf8_string(std::string_view input);
}

#endif
