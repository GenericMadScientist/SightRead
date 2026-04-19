#include <algorithm>
#include <climits>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include "sightread/detail/intervalset.hpp"
#include "sightread/detail/midiconverter.hpp"
#include "sightread/detail/parserutil.hpp"

namespace {
SightRead::TempoMap
read_first_midi_track(const SightRead::Detail::MidiTrack& track, int resolution)
{
    constexpr int SET_TEMPO_ID = 0x51;
    constexpr int TIME_SIG_ID = 0x58;

    std::vector<SightRead::BPM> tempos;
    std::vector<SightRead::TimeSignature> time_sigs;
    for (const auto& event : track.events) {
        const auto* meta_event
            = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
        if (meta_event == nullptr) {
            continue;
        }
        switch (meta_event->type) {
        case SET_TEMPO_ID: {
            if (meta_event->data.size() < 3) {
                throw SightRead::ParseError("Tempo meta event too short");
            }
            const auto us_per_quarter = meta_event->data.at(0) << 16
                | meta_event->data.at(1) << 8 | meta_event->data.at(2);
            const auto millibeats_per_minute = 60000000000.0 / us_per_quarter;
            tempos.push_back({.position = SightRead::Tick {event.time},
                              .millibeats_per_minute = millibeats_per_minute});
            break;
        }
        case TIME_SIG_ID:
            if (meta_event->data.size() < 2) {
                throw SightRead::ParseError("Tempo meta event too short");
            }
            if (meta_event->data.at(1) >= (CHAR_BIT * sizeof(int))) {
                throw SightRead::ParseError("Time sig denominator too large");
            }
            time_sigs.push_back({.position = SightRead::Tick {event.time},
                                 .numerator = meta_event->data.at(0),
                                 .denominator = 1 << meta_event->data.at(1)});
            break;
        default:
            break;
        }
    }

    return {std::move(time_sigs), std::move(tempos), {}, resolution};
}

std::optional<std::string>
midi_track_name(const SightRead::Detail::MidiTrack& track)
{
    if (track.events.empty()) {
        return std::nullopt;
    }
    for (const auto& event : track.events) {
        const auto* meta_event
            = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
        if (meta_event == nullptr) {
            continue;
        }
        if (meta_event->type != 3) {
            continue;
        }
        return std::string {meta_event->data.cbegin(), meta_event->data.cend()};
    }
    return std::nullopt;
}

std::vector<SightRead::Tick>
od_beats_from_track(const SightRead::Detail::MidiTrack& track)
{
    constexpr int NOTE_ON_ID = 0x90;
    constexpr int UPPER_NIBBLE_MASK = 0xF0;
    constexpr int BEAT_LOW_KEY = 12;
    constexpr int BEAT_HIGH_KEY = 13;

    std::vector<SightRead::Tick> od_beats;

    for (const auto& event : track.events) {
        const auto* midi_event
            = std::get_if<SightRead::Detail::MidiEvent>(&event.event);
        if (midi_event == nullptr) {
            continue;
        }
        if ((midi_event->status & UPPER_NIBBLE_MASK) != NOTE_ON_ID) {
            continue;
        }
        if (midi_event->data.at(1) == 0) {
            continue;
        }
        const auto key = midi_event->data.at(0);
        if (key == BEAT_LOW_KEY || key == BEAT_HIGH_KEY) {
            od_beats.emplace_back(event.time);
        }
    }

    return od_beats;
}

std::vector<SightRead::PracticeSection>
practice_sections_from_track(const SightRead::Detail::MidiTrack& track)
{
    using namespace std::string_view_literals;

    constexpr std::array practice_section_prefixes {"[section "sv,
                                                    "[section_"sv, "[prc_"sv};

    std::vector<SightRead::PracticeSection> practice_sections;
    for (const auto& event : track.events) {
        const auto* meta_event
            = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
        if (meta_event == nullptr || meta_event->type != 1) {
            continue;
        }
        std::span<const std::uint8_t> section_name {meta_event->data.data(),
                                                    meta_event->data.size()};
        if (section_name.empty() || section_name.back() != ']') {
            continue;
        }
        section_name = section_name.first(section_name.size() - 1);
        for (auto prefix : practice_section_prefixes) {
            if (section_name.size() < prefix.size()
                || !std::equal(prefix.cbegin(), prefix.cend(),
                               section_name.begin())) {
                continue;
            }
            section_name = section_name.subspan(prefix.size());
            practice_sections.push_back(
                {.name = std::string {section_name.begin(), section_name.end()},
                 .start = SightRead::Tick {event.time}});
            break;
        }
    }
    return practice_sections;
}

void process_events_track(const SightRead::Detail::MidiTrack& track,
                          SightRead::SongGlobalData& global_data,
                          std::optional<SightRead::Tick>& coda_event_time)
{
    using namespace std::string_view_literals;

    constexpr std::string_view coda_text = "[coda]"sv;

    global_data.practice_sections(practice_sections_from_track(track));
    coda_event_time = std::nullopt;

    for (const auto& event : track.events) {
        const auto* meta_event
            = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
        if (meta_event == nullptr || meta_event->type != 1) {
            continue;
        }
        std::span<const std::uint8_t> event_text {meta_event->data.data(),
                                                  meta_event->data.size()};
        if (event_text.size() != coda_text.size()
            || !std::equal(coda_text.cbegin(), coda_text.cend(),
                           event_text.begin())) {
            continue;
        }
        coda_event_time = SightRead::Tick {event.time};
    }
}

bool is_five_lane_green_note(const SightRead::Detail::TimedEvent& event)
{
    constexpr std::array<std::uint8_t, 4> GREEN_LANE_KEYS {65, 77, 89, 101};
    constexpr int NOTE_OFF_ID = 0x80;
    constexpr int NOTE_ON_ID = 0x90;
    constexpr int UPPER_NIBBLE_MASK = 0xF0;

    const auto* midi_event
        = std::get_if<SightRead::Detail::MidiEvent>(&event.event);
    if (midi_event == nullptr) {
        return false;
    }
    const auto event_type = midi_event->status & UPPER_NIBBLE_MASK;
    if (event_type != NOTE_ON_ID && event_type != NOTE_OFF_ID) {
        return false;
    }
    const auto key = midi_event->data.at(0);
    return std::ranges::find(GREEN_LANE_KEYS, key)
        != std::ranges::end(GREEN_LANE_KEYS);
}

bool has_five_lane_green_notes(const SightRead::Detail::MidiTrack& midi_track)
{
    return std::ranges::find_if(midi_track.events, is_five_lane_green_note)
        != std::ranges::end(midi_track.events);
}

bool is_enable_chart_dynamics(const SightRead::Detail::TimedEvent& event)
{
    using namespace std::literals;
    constexpr std::array ENABLE_DYNAMICS_STRINGS {"[ENABLE_CHART_DYNAMICS]"sv,
                                                  "ENABLE_CHART_DYNAMICS"sv};

    const auto* meta_event
        = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
    if (meta_event == nullptr) {
        return false;
    }
    if (meta_event->type != 1) {
        return false;
    }
    return std::ranges::any_of(ENABLE_DYNAMICS_STRINGS, [&](const auto string) {
        return std::ranges::equal(meta_event->data, string);
    });
}

bool has_enable_chart_dynamics(const SightRead::Detail::MidiTrack& midi_track)
{
    return std::ranges::find_if(midi_track.events, is_enable_chart_dynamics)
        != std::ranges::end(midi_track.events);
}

bool is_enhanced_opens(const SightRead::Detail::TimedEvent& event)
{
    using namespace std::literals;
    constexpr std::array ENHANCED_OPENS_STRINGS {"[ENHANCED_OPENS]"sv};

    const auto* meta_event
        = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
    if (meta_event == nullptr) {
        return false;
    }
    if (meta_event->type != 1) {
        return false;
    }
    return std::ranges::any_of(ENHANCED_OPENS_STRINGS, [&](const auto string) {
        return std::ranges::equal(meta_event->data, string);
    });
}

bool has_enhanced_opens(const SightRead::Detail::MidiTrack& midi_track)
{
    return std::ranges::find_if(midi_track.events, is_enhanced_opens)
        != std::ranges::end(midi_track.events);
}

bool is_open_sysex_event(const SightRead::Detail::SysexEvent& event)
{
    constexpr std::array<std::tuple<std::size_t, int>, 6> REQUIRED_BYTES {
        std::tuple {0, 0x50}, {1, 0x53}, {2, 0}, {3, 0}, {5, 1}, {7, 0xF7}};
    constexpr std::array<std::tuple<std::size_t, int>, 2> UPPER_BOUNDS {
        std::tuple {4, 3}, {6, 1}};
    constexpr int SYSEX_DATA_SIZE = 8;

    if (event.data.size() != SYSEX_DATA_SIZE) {
        return false;
    }
    if (std::ranges::any_of(REQUIRED_BYTES, [&](const auto& pair) {
            return event.data.at(std::get<0>(pair)) != std::get<1>(pair);
        })) {
        return false;
    }
    return std::ranges::all_of(UPPER_BOUNDS, [&](const auto& pair) {
        return event.data.at(std::get<0>(pair)) <= std::get<1>(pair);
    });
}

std::optional<SightRead::Difficulty>
look_up_difficulty(const std::array<std::tuple<int, int, SightRead::Difficulty>,
                                    4>& diff_ranges,
                   std::uint8_t key)
{
    for (const auto& [min, max, diff] : diff_ranges) {
        if (key >= min && key <= max) {
            return {diff};
        }
    }

    return std::nullopt;
}

std::optional<SightRead::Difficulty>
difficulty_from_key(std::uint8_t key, SightRead::TrackType track_type,
                    bool enable_enhanced_opens)
{
    std::array<std::tuple<int, int, SightRead::Difficulty>, 4> diff_ranges;
    switch (track_type) {
    case SightRead::TrackType::FiveFret:
    case SightRead::TrackType::FortniteFestival:
        if (enable_enhanced_opens) {
            diff_ranges = {{{95, 102, SightRead::Difficulty::Expert}, // NOLINT
                            {83, 90, SightRead::Difficulty::Hard}, // NOLINT
                            {71, 78, SightRead::Difficulty::Medium}, // NOLINT
                            {59, 66, SightRead::Difficulty::Easy}}}; // NOLINT
        } else {
            diff_ranges = {{{96, 102, SightRead::Difficulty::Expert}, // NOLINT
                            {84, 90, SightRead::Difficulty::Hard}, // NOLINT
                            {72, 78, SightRead::Difficulty::Medium}, // NOLINT
                            {60, 66, SightRead::Difficulty::Easy}}}; // NOLINT
        }
        break;
    case SightRead::TrackType::SixFret:
        diff_ranges = {{{94, 102, SightRead::Difficulty::Expert}, // NOLINT
                        {82, 90, SightRead::Difficulty::Hard}, // NOLINT
                        {70, 78, SightRead::Difficulty::Medium}, // NOLINT
                        {58, 66, SightRead::Difficulty::Easy}}}; // NOLINT
        break;
    case SightRead::TrackType::Drums:
        diff_ranges = {{{95, 101, SightRead::Difficulty::Expert}, // NOLINT
                        {83, 89, SightRead::Difficulty::Hard}, // NOLINT
                        {71, 77, SightRead::Difficulty::Medium}, // NOLINT
                        {59, 65, SightRead::Difficulty::Easy}}}; // NOLINT
        break;
    }
    return look_up_difficulty(diff_ranges, key);
}

template <typename T, std::size_t N>
T colour_from_key_and_bounds(std::uint8_t key,
                             const std::array<unsigned int, 4>& diff_ranges,
                             const std::array<T, N>& colours)
{
    for (auto min : diff_ranges) {
        if (key >= min && (key - min) < colours.size()) {
            return colours.at(key - min);
        }
    }

    throw SightRead::ParseError("Invalid key for note");
}

int colour_from_key(std::uint8_t key, SightRead::TrackType track_type,
                    bool from_five_lane, bool enable_enhanced_opens)
{
    constexpr std::array FIVE_FRET_WITHOUT_OPENS_NOTE_COLOURS {
        SightRead::FIVE_FRET_GREEN, SightRead::FIVE_FRET_RED,
        SightRead::FIVE_FRET_YELLOW, SightRead::FIVE_FRET_BLUE,
        SightRead::FIVE_FRET_ORANGE};
    constexpr std::array FIVE_FRET_WITH_OPENS_NOTE_COLOURS {
        SightRead::FIVE_FRET_OPEN, SightRead::FIVE_FRET_GREEN,
        SightRead::FIVE_FRET_RED,  SightRead::FIVE_FRET_YELLOW,
        SightRead::FIVE_FRET_BLUE, SightRead::FIVE_FRET_ORANGE};
    constexpr std::array FIVE_FRET_WITHOUT_OPENS_DIFF_RANGES {96U, 84U, 72U,
                                                              60U};
    constexpr std::array FIVE_FRET_WITH_OPENS_DIFF_RANGES {95U, 83U, 71U, 59U};

    std::array<unsigned int, 4> diff_ranges {};
    switch (track_type) {
    case SightRead::TrackType::FiveFret:
    case SightRead::TrackType::FortniteFestival: {
        if (enable_enhanced_opens) {
            return colour_from_key_and_bounds(
                key, FIVE_FRET_WITH_OPENS_DIFF_RANGES,
                FIVE_FRET_WITH_OPENS_NOTE_COLOURS);
        }
        return colour_from_key_and_bounds(key,
                                          FIVE_FRET_WITHOUT_OPENS_DIFF_RANGES,
                                          FIVE_FRET_WITHOUT_OPENS_NOTE_COLOURS);
    }
    case SightRead::TrackType::SixFret: {
        diff_ranges = {94, 82, 70, 58}; // NOLINT
        constexpr std::array GHL_NOTE_COLOURS {
            SightRead::SIX_FRET_OPEN,      SightRead::SIX_FRET_WHITE_LOW,
            SightRead::SIX_FRET_WHITE_MID, SightRead::SIX_FRET_WHITE_HIGH,
            SightRead::SIX_FRET_BLACK_LOW, SightRead::SIX_FRET_BLACK_MID,
            SightRead::SIX_FRET_BLACK_HIGH};
        return colour_from_key_and_bounds(key, diff_ranges, GHL_NOTE_COLOURS);
    }
    case SightRead::TrackType::Drums: {
        diff_ranges = {95, 83, 71, 59}; // NOLINT
        constexpr std::array DRUM_NOTE_COLOURS {
            SightRead::DRUM_DOUBLE_KICK, SightRead::DRUM_KICK,
            SightRead::DRUM_RED,         SightRead::DRUM_YELLOW,
            SightRead::DRUM_BLUE,        SightRead::DRUM_GREEN};
        constexpr std::array FIVE_LANE_COLOURS {
            SightRead::DRUM_DOUBLE_KICK, SightRead::DRUM_KICK,
            SightRead::DRUM_RED,         SightRead::DRUM_YELLOW,
            SightRead::DRUM_BLUE,        SightRead::DRUM_GREEN,
            SightRead::DRUM_GREEN};
        if (from_five_lane) {
            return colour_from_key_and_bounds(key, diff_ranges,
                                              FIVE_LANE_COLOURS);
        }
        return colour_from_key_and_bounds(key, diff_ranges, DRUM_NOTE_COLOURS);
    }
    }

    throw std::invalid_argument("Invalid track type");
}

SightRead::NoteFlags flags_from_track_type(SightRead::TrackType track_type)
{
    switch (track_type) {
    case SightRead::TrackType::FiveFret:
    case SightRead::TrackType::FortniteFestival:
        return SightRead::FLAGS_FIVE_FRET_GUITAR;
    case SightRead::TrackType::SixFret:
        return SightRead::FLAGS_SIX_FRET_GUITAR;
    case SightRead::TrackType::Drums:
        return SightRead::FLAGS_DRUMS;
    }

    throw std::invalid_argument("Invalid track type");
}

bool is_cymbal_key(std::uint8_t key, bool from_five_lane)
{
    const auto index = (key + 1) % 12;
    if (from_five_lane) {
        return index == 3 || index == 5; // NOLINT
    }
    return index == 3 || index == 4 || index == 5; // NOLINT
}

SightRead::NoteFlags dynamics_flags_from_velocity(std::uint8_t velocity)
{
    constexpr std::uint8_t MIN_ACCENT_VELOCITY = 127;

    if (velocity == 1) {
        return SightRead::FLAGS_GHOST;
    }
    if (velocity >= MIN_ACCENT_VELOCITY) {
        return SightRead::FLAGS_ACCENT;
    }
    return static_cast<SightRead::NoteFlags>(0);
}

// Like combine_solo_events, but never skips on events to suit Midi parsing and
// checks if there is an unmatched on event.
// The tuples are a pair of the form (position, rank), where events later in the
// file have a higher rank. This is in case of the Note Off event being right
// after the corresponding Note On event in the file, but at the same tick.
//
// expand_length_zero_events is because some drum events have the length
// increased by 1 if the start and end are at the same time.
std::vector<std::tuple<int, int>>
combine_note_on_off_events(const std::vector<std::tuple<int, int>>& on_events,
                           const std::vector<std::tuple<int, int>>& off_events,
                           bool expand_length_zero_events = false)
{
    std::vector<std::tuple<int, int>> ranges;

    auto on_iter = on_events.cbegin();
    auto off_iter = off_events.cbegin();

    while (on_iter < on_events.cend() && off_iter < off_events.cend()) {
        if (*on_iter >= *off_iter) {
            ++off_iter;
            continue;
        }
        const auto start = std::get<0>(*on_iter);
        auto end = std::get<0>(*off_iter);
        if (start == end && expand_length_zero_events) {
            ++end;
        }
        ranges.emplace_back(start, end);
        ++on_iter;
        ++off_iter;
    }

    if (on_iter != on_events.cend()) {
        throw SightRead::ParseError("on event has no corresponding off event");
    }

    return ranges;
}

struct InstrumentMidiTrack {
public:
    std::map<std::tuple<SightRead::Difficulty, int, SightRead::NoteFlags>,
             std::vector<std::tuple<int, int>>>
        note_on_events;
    std::map<std::tuple<SightRead::Difficulty, int>,
             std::vector<std::tuple<int, int>>>
        note_off_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        open_on_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        open_off_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        tap_on_sysex_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        tap_off_sysex_events;
    std::vector<std::tuple<int, int>> yellow_tom_on_events;
    std::vector<std::tuple<int, int>> yellow_tom_off_events;
    std::vector<std::tuple<int, int>> blue_tom_on_events;
    std::vector<std::tuple<int, int>> blue_tom_off_events;
    std::vector<std::tuple<int, int>> green_tom_on_events;
    std::vector<std::tuple<int, int>> green_tom_off_events;
    std::vector<std::tuple<int, int>> solo_on_events;
    std::vector<std::tuple<int, int>> solo_off_events;
    std::vector<std::tuple<int, int>> sp_on_events;
    std::vector<std::tuple<int, int>> sp_off_events;
    std::vector<std::tuple<int, int>> tap_on_events;
    std::vector<std::tuple<int, int>> tap_off_events;
    std::vector<std::tuple<int, int>> flam_on_events;
    std::vector<std::tuple<int, int>> flam_off_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        force_hopo_on_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        force_hopo_off_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        force_strum_on_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        force_strum_off_events;
    std::vector<std::tuple<int, int>> fill_on_events;
    std::vector<std::tuple<int, int>> fill_off_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        disco_flip_on_events;
    std::map<SightRead::Difficulty, std::vector<std::tuple<int, int>>>
        disco_flip_off_events;

    InstrumentMidiTrack() = default;
};

bool is_tap_sysex_event(const SightRead::Detail::SysexEvent& event)
{
    constexpr std::array<std::tuple<std::size_t, int>, 6> REQUIRED_BYTES {
        std::tuple {0, 0x50}, {1, 0x53}, {2, 0}, {3, 0}, {5, 4}, {7, 0xF7}};
    constexpr int SYSEX_DATA_SIZE = 8;
    constexpr int DIFF_INDEX = 4;
    constexpr int PS_EVENT_VALUE_INDEX = 6;
    constexpr int ALL_DIFFICULTIES = 0xFF;

    if (event.data.size() != SYSEX_DATA_SIZE) {
        return false;
    }
    if (event.data.at(DIFF_INDEX) > 3
        && event.data.at(DIFF_INDEX) != ALL_DIFFICULTIES) {
        return false;
    }
    if (event.data.at(PS_EVENT_VALUE_INDEX) > 1) {
        return false;
    }
    return std::ranges::all_of(REQUIRED_BYTES, [&](const auto& pair) {
        return event.data.at(std::get<0>(pair)) == std::get<1>(pair);
    });
}

std::set<SightRead::Difficulty> difficulties_from_sysex_diff(std::uint8_t diff)
{
    constexpr int ALL_DIFFICULTIES = 0xFF;

    switch (diff) {
    case 0:
        return {SightRead::Difficulty::Easy};
    case 1:
        return {SightRead::Difficulty::Medium};
    case 2:
        return {SightRead::Difficulty::Hard};
    case 3:
        return {SightRead::Difficulty::Expert};
    case ALL_DIFFICULTIES:
        return {SightRead::Difficulty::Easy, SightRead::Difficulty::Medium,
                SightRead::Difficulty::Hard, SightRead::Difficulty::Expert};
    default:
        return {};
    }
}

void add_sysex_event(InstrumentMidiTrack& track,
                     const SightRead::Detail::SysexEvent& event, int time,
                     int rank)
{
    constexpr int SYSEX_ON_INDEX = 6;
    const auto diffs = difficulties_from_sysex_diff(event.data.at(4));

    for (auto diff : diffs) {
        if (is_open_sysex_event(event)) {
            if (event.data.at(SYSEX_ON_INDEX) == 0) {
                track.open_off_events[diff].emplace_back(time, rank);
            } else {
                track.open_on_events[diff].emplace_back(time, rank);
            }
        } else if (is_tap_sysex_event(event)) {
            if (event.data.at(SYSEX_ON_INDEX) == 0) {
                track.tap_off_sysex_events[diff].emplace_back(time, rank);
            } else {
                track.tap_on_sysex_events[diff].emplace_back(time, rank);
            }
        }
    }
}

void append_disco_flip(InstrumentMidiTrack& event_track,
                       const SightRead::Detail::MetaEvent& meta_event, int time,
                       int rank)
{
    constexpr int FLIP_START_SIZE = 15;
    constexpr int FLIP_END_SIZE = 14;
    constexpr int TEXT_EVENT_ID = 1;
    constexpr std::array<std::uint8_t, 5> MIX {{'[', 'm', 'i', 'x', ' '}};
    constexpr std::array<std::uint8_t, 6> DRUMS {
        {' ', 'd', 'r', 'u', 'm', 's'}};

    if (meta_event.type != TEXT_EVENT_ID) {
        return;
    }
    if (meta_event.data.size() != FLIP_START_SIZE
        && meta_event.data.size() != FLIP_END_SIZE) {
        return;
    }
    if (!std::equal(MIX.cbegin(), MIX.cend(), meta_event.data.cbegin())) {
        return;
    }
    if (!std::equal(DRUMS.cbegin(), DRUMS.cend(),
                    meta_event.data.cbegin() + MIX.size() + 1)) {
        return;
    }
    const auto diff = static_cast<SightRead::Difficulty>(
        meta_event.data.at(MIX.size()) - '0');
    if (meta_event.data.size() == FLIP_END_SIZE
        && meta_event.data.at(FLIP_END_SIZE - 1) == ']') {
        event_track.disco_flip_off_events[diff].emplace_back(time, rank);
    } else if (meta_event.data.size() == FLIP_START_SIZE
               && meta_event.data.at(FLIP_START_SIZE - 2) == 'd'
               && meta_event.data.at(FLIP_START_SIZE - 1) == ']') {
        event_track.disco_flip_on_events[diff].emplace_back(time, rank);
    }
}

bool force_hopo_key(std::uint8_t key, SightRead::TrackType track_type)
{
    constexpr std::array FORCE_HOPO_KEYS {65, 77, 89, 101};
    if (track_type == SightRead::TrackType::Drums) {
        return false;
    }
    return std::ranges::find(FORCE_HOPO_KEYS, key)
        != std::ranges::end(FORCE_HOPO_KEYS);
}

bool force_strum_key(std::uint8_t key, SightRead::TrackType track_type)
{
    constexpr std::array FORCE_STRUM_KEYS {66, 78, 90, 102};
    if (track_type == SightRead::TrackType::Drums) {
        return false;
    }
    return std::ranges::find(FORCE_STRUM_KEYS, key)
        != std::ranges::end(FORCE_STRUM_KEYS);
}

void add_note_off_event(InstrumentMidiTrack& track,
                        const std::array<std::uint8_t, 2>& data, int time,
                        int rank, bool from_five_lane,
                        bool enable_enhanced_opens,
                        SightRead::TrackType track_type)
{
    constexpr int YELLOW_TOM_ID = 110;
    constexpr int BLUE_TOM_ID = 111;
    constexpr int GREEN_TOM_ID = 112;
    constexpr int SOLO_NOTE_ID = 103;
    constexpr int SP_NOTE_ID = 116;
    constexpr int TAP_NOTE_ID = 104;
    constexpr int DRUM_FILL_ID = 120;
    constexpr int FLAM_MARKER_ID = 109;

    const auto diff
        = difficulty_from_key(data.at(0), track_type, enable_enhanced_opens);
    if (diff.has_value()) {
        if (force_hopo_key(data.at(0), track_type)) {
            track.force_hopo_off_events[*diff].emplace_back(time, rank);
        } else if (force_strum_key(data.at(0), track_type)) {
            track.force_strum_off_events[*diff].emplace_back(time, rank);
        } else {
            const auto colour = colour_from_key(
                data.at(0), track_type, from_five_lane, enable_enhanced_opens);
            track.note_off_events[{*diff, colour}].emplace_back(time, rank);
        }
    } else {
        switch (data.at(0)) {
        case YELLOW_TOM_ID:
            track.yellow_tom_off_events.emplace_back(time, rank);
            break;
        case BLUE_TOM_ID:
            track.blue_tom_off_events.emplace_back(time, rank);
            break;
        case GREEN_TOM_ID:
            track.green_tom_off_events.emplace_back(time, rank);
            break;
        case SOLO_NOTE_ID:
            track.solo_off_events.emplace_back(time, rank);
            break;
        case SP_NOTE_ID:
            track.sp_off_events.emplace_back(time, rank);
            break;
        case TAP_NOTE_ID:
            track.tap_off_events.emplace_back(time, rank);
            break;
        case DRUM_FILL_ID:
            track.fill_off_events.emplace_back(time, rank);
            break;
        case FLAM_MARKER_ID:
            track.flam_off_events.emplace_back(time, rank);
            break;
        default:
            break;
        }
    }
}

void add_note_on_event(InstrumentMidiTrack& track,
                       const std::array<std::uint8_t, 2>& data, int time,
                       int rank, bool from_five_lane, bool parse_dynamics,
                       bool enable_enhanced_opens,
                       SightRead::TrackType track_type)
{
    constexpr int YELLOW_TOM_ID = 110;
    constexpr int BLUE_TOM_ID = 111;
    constexpr int GREEN_TOM_ID = 112;
    constexpr int SOLO_NOTE_ID = 103;
    constexpr int SP_NOTE_ID = 116;
    constexpr int TAP_NOTE_ID = 104;
    constexpr int DRUM_FILL_ID = 120;
    constexpr int FLAM_MARKER_ID = 109;

    // Velocity 0 Note On events are counted as Note Off events.
    if (data.at(1) == 0) {
        add_note_off_event(track, data, time, rank, from_five_lane,
                           enable_enhanced_opens, track_type);
        return;
    }

    const auto diff
        = difficulty_from_key(data.at(0), track_type, enable_enhanced_opens);
    if (diff.has_value()) {
        if (force_hopo_key(data.at(0), track_type)) {
            track.force_hopo_on_events[*diff].emplace_back(time, rank);
        } else if (force_strum_key(data.at(0), track_type)) {
            track.force_strum_on_events[*diff].emplace_back(time, rank);
        } else {
            auto colour = colour_from_key(
                data.at(0), track_type, from_five_lane, enable_enhanced_opens);
            auto flags = flags_from_track_type(track_type);
            if (track_type == SightRead::TrackType::Drums) {
                if (is_cymbal_key(data.at(0), from_five_lane)) {
                    flags = static_cast<SightRead::NoteFlags>(
                        flags | SightRead::FLAGS_CYMBAL);
                }
                if (parse_dynamics) {
                    flags = static_cast<SightRead::NoteFlags>(
                        flags | dynamics_flags_from_velocity(data.at(1)));
                }
            }
            track.note_on_events[{*diff, colour, flags}].emplace_back(time,
                                                                      rank);
        }
    } else {
        switch (data.at(0)) {
        case YELLOW_TOM_ID:
            track.yellow_tom_on_events.emplace_back(time, rank);
            break;
        case BLUE_TOM_ID:
            track.blue_tom_on_events.emplace_back(time, rank);
            break;
        case GREEN_TOM_ID:
            track.green_tom_on_events.emplace_back(time, rank);
            break;
        case SOLO_NOTE_ID:
            track.solo_on_events.emplace_back(time, rank);
            break;
        case SP_NOTE_ID:
            track.sp_on_events.emplace_back(time, rank);
            break;
        case TAP_NOTE_ID:
            track.tap_on_events.emplace_back(time, rank);
            break;
        case DRUM_FILL_ID:
            track.fill_on_events.emplace_back(time, rank);
            break;
        case FLAM_MARKER_ID:
            track.flam_on_events.emplace_back(time, rank);
            break;
        default:
            break;
        }
    }
}

InstrumentMidiTrack
read_instrument_midi_track(const SightRead::Detail::MidiTrack& midi_track,
                           SightRead::TrackType track_type)
{
    constexpr int NOTE_OFF_ID = 0x80;
    constexpr int NOTE_ON_ID = 0x90;
    constexpr int UPPER_NIBBLE_MASK = 0xF0;
    constexpr std::array DIFFICULTIES {
        SightRead::Difficulty::Easy, SightRead::Difficulty::Medium,
        SightRead::Difficulty::Hard, SightRead::Difficulty::Expert};

    const bool from_five_lane = track_type == SightRead::TrackType::Drums
        && has_five_lane_green_notes(midi_track);
    const bool parse_dynamics = track_type == SightRead::TrackType::Drums
        && has_enable_chart_dynamics(midi_track);
    const bool enable_enhanced_opens
        = track_type == SightRead::TrackType::FiveFret
        && has_enhanced_opens(midi_track);

    InstrumentMidiTrack event_track;
    for (auto d : DIFFICULTIES) {
        event_track.disco_flip_on_events[d] = {};
        event_track.disco_flip_off_events[d] = {};
        event_track.force_hopo_on_events[d] = {};
        event_track.force_hopo_off_events[d] = {};
        event_track.force_strum_on_events[d] = {};
        event_track.force_strum_off_events[d] = {};
    }

    int rank = 0;
    for (const auto& event : midi_track.events) {
        ++rank;
        const auto* midi_event
            = std::get_if<SightRead::Detail::MidiEvent>(&event.event);
        if (midi_event == nullptr) {
            const auto* sysex_event
                = std::get_if<SightRead::Detail::SysexEvent>(&event.event);
            if (sysex_event != nullptr) {
                add_sysex_event(event_track, *sysex_event, event.time, rank);
                continue;
            }
            if (track_type == SightRead::TrackType::Drums) {
                const auto* meta_event
                    = std::get_if<SightRead::Detail::MetaEvent>(&event.event);
                if (meta_event != nullptr) {
                    append_disco_flip(event_track, *meta_event, event.time,
                                      rank);
                }
            }

            continue;
        }
        switch (midi_event->status & UPPER_NIBBLE_MASK) {
        case NOTE_OFF_ID:
            add_note_off_event(event_track, midi_event->data, event.time, rank,
                               from_five_lane, enable_enhanced_opens,
                               track_type);
            break;
        case NOTE_ON_ID:
            add_note_on_event(event_track, midi_event->data, event.time, rank,
                              from_five_lane, parse_dynamics,
                              enable_enhanced_opens, track_type);
            break;
        default:
            break;
        }
    }

    event_track.disco_flip_off_events.at(SightRead::Difficulty::Easy)
        .emplace_back(std::numeric_limits<int>::max(), ++rank);
    event_track.disco_flip_off_events.at(SightRead::Difficulty::Medium)
        .emplace_back(std::numeric_limits<int>::max(), ++rank);
    event_track.disco_flip_off_events.at(SightRead::Difficulty::Hard)
        .emplace_back(std::numeric_limits<int>::max(), ++rank);
    event_track.disco_flip_off_events.at(SightRead::Difficulty::Expert)
        .emplace_back(std::numeric_limits<int>::max(), ++rank);

    if (event_track.sp_on_events.empty()
        && event_track.solo_on_events.size() > 1) {
        std::swap(event_track.sp_off_events, event_track.solo_off_events);
        std::swap(event_track.sp_on_events, event_track.solo_on_events);
    }

    return event_track;
}

void apply_forcing(
    std::map<SightRead::Difficulty, std::vector<SightRead::Note>>& notes,
    const InstrumentMidiTrack& event_track,
    const std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>>& tap_events)
{
    constexpr std::array DIFFICULTIES {
        SightRead::Difficulty::Easy, SightRead::Difficulty::Medium,
        SightRead::Difficulty::Hard, SightRead::Difficulty::Expert};

    const HalfOpenIntervalSet<int> tap_note_events {combine_note_on_off_events(
        event_track.tap_on_events, event_track.tap_off_events)};

    std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>> force_hopo_events;
    std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>>
        force_strum_events;
    for (auto d : DIFFICULTIES) {
        force_hopo_events.emplace(d,
                                  combine_note_on_off_events(
                                      event_track.force_hopo_on_events.at(d),
                                      event_track.force_hopo_off_events.at(d)));
        force_strum_events.emplace(
            d,
            combine_note_on_off_events(
                event_track.force_strum_on_events.at(d),
                event_track.force_strum_off_events.at(d)));
    }

    for (auto& [diff, note_array] : notes) {
        for (auto& note : note_array) {
            const auto pos = note.position.value();
            if (tap_note_events.contains(pos)) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags | SightRead::FLAGS_TAP);
            }
            const auto tap_events_iter = tap_events.find(diff);
            if (tap_events_iter != tap_events.cend()
                && tap_events_iter->second.contains(pos)) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags | SightRead::FLAGS_TAP);
            }
            if (force_hopo_events.at(diff).contains(pos)) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags | SightRead::FLAGS_FORCE_HOPO);
            }
            if (force_strum_events.at(diff).contains(pos)) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags | SightRead::FLAGS_FORCE_STRUM);
            }
        }
    }
}

std::map<SightRead::Difficulty, std::vector<SightRead::Note>>
notes_from_event_track(
    const InstrumentMidiTrack& event_track,
    const std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>>&
        open_events,
    const std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>>& tap_events,
    SightRead::TrackType track_type, int sustain_cutoff_threshold)
{
    std::map<SightRead::Difficulty, std::vector<SightRead::Note>> notes;
    for (const auto& [key, note_ons] : event_track.note_on_events) {
        const auto& [diff, colour, flags] = key;
        if (!event_track.note_off_events.contains({diff, colour})) {
            throw SightRead::ParseError("No corresponding Note Off events");
        }
        const auto& note_offs = event_track.note_off_events.at({diff, colour});
        for (const auto& [pos, end] :
             combine_note_on_off_events(note_ons, note_offs)) {
            auto note_length = end - pos;
            if (note_length <= sustain_cutoff_threshold) {
                note_length = 0;
            }
            auto note_colour = colour;
            if (track_type == SightRead::TrackType::FiveFret) {
                const auto open_events_iter = open_events.find(diff);
                if (open_events_iter != open_events.cend()
                    && open_events_iter->second.contains(pos)) {
                    note_colour = SightRead::FIVE_FRET_OPEN;
                }
            }
            SightRead::Note note;
            note.position = SightRead::Tick {pos};
            note.lengths.at(static_cast<unsigned int>(note_colour))
                = SightRead::Tick {note_length};
            note.flags = flags_from_track_type(track_type);
            notes[diff].push_back(note);
        }
    }

    if (track_type != SightRead::TrackType::Drums) {
        apply_forcing(notes, event_track, tap_events);
    }

    return notes;
}

std::map<SightRead::Difficulty, SightRead::NoteTrack> ghl_note_tracks_from_midi(
    const SightRead::Detail::MidiTrack& midi_track,
    const std::shared_ptr<SightRead::SongGlobalData>& global_data,
    const SightRead::HopoThreshold& hopo_threshold,
    int sustain_cutoff_threshold, bool permit_solos, bool allow_open_chords)
{
    const auto event_track
        = read_instrument_midi_track(midi_track, SightRead::TrackType::SixFret);

    const auto notes = notes_from_event_track(event_track, {}, {},
                                              SightRead::TrackType::SixFret,
                                              sustain_cutoff_threshold);

    std::vector<SightRead::StarPower> sp_phrases;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.sp_on_events, event_track.sp_off_events)) {
        sp_phrases.push_back({.position = SightRead::Tick {start},
                              .length = SightRead::Tick {end - start}});
    }

    std::map<SightRead::Difficulty, SightRead::NoteTrack> note_tracks;
    for (const auto& [diff, note_set] : notes) {
        std::vector<int> solo_ons;
        std::vector<int> solo_offs;
        solo_ons.reserve(event_track.solo_on_events.size());
        for (const auto& [pos, rank] : event_track.solo_on_events) {
            solo_ons.push_back(pos);
        }
        solo_offs.reserve(event_track.solo_off_events.size());
        for (const auto& [pos, rank] : event_track.solo_off_events) {
            solo_offs.push_back(pos);
        }
        auto solos = SightRead::Detail::form_solo_vector(
            solo_ons, solo_offs, note_set, SightRead::TrackType::SixFret, true);
        if (!permit_solos) {
            solos.clear();
        }
        SightRead::NoteTrack note_track {
            note_set, SightRead::TrackType::SixFret, global_data,
            allow_open_chords,
            hopo_threshold.midi_max_hopo_gap(global_data->resolution())};
        note_track.sp_phrases(sp_phrases);
        note_track.solos(std::move(solos));
        note_tracks.emplace(diff, std::move(note_track));
    }

    return note_tracks;
}

class TomEvents {
private:
    HalfOpenIntervalSet<int> m_yellow_tom_events;
    HalfOpenIntervalSet<int> m_blue_tom_events;
    HalfOpenIntervalSet<int> m_green_tom_events;

public:
    explicit TomEvents(const InstrumentMidiTrack& events)
        : m_yellow_tom_events {combine_note_on_off_events(
              events.yellow_tom_on_events, events.yellow_tom_off_events, true)}
        , m_blue_tom_events {combine_note_on_off_events(
              events.blue_tom_on_events, events.blue_tom_off_events, true)}
        , m_green_tom_events {combine_note_on_off_events(
              events.green_tom_on_events, events.green_tom_off_events, true)}
    {
    }

    [[nodiscard]] bool force_tom(int colour, int pos) const
    {
        switch (colour) {
        case SightRead::DRUM_YELLOW:
            return m_yellow_tom_events.contains(pos);
        case SightRead::DRUM_BLUE:
            return m_blue_tom_events.contains(pos);
        case SightRead::DRUM_GREEN:
            return m_green_tom_events.contains(pos);
        default:
            return false;
        }
    }
};

// This is to deal with G cymbal + G tom from five lane being turned into G
// cymbal + B tom. This combination cannot happen from a four lane chart.
void fix_double_greens(std::vector<SightRead::Note>& notes)
{
    std::set<SightRead::Tick> green_cymbal_positions;

    for (const auto& note : notes) {
        if ((note.lengths.at(SightRead::DRUM_GREEN) != SightRead::Tick {-1})
            && ((note.flags & SightRead::FLAGS_CYMBAL) != 0U)) {
            green_cymbal_positions.insert(note.position);
        }
    }

    for (auto& note : notes) {
        if ((note.lengths.at(SightRead::DRUM_GREEN) == SightRead::Tick {-1})
            || ((note.flags & SightRead::FLAGS_CYMBAL) != 0U)) {
            continue;
        }
        if (green_cymbal_positions.contains(note.position)) {
            std::swap(note.lengths.at(SightRead::DRUM_BLUE),
                      note.lengths.at(SightRead::DRUM_GREEN));
        }
    }
}

std::map<SightRead::Difficulty, SightRead::NoteTrack>
drum_note_tracks_from_midi(
    const SightRead::Detail::MidiTrack& midi_track,
    const std::shared_ptr<SightRead::SongGlobalData>& global_data,
    bool permit_solos, std::optional<SightRead::Tick> coda_event_time)
{
    const auto event_track
        = read_instrument_midi_track(midi_track, SightRead::TrackType::Drums);

    const TomEvents tom_events {event_track};

    std::map<SightRead::Difficulty, std::vector<SightRead::Note>> notes;
    for (const auto& [key, note_ons] : event_track.note_on_events) {
        const auto& [diff, colour, flags] = key;
        const std::tuple<SightRead::Difficulty, int> no_flags_key {diff,
                                                                   colour};
        if (!event_track.note_off_events.contains(no_flags_key)) {
            throw SightRead::ParseError("No corresponding Note Off events");
        }
        const auto& note_offs = event_track.note_off_events.at(no_flags_key);
        for (const auto& [pos, end] :
             combine_note_on_off_events(note_ons, note_offs)) {
            SightRead::Note note;
            note.position = SightRead::Tick {pos};
            note.lengths.at(static_cast<unsigned int>(colour))
                = SightRead::Tick {0};
            note.flags = flags;
            if (tom_events.force_tom(colour, pos)) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags & ~SightRead::FLAGS_CYMBAL);
            }
            notes[diff].push_back(note);
        }
        fix_double_greens(notes[diff]);
    }

    std::vector<SightRead::StarPower> sp_phrases;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.sp_on_events, event_track.sp_off_events)) {
        sp_phrases.push_back({.position = SightRead::Tick {start},
                              .length = SightRead::Tick {end - start}});
    }

    std::vector<SightRead::BigRockEnding> bres;
    std::vector<SightRead::DrumFill> drum_fills;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.fill_on_events, event_track.fill_off_events)) {
        if (coda_event_time.has_value() && coda_event_time->value() <= start) {
            bres.push_back({.start = SightRead::Tick {start},
                            .end = SightRead::Tick {end}});
        } else {
            drum_fills.push_back({.position = SightRead::Tick {start},
                                  .length = SightRead::Tick {end - start}});
        }
    }

    std::vector<SightRead::FlamMarker> flam_markers;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.flam_on_events, event_track.flam_off_events)) {
        flam_markers.push_back({.position = SightRead::Tick {start},
                                .length = SightRead::Tick {end - start}});
    }

    std::map<SightRead::Difficulty, SightRead::NoteTrack> note_tracks;
    for (const auto& [diff, note_set] : notes) {
        std::vector<int> solo_ons;
        std::vector<int> solo_offs;
        solo_ons.reserve(event_track.solo_on_events.size());
        for (const auto& [pos, rank] : event_track.solo_on_events) {
            solo_ons.push_back(pos);
        }
        solo_offs.reserve(event_track.solo_off_events.size());
        for (const auto& [pos, rank] : event_track.solo_off_events) {
            solo_offs.push_back(pos);
        }
        std::vector<SightRead::DiscoFlip> disco_flips;
        for (const auto& [start, end] : combine_note_on_off_events(
                 event_track.disco_flip_on_events.at(diff),
                 event_track.disco_flip_off_events.at(diff))) {
            disco_flips.push_back({.position = SightRead::Tick {start},
                                   .length = SightRead::Tick {end - start}});
        }
        auto solos = SightRead::Detail::form_solo_vector(
            solo_ons, solo_offs, note_set, SightRead::TrackType::Drums, true);
        if (!permit_solos) {
            solos.clear();
        }
        SightRead::NoteTrack note_track {note_set, SightRead::TrackType::Drums,
                                         global_data};
        note_track.sp_phrases(sp_phrases);
        note_track.solos(std::move(solos));
        note_track.bres(bres);
        note_track.drum_fills(drum_fills);
        note_track.disco_flips(disco_flips);
        note_track.flam_markers(flam_markers);
        note_tracks.emplace(diff, std::move(note_track));
    }

    return note_tracks;
}

std::vector<SightRead::BigRockEnding>
read_bres(const InstrumentMidiTrack& event_track,
          std::optional<SightRead::Tick> coda_event_time)
{
    std::vector<SightRead::BigRockEnding> bres;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.fill_on_events, event_track.fill_off_events)) {
        if (coda_event_time.has_value() && coda_event_time->value() <= start) {
            bres.push_back({.start = SightRead::Tick {start},
                            .end = SightRead::Tick {end}});
        }
    }

    return bres;
}

bool is_fortnite_instrument(SightRead::Instrument instrument)
{
    const std::set<SightRead::Instrument> fortnite_instruments {
        SightRead::Instrument::FortniteGuitar,
        SightRead::Instrument::FortniteBass,
        SightRead::Instrument::FortniteDrums,
        SightRead::Instrument::FortniteVocals,
        SightRead::Instrument::FortniteProGuitar,
        SightRead::Instrument::FortniteProBass};
    return fortnite_instruments.contains(instrument);
}

std::map<SightRead::Difficulty, SightRead::NoteTrack>
fortnite_note_tracks_from_midi(
    const SightRead::Detail::MidiTrack& midi_track,
    const std::shared_ptr<SightRead::SongGlobalData>& global_data,
    int sustain_cutoff_threshold, bool permit_solos,
    std::optional<SightRead::Tick> coda_event_time)
{
    const auto event_track = read_instrument_midi_track(
        midi_track, SightRead::TrackType::FortniteFestival);
    const auto bres = read_bres(event_track, coda_event_time);

    const auto notes = notes_from_event_track(
        event_track, {}, {}, SightRead::TrackType::FortniteFestival,
        sustain_cutoff_threshold);

    std::vector<SightRead::StarPower> sp_phrases;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.sp_on_events, event_track.sp_off_events)) {
        sp_phrases.push_back({.position = SightRead::Tick {start},
                              .length = SightRead::Tick {end - start}});
    }

    std::map<SightRead::Difficulty, SightRead::NoteTrack> note_tracks;
    for (const auto& [diff, note_set] : notes) {
        std::vector<int> solo_ons;
        std::vector<int> solo_offs;
        solo_ons.reserve(event_track.solo_on_events.size());
        for (const auto& [pos, rank] : event_track.solo_on_events) {
            solo_ons.push_back(pos);
        }
        solo_offs.reserve(event_track.solo_off_events.size());
        for (const auto& [pos, rank] : event_track.solo_off_events) {
            solo_offs.push_back(pos);
        }
        auto solos = SightRead::Detail::form_solo_vector(
            solo_ons, solo_offs, note_set,
            SightRead::TrackType::FortniteFestival, true);
        if (!permit_solos) {
            solos.clear();
        }
        SightRead::NoteTrack note_track {
            note_set, SightRead::TrackType::FortniteFestival, global_data};
        note_track.sp_phrases(sp_phrases);
        note_track.solos(std::move(solos));
        note_track.bres(bres);
        note_tracks.emplace(diff, std::move(note_track));
    }

    return note_tracks;
}

std::map<SightRead::Difficulty, SightRead::NoteTrack> note_tracks_from_midi(
    const SightRead::Detail::MidiTrack& midi_track,
    const std::shared_ptr<SightRead::SongGlobalData>& global_data,
    const SightRead::HopoThreshold& hopo_threshold,
    int sustain_cutoff_threshold, bool permit_solos, bool allow_open_chords,
    std::optional<SightRead::Tick> coda_event_time)
{
    const auto event_track = read_instrument_midi_track(
        midi_track, SightRead::TrackType::FiveFret);
    const auto bres = read_bres(event_track, coda_event_time);

    std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>> open_events;
    for (const auto& [diff, open_ons] : event_track.open_on_events) {
        if (!event_track.open_off_events.contains(diff)) {
            throw SightRead::ParseError("No open Note Off events");
        }
        const auto& open_offs = event_track.open_off_events.at(diff);
        open_events.emplace(diff,
                            combine_note_on_off_events(open_ons, open_offs));
    }

    std::map<SightRead::Difficulty, HalfOpenIntervalSet<int>> tap_events;
    for (const auto& [diff, tap_ons] : event_track.tap_on_sysex_events) {
        if (!event_track.tap_off_sysex_events.contains(diff)) {
            throw SightRead::ParseError("No tap Note Off events");
        }
        const auto& tap_offs = event_track.tap_off_sysex_events.at(diff);
        tap_events.emplace(diff, combine_note_on_off_events(tap_ons, tap_offs));
    }

    const auto notes = notes_from_event_track(
        event_track, open_events, tap_events, SightRead::TrackType::FiveFret,
        sustain_cutoff_threshold);

    std::vector<SightRead::StarPower> sp_phrases;
    for (const auto& [start, end] : combine_note_on_off_events(
             event_track.sp_on_events, event_track.sp_off_events)) {
        sp_phrases.push_back({.position = SightRead::Tick {start},
                              .length = SightRead::Tick {end - start}});
    }

    std::map<SightRead::Difficulty, SightRead::NoteTrack> note_tracks;
    for (const auto& [diff, note_set] : notes) {
        std::vector<int> solo_ons;
        std::vector<int> solo_offs;
        solo_ons.reserve(event_track.solo_on_events.size());
        for (const auto& [pos, rank] : event_track.solo_on_events) {
            solo_ons.push_back(pos);
        }
        solo_offs.reserve(event_track.solo_off_events.size());
        for (const auto& [pos, rank] : event_track.solo_off_events) {
            solo_offs.push_back(pos);
        }
        auto solos = SightRead::Detail::form_solo_vector(
            solo_ons, solo_offs, note_set, SightRead::TrackType::FiveFret,
            true);
        if (!permit_solos) {
            solos.clear();
        }
        SightRead::NoteTrack note_track {
            note_set, SightRead::TrackType::FiveFret, global_data,
            allow_open_chords,
            hopo_threshold.midi_max_hopo_gap(global_data->resolution())};
        note_track.sp_phrases(sp_phrases);
        note_track.solos(std::move(solos));
        note_track.bres(bres);
        note_tracks.emplace(diff, std::move(note_track));
    }

    return note_tracks;
}
}

SightRead::Detail::MidiConverter::MidiConverter(SightRead::Metadata metadata)
    : m_metadata {std::move(metadata)}
    , m_permitted_instruments {SightRead::all_instruments()}
    , m_permit_solos {true}
    , m_allow_open_chords {false}
    , m_use_sustain_cutoff_threshold {false}
{
}

SightRead::Detail::MidiConverter&
SightRead::Detail::MidiConverter::permit_instruments(
    std::set<SightRead::Instrument> permitted_instruments)
{
    m_permitted_instruments = std::move(permitted_instruments);
    return *this;
}

SightRead::Detail::MidiConverter&
SightRead::Detail::MidiConverter::parse_solos(bool permit_solos)
{
    m_permit_solos = permit_solos;
    return *this;
}

SightRead::Detail::MidiConverter&
SightRead::Detail::MidiConverter::allow_open_chords(bool allow_open_chords)
{
    m_allow_open_chords = allow_open_chords;
    return *this;
}

SightRead::Detail::MidiConverter&
SightRead::Detail::MidiConverter::use_sustain_cutoff_threshold(
    bool use_sustain_cutoff_threshold)
{
    m_use_sustain_cutoff_threshold = use_sustain_cutoff_threshold;
    return *this;
}

int SightRead::Detail::MidiConverter::sustain_cutoff_threshold(
    int resolution) const
{
    if (m_use_sustain_cutoff_threshold
        && m_metadata.sustain_cutoff_threshold.has_value()) {
        return *m_metadata.sustain_cutoff_threshold;
    }

    return resolution / 3;
}

std::optional<SightRead::Instrument>
SightRead::Detail::MidiConverter::midi_section_instrument(
    const std::string& track_name) const
{
    const std::map<std::string, std::vector<SightRead::Instrument>>
        INSTRUMENTS {
            {"PART GUITAR",
             {SightRead::Instrument::Guitar,
              SightRead::Instrument::FortniteGuitar}},
            {"T1 GEMS", {SightRead::Instrument::Guitar}},
            {"PART GUITAR COOP", {SightRead::Instrument::GuitarCoop}},
            {"PART BASS",
             {SightRead::Instrument::Bass,
              SightRead::Instrument::FortniteBass}},
            {"PART RHYTHM", {SightRead::Instrument::Rhythm}},
            {"PART KEYS", {SightRead::Instrument::Keys}},
            {"PART GUITAR GHL", {SightRead::Instrument::GHLGuitar}},
            {"PART BASS GHL", {SightRead::Instrument::GHLBass}},
            {"PART RHYTHM GHL", {SightRead::Instrument::GHLRhythm}},
            {"PART GUITAR COOP GHL", {SightRead::Instrument::GHLGuitarCoop}},
            {"PART DRUMS",
             {SightRead::Instrument::Drums,
              SightRead::Instrument::FortniteDrums}},
            {"PART VOCALS", {SightRead::Instrument::FortniteVocals}},
            {"PLASTIC GUITAR", {SightRead::Instrument::FortniteProGuitar}},
            {"PLASTIC BASS", {SightRead::Instrument::FortniteProBass}}};

    const auto iter = INSTRUMENTS.find(track_name);
    if (iter == INSTRUMENTS.end()) {
        return std::nullopt;
    }
    for (auto instrument : iter->second) {
        if (m_permitted_instruments.contains(instrument)) {
            return instrument;
        }
    }
    return std::nullopt;
}

void SightRead::Detail::MidiConverter::process_instrument_track(
    const std::string& track_name, const SightRead::Detail::MidiTrack& track,
    SightRead::Song& song, std::optional<SightRead::Tick> coda_event_time) const
{
    const auto inst = midi_section_instrument(track_name);
    if (!inst.has_value()) {
        return;
    }

    const auto sustain_threshold
        = sustain_cutoff_threshold(song.global_data().resolution());
    if (is_fortnite_instrument(*inst)) {
        auto tracks = fortnite_note_tracks_from_midi(
            track, song.global_data_ptr(), sustain_threshold, m_permit_solos,
            coda_event_time);
        for (auto& [diff, note_track] : tracks) {
            song.add_note_track(*inst, diff, std::move(note_track));
        }
    } else if (SightRead::Detail::is_six_fret_instrument(*inst)) {
        auto tracks = ghl_note_tracks_from_midi(
            track, song.global_data_ptr(), m_metadata.hopo_threshold,
            sustain_threshold, m_permit_solos, m_allow_open_chords);
        for (auto& [diff, note_track] : tracks) {
            song.add_note_track(*inst, diff, std::move(note_track));
        }
    } else if (*inst == SightRead::Instrument::Drums) {
        auto tracks = drum_note_tracks_from_midi(
            track, song.global_data_ptr(), m_permit_solos, coda_event_time);
        for (auto& [diff, note_track] : tracks) {
            song.add_note_track(*inst, diff, std::move(note_track));
        }
    } else {
        auto tracks = note_tracks_from_midi(
            track, song.global_data_ptr(), m_metadata.hopo_threshold,
            sustain_threshold, m_permit_solos, m_allow_open_chords,
            coda_event_time);
        for (auto& [diff, note_track] : tracks) {
            song.add_note_track(*inst, diff, std::move(note_track));
        }
    }
}

SightRead::Song SightRead::Detail::MidiConverter::convert(
    const SightRead::Detail::Midi& midi) const
{
    if (midi.ticks_per_quarter_note == 0) {
        throw SightRead::ParseError("Resolution must be > 0");
    }

    SightRead::Song song;

    song.global_data().is_from_midi(true);
    song.global_data().resolution(midi.ticks_per_quarter_note);
    song.global_data().name(m_metadata.name);
    song.global_data().artist(m_metadata.artist);
    song.global_data().charter(m_metadata.charter);

    if (midi.tracks.empty()) {
        return song;
    }

    song.global_data().tempo_map(
        read_first_midi_track(midi.tracks.at(0), midi.ticks_per_quarter_note));

    std::map<std::string, MidiTrack> tracks_with_names;
    for (const auto& track : midi.tracks) {
        const auto track_name = midi_track_name(track);
        if (track_name.has_value()) {
            tracks_with_names.insert({*track_name, track});
        }
    }

    const auto beat_iter = tracks_with_names.find("BEAT");
    if (beat_iter != tracks_with_names.end()) {
        song.global_data().od_beats(od_beats_from_track(beat_iter->second));
    }

    std::optional<SightRead::Tick> coda_event_time;
    const auto events_iter = tracks_with_names.find("EVENTS");
    if (events_iter != tracks_with_names.end()) {
        process_events_track(events_iter->second, song.global_data(),
                             coda_event_time);
    }

    for (const auto& [name, track] : tracks_with_names) {
        if (name == "BEAT" || name == "EVENTS") {
            continue;
        }
        process_instrument_track(name, track, song, coda_event_time);
    }

    const auto& od_beats = song.global_data().od_beats();
    if (!od_beats.empty()) {
        auto old_tempo_map = song.global_data().tempo_map();
        SightRead::TempoMap new_tempo_map {old_tempo_map.time_sigs(),
                                           old_tempo_map.bpms(), od_beats,
                                           midi.ticks_per_quarter_note};
        song.global_data().tempo_map(new_tempo_map);
    }

    return song;
}
