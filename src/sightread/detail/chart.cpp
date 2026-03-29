#include "sightread/detail/chart.hpp"
#include "sightread/detail/stringutil.hpp"
#include "sightread/songparts.hpp"

namespace {
std::string_view strip_square_brackets(std::string_view input)
{
    if (input.empty()) {
        throw SightRead::ParseError("Header string empty");
    }
    return input.substr(1, input.size() - 2);
}

// Split input by space characters, similar to .Split(' ') in C#. Note that
// the lifetime of the string_views in the output is the same as that of the
// input.
std::vector<std::string_view> split_by_space(std::string_view input)
{
    std::vector<std::string_view> substrings;

    while (true) {
        const auto space_location = input.find(' ');
        if (space_location == std::string_view::npos) {
            break;
        }
        substrings.push_back(input.substr(0, space_location));
        input.remove_prefix(space_location + 1);
    }

    substrings.push_back(input);
    return substrings;
}

SightRead::Detail::NoteEvent
convert_line_to_note(int position,
                     const std::vector<std::string_view>& split_line)
{
    constexpr int MAX_NORMAL_EVENT_SIZE = 5;

    if (split_line.size() < MAX_NORMAL_EVENT_SIZE) {
        throw SightRead::ParseError("Line incomplete");
    }
    const auto fret = SightRead::Detail::string_view_to_int(split_line.at(3));
    const auto length = SightRead::Detail::string_view_to_int(split_line.at(4));
    if (!fret.has_value() || !length.has_value()) {
        throw SightRead::ParseError("Bad note event");
    }
    return {.position = position, .fret = *fret, .length = *length};
}

SightRead::Detail::SpecialEvent
convert_line_to_special(int position,
                        const std::vector<std::string_view>& split_line)
{
    constexpr int MAX_NORMAL_EVENT_SIZE = 5;

    if (split_line.size() < MAX_NORMAL_EVENT_SIZE) {
        throw SightRead::ParseError("Line incomplete");
    }
    const auto sp_key = SightRead::Detail::string_view_to_int(split_line.at(3));
    const auto length = SightRead::Detail::string_view_to_int(split_line.at(4));
    if (!sp_key.has_value() || !length.has_value()) {
        throw SightRead::ParseError("Bad SP event");
    }
    return {.position = position, .key = *sp_key, .length = *length};
}

SightRead::Detail::BpmEvent
convert_line_to_bpm(int position,
                    const std::vector<std::string_view>& split_line)
{
    if (split_line.size() < 4) {
        throw SightRead::ParseError("Line incomplete");
    }
    const auto bpm = SightRead::Detail::string_view_to_int(split_line.at(3));
    if (!bpm.has_value()) {
        throw SightRead::ParseError("Bad BPM event");
    }
    return {.position = position, .bpm = *bpm};
}

SightRead::Detail::TimeSigEvent
convert_line_to_timesig(int position,
                        const std::vector<std::string_view>& split_line)
{
    constexpr int MAX_NORMAL_EVENT_SIZE = 5;

    if (split_line.size() < 4) {
        throw SightRead::ParseError("Line incomplete");
    }
    const auto numer = SightRead::Detail::string_view_to_int(split_line.at(3));
    std::optional<int> denom = 2;
    if (split_line.size() >= MAX_NORMAL_EVENT_SIZE) {
        denom = SightRead::Detail::string_view_to_int(split_line.at(4));
    }
    if (!numer.has_value() || !denom.has_value()) {
        throw SightRead::ParseError("Bad TS event");
    }
    return {.position = position, .numerator = *numer, .denominator = *denom};
}

SightRead::Detail::Event
convert_line_to_event(int position,
                      const std::vector<std::string_view>& split_line)
{
    if (split_line.size() < 4) {
        throw SightRead::ParseError("Line incomplete");
    }
    SightRead::Detail::Event event;
    event.position = position;
    for (auto it = std::next(split_line.cbegin(), 3); it < split_line.cend();
         ++it) {
        event.data += *it;
        if (std::next(it) != split_line.cend()) {
            event.data += ' ';
        }
    }
    return event;
}

SightRead::Detail::ChartSection read_section(std::string_view& input)
{
    SightRead::Detail::ChartSection section;
    section.name
        = strip_square_brackets(SightRead::Detail::break_off_newline(input));

    if (SightRead::Detail::break_off_newline(input) != "{") {
        throw SightRead::ParseError("Section does not open with {");
    }

    while (true) {
        const auto next_line = SightRead::Detail::break_off_newline(input);
        if (next_line == "}") {
            break;
        }
        const auto separated_line = split_by_space(next_line);
        if (separated_line.size() < 3) {
            throw SightRead::ParseError("Line incomplete");
        }
        const auto key = separated_line.at(0);
        const auto key_val = SightRead::Detail::string_view_to_int(key);
        if (key_val.has_value()) {
            const auto pos = *key_val;
            const auto event_type = separated_line.at(2);
            if (event_type == "N") {
                section.note_events.push_back(
                    convert_line_to_note(pos, separated_line));
            } else if (event_type == "S") {
                section.special_events.push_back(
                    convert_line_to_special(pos, separated_line));
            } else if (event_type == "B") {
                section.bpm_events.push_back(
                    convert_line_to_bpm(pos, separated_line));
            } else if (event_type == "TS") {
                section.ts_events.push_back(
                    convert_line_to_timesig(pos, separated_line));
            } else if (event_type == "E") {
                section.events.push_back(
                    convert_line_to_event(pos, separated_line));
            }
        } else {
            std::string value;
            for (auto it = std::next(separated_line.cbegin(), 2);
                 it < separated_line.cend(); ++it) {
                value.append(*it);
            }
            section.key_value_pairs[std::string(key)] = value;
        }
    }

    return section;
}
}

SightRead::Detail::Chart SightRead::Detail::parse_chart(std::string_view data)
{
    SightRead::Detail::Chart chart;

    while (!data.empty()) {
        chart.sections.push_back(read_section(data));
    }

    return chart;
}
