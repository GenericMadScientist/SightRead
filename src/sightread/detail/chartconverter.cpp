#include <algorithm>
#include <climits>
#include <limits>
#include <map>
#include <optional>
#include <tuple>
#include <utility>

#include "sightread/detail/chartconverter.hpp"
#include "sightread/detail/drumtracktype.hpp"
#include "sightread/detail/parserutil.hpp"

namespace {
std::string get_with_default(const std::map<std::string, std::string>& map,
                             const std::string& key, std::string default_value)
{
    const auto iter = map.find(key);
    if (iter == map.end()) {
        return default_value;
    }
    return iter->second;
}

SightRead::TempoMap
tempo_map_from_section(const SightRead::Detail::ChartSection& section,
                       int resolution)
{
    std::vector<SightRead::BPM> bpms;
    bpms.reserve(section.bpm_events.size());
    for (const auto& bpm : section.bpm_events) {
        bpms.push_back({.position = SightRead::Tick {bpm.position},
                        .millibeats_per_minute = static_cast<double>(bpm.bpm)});
    }
    std::vector<SightRead::TimeSignature> tses;
    for (const auto& ts : section.ts_events) {
        if (static_cast<std::size_t>(ts.denominator)
            >= (CHAR_BIT * sizeof(int))) {
            throw SightRead::ParseError("Invalid Time Signature denominator");
        }
        tses.push_back({.position = SightRead::Tick {ts.position},
                        .numerator = ts.numerator,
                        .denominator = 1 << ts.denominator});
    }
    return {std::move(tses), std::move(bpms), {}, resolution};
}

std::vector<SightRead::PracticeSection>
practice_sections_from_section(const SightRead::Detail::ChartSection& section)
{
    using namespace std::string_view_literals;

    constexpr std::array practice_section_prefixes {
        R"("section )"sv, R"("section_)"sv, R"("prc_)"sv};
    std::vector<SightRead::PracticeSection> practice_sections;
    for (const auto& event : section.events) {
        std::string_view section_name = event.data;
        if (!section_name.ends_with('"')) {
            continue;
        }
        section_name = section_name.substr(0, section_name.size() - 1);
        for (auto prefix : practice_section_prefixes) {
            if (!section_name.starts_with(prefix)) {
                continue;
            }
            section_name = section_name.substr(prefix.size());
            practice_sections.push_back(
                {.name = std::string {section_name},
                 .start = SightRead::Tick {event.position}});
            break;
        }
    }
    return practice_sections;
}

std::optional<std::tuple<SightRead::Difficulty, SightRead::Instrument>>
diff_inst_from_header(const std::string& header)
{
    using namespace std::literals;

    constexpr std::array<std::tuple<std::string_view, SightRead::Difficulty>, 4>
        DIFFICULTIES {std::tuple {"Easy"sv, SightRead::Difficulty::Easy},
                      {"Medium"sv, SightRead::Difficulty::Medium},
                      {"Hard"sv, SightRead::Difficulty::Hard},
                      {"Expert"sv, SightRead::Difficulty::Expert}};
    constexpr std::array<std::tuple<std::string_view, SightRead::Instrument>,
                         11>
        INSTRUMENTS {std::tuple {"Single"sv, SightRead::Instrument::Guitar},
                     {"DoubleGuitar"sv, SightRead::Instrument::GuitarCoop},
                     {"DoubleBass"sv, SightRead::Instrument::Bass},
                     {"DoubleRhythm"sv, SightRead::Instrument::Rhythm},
                     {"Keyboard"sv, SightRead::Instrument::Keys},
                     {"GHLGuitar"sv, SightRead::Instrument::GHLGuitar},
                     {"GHLBass"sv, SightRead::Instrument::GHLBass},
                     {"GHLRhythm"sv, SightRead::Instrument::GHLRhythm},
                     {"GHLCoop"sv, SightRead::Instrument::GHLGuitarCoop},
                     {"GHLKeys"sv, SightRead::Instrument::GHLKeys},
                     {"Drums"sv, SightRead::Instrument::Drums}};

    // NOLINT is required because following clang-tidy here causes the
    // VS2017 compile to fail.
    const auto diff_iter = std::ranges::find_if( // NOLINT
        DIFFICULTIES, [&](const auto& pair) {
            return header.starts_with(std::get<0>(pair));
        });
    if (diff_iter == std::ranges::end(DIFFICULTIES)) {
        return std::nullopt;
    }

    const auto inst_iter = std::ranges::find_if( // NOLINT
        INSTRUMENTS,
        [&](const auto& pair) { return header.ends_with(std::get<0>(pair)); });
    if (inst_iter == std::ranges::end(INSTRUMENTS)) {
        return std::nullopt;
    }

    return std::tuple {std::get<1>(*diff_iter), std::get<1>(*inst_iter)};
}

std::optional<SightRead::Note>
note_from_colour_key_map(const std::map<int, int>& colour_map, int position,
                         int length, int fret_type, SightRead::NoteFlags flags)
{
    const auto colour_iter = colour_map.find(fret_type);
    if (colour_iter == colour_map.end()) {
        return std::nullopt;
    }
    SightRead::Note note;
    note.position = SightRead::Tick {position};
    note.lengths.at(static_cast<unsigned int>(colour_iter->second))
        = SightRead::Tick {length};
    note.flags = flags;
    return note;
}

bool is_cymbal_colour(int fret_type,
                      SightRead::Detail::DrumTrackType drum_track_type)
{
    constexpr int FOUR_LANE_CYMBAL_THRESHOLD = 64;

    if (drum_track_type == SightRead::Detail::DrumTrackType::FiveLane) {
        return fret_type == 2 || fret_type == 4;
    }
    return fret_type >= FOUR_LANE_CYMBAL_THRESHOLD;
}

std::optional<SightRead::Note>
note_from_note_colour(int position, int length, int fret_type,
                      SightRead::TrackType track_type,
                      SightRead::Detail::DrumTrackType drum_track_type)
{
    std::map<int, int> colours;
    switch (track_type) {
    case SightRead::TrackType::FiveFret:
        colours = {{0, SightRead::FIVE_FRET_GREEN},
                   {1, SightRead::FIVE_FRET_RED},
                   {2, SightRead::FIVE_FRET_YELLOW},
                   {3, SightRead::FIVE_FRET_BLUE},
                   {4, SightRead::FIVE_FRET_ORANGE},
                   {7, SightRead::FIVE_FRET_OPEN}}; // NOLINT
        return note_from_colour_key_map(colours, position, length, fret_type,
                                        SightRead::FLAGS_FIVE_FRET_GUITAR);
    case SightRead::TrackType::SixFret:
        colours = {{0, SightRead::SIX_FRET_WHITE_LOW},
                   {1, SightRead::SIX_FRET_WHITE_MID},
                   {2, SightRead::SIX_FRET_WHITE_HIGH},
                   {3, SightRead::SIX_FRET_BLACK_LOW},
                   {4, SightRead::SIX_FRET_BLACK_MID},
                   {7, SightRead::SIX_FRET_OPEN}, // NOLINT
                   {8, SightRead::SIX_FRET_BLACK_HIGH}}; // NOLINT
        return note_from_colour_key_map(colours, position, length, fret_type,
                                        SightRead::FLAGS_SIX_FRET_GUITAR);
    case SightRead::TrackType::Drums: {
        if (drum_track_type == SightRead::Detail::DrumTrackType::FiveLane) {
            colours = {
                {0, SightRead::DRUM_KICK},
                {1, SightRead::DRUM_RED},
                {2, SightRead::DRUM_YELLOW},
                {3, SightRead::DRUM_BLUE},
                {4, SightRead::DRUM_GREEN},
                {5, SightRead::DRUM_GREEN} // NOLINT
            };
        } else {
            colours = {{0, SightRead::DRUM_KICK},
                       {1, SightRead::DRUM_RED},
                       {2, SightRead::DRUM_YELLOW},
                       {3, SightRead::DRUM_BLUE},
                       {4, SightRead::DRUM_GREEN},
                       {32, SightRead::DRUM_DOUBLE_KICK}, // NOLINT
                       {66, SightRead::DRUM_YELLOW}, // NOLINT
                       {67, SightRead::DRUM_BLUE}, // NOLINT
                       {68, SightRead::DRUM_GREEN}}; // NOLINT
        }
        auto note = note_from_colour_key_map(colours, position, length,
                                             fret_type, SightRead::FLAGS_DRUMS);
        if (note.has_value() && is_cymbal_colour(fret_type, drum_track_type)) {
            note->flags = static_cast<SightRead::NoteFlags>(
                note->flags | SightRead::FLAGS_CYMBAL);
        }
        return note;
    }
    case SightRead::TrackType::FortniteFestival:
        throw std::invalid_argument(
            ".chart files not supported with Fortnite Festival");
    }

    throw std::invalid_argument("Invalid track type");
}

// Turns five lane G tom + G cymbal into B tom + G tom.
void fix_double_greens(std::vector<SightRead::Note>& notes)
{
    std::set<SightRead::Tick> green_tom_positions;
    for (const auto& note : notes) {
        if (note.lengths.at(3) != SightRead::Tick {-1}
            && (note.flags & SightRead::FLAGS_CYMBAL) == 0U) {
            green_tom_positions.insert(note.position);
        }
    }

    for (auto& note : notes) {
        if (note.lengths.at(3) != SightRead::Tick {-1}
            && (note.flags & SightRead::FLAGS_CYMBAL) != 0U
            && green_tom_positions.contains(note.position)) {
            std::swap(note.lengths.at(2), note.lengths.at(3));
            note.flags = static_cast<SightRead::NoteFlags>(
                note.flags & ~SightRead::FLAGS_CYMBAL);
        }
    }
}

std::vector<SightRead::Note>
apply_cymbal_events(const std::vector<SightRead::Note>& notes)
{
    std::map<unsigned int,
             std::vector<std::tuple<SightRead::Tick, SightRead::Tick>>>
        cymbal_markers;
    for (const auto& note : notes) {
        if ((note.flags & SightRead::FLAGS_CYMBAL) == 0U) {
            continue;
        }
        for (auto i = 0U; i < note.lengths.size(); ++i) {
            if (note.lengths.at(i) != SightRead::Tick {-1}) {
                cymbal_markers[i].emplace_back(
                    note.position, note.position + note.lengths.at(i));
            }
        }
    }

    std::vector<SightRead::Note> new_notes;
    for (auto note : notes) {
        if ((note.flags & SightRead::FLAGS_CYMBAL) != 0U) {
            continue;
        }
        for (auto i = 0U; i < note.lengths.size(); ++i) {
            if (note.lengths.at(i) == SightRead::Tick {-1}) {
                continue;
            }

            for (const auto& range : cymbal_markers[i]) {
                if (note.position >= std::get<0>(range)
                    && note.position <= std::get<1>(range)) {
                    note.flags = static_cast<SightRead::NoteFlags>(
                        note.flags | SightRead::FLAGS_CYMBAL);
                }
            }

            new_notes.push_back(note);
        }
    }

    return new_notes;
}

int no_dynamics_lane_colour(const SightRead::Note& note)
{
    if (note.lengths.at(SightRead::DRUM_RED) != SightRead::Tick {-1}) {
        return 0;
    }
    if (note.lengths.at(SightRead::DRUM_YELLOW) != SightRead::Tick {-1}) {
        return 1;
    }
    if (note.lengths.at(SightRead::DRUM_BLUE) != SightRead::Tick {-1}) {
        return 2;
    }
    if (note.lengths.at(SightRead::DRUM_GREEN) != SightRead::Tick {-1}) {
        return 3;
    }
    return -1;
}

std::vector<SightRead::Note> apply_dynamics_events(
    std::vector<SightRead::Note> notes,
    const std::vector<SightRead::Detail::NoteEvent>& note_events)
{
    constexpr int GHOST_BASE = 34;
    constexpr int ACCENT_BASE = 40;
    constexpr int LANE_COUNT = 4;

    std::set<std::tuple<SightRead::Tick, int>> accent_events;
    std::set<std::tuple<SightRead::Tick, int>> ghost_events;

    for (const auto& event : note_events) {
        if (event.fret > ACCENT_BASE + LANE_COUNT || event.fret < GHOST_BASE) {
            continue;
        }
        if (event.fret < GHOST_BASE + LANE_COUNT) {
            accent_events.emplace(SightRead::Tick {event.position},
                                  event.fret - GHOST_BASE);
        }
        if (event.fret >= ACCENT_BASE) {
            ghost_events.emplace(SightRead::Tick {event.position},
                                 event.fret - ACCENT_BASE);
        }
    }
    for (auto& note : notes) {
        if (note.is_kick_note()) {
            continue;
        }
        const auto lane = no_dynamics_lane_colour(note);
        if (accent_events.contains({note.position, lane})) {
            note.flags = static_cast<SightRead::NoteFlags>(
                note.flags | SightRead::FLAGS_ACCENT);
        } else if (ghost_events.contains({note.position, lane})) {
            note.flags = static_cast<SightRead::NoteFlags>(
                note.flags | SightRead::FLAGS_GHOST);
        }
    }
    return notes;
}

class ForcingEvents {
private:
    std::set<int> m_forcing_positions;
    std::set<int> m_tap_positions;

    static bool is_forcing_key(int fret_type, SightRead::TrackType track_type)
    {
        constexpr int HOPO_FORCE_KEY = 5;

        switch (track_type) {
        case SightRead::TrackType::FiveFret:
        case SightRead::TrackType::SixFret:
            return fret_type == HOPO_FORCE_KEY;
        case SightRead::TrackType::Drums:
            return false;
        case SightRead::TrackType::FortniteFestival:
            throw std::invalid_argument(
                ".chart files not supported with Fortnite Festival");
        }

        throw std::invalid_argument("Invalid TrackType");
    }

    static bool is_tap_key(int fret_type, SightRead::TrackType track_type)
    {
        constexpr int TAP_FORCE_KEY = 6;

        switch (track_type) {
        case SightRead::TrackType::FiveFret:
        case SightRead::TrackType::SixFret:
            return fret_type == TAP_FORCE_KEY;
        case SightRead::TrackType::Drums:
            return false;
        case SightRead::TrackType::FortniteFestival:
            throw std::invalid_argument(
                ".chart files not supported with Fortnite Festival");
        }

        throw std::invalid_argument("Invalid TrackType");
    }

public:
    void apply_forcing(std::vector<SightRead::Note>& notes) const
    {
        for (auto& note : notes) {
            if (m_tap_positions.contains(note.position.value())) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags | SightRead::FLAGS_TAP);
            } else if (m_forcing_positions.contains(note.position.value())) {
                note.flags = static_cast<SightRead::NoteFlags>(
                    note.flags | SightRead::FLAGS_FORCE_FLIP);
            }
        }
    }

    void add_force_event(const SightRead::Detail::NoteEvent& event,
                         SightRead::TrackType track_type)
    {
        if (is_forcing_key(event.fret, track_type)) {
            m_forcing_positions.insert(event.position);
        } else if (is_tap_key(event.fret, track_type)) {
            m_tap_positions.insert(event.position);
        }
    }
};

std::vector<SightRead::Note>
apply_drum_events(std::vector<SightRead::Note> notes,
                  const std::vector<SightRead::Detail::NoteEvent>& note_events,
                  SightRead::TrackType track_type,
                  SightRead::Detail::DrumTrackType drum_track_type)
{
    if (track_type != SightRead::TrackType::Drums
        || drum_track_type == SightRead::Detail::DrumTrackType::FourLane) {
        return notes;
    }

    if (drum_track_type == SightRead::Detail::DrumTrackType::FiveLane) {
        fix_double_greens(notes);
    } else {
        notes = apply_cymbal_events(notes);
    }
    return apply_dynamics_events(notes, note_events);
}

bool matches_template(const std::string& data, std::string_view str_template)
{
    if (data.size() < 2) {
        return false;
    }

    auto begin = data.cbegin();
    auto end = data.cend();

    if (*begin == '[' && *std::prev(end) == ']') {
        ++begin;
        --end;
    }

    if (std::distance(begin, end) != static_cast<int>(str_template.size())) {
        return false;
    }

    for (auto i = 0U; i < str_template.size(); ++i) {
        if (str_template.at(i) != '*' && str_template.at(i) != *(begin + i)) {
            return false;
        }
    }

    return true;
}

bool is_event_disco_start(const std::string& data)
{
    return matches_template(data, "mix_*_drums*d");
}

bool is_event_disco_end(const std::string& data)
{
    return matches_template(data, "mix_*_drums*");
}

SightRead::Detail::DrumTrackType
drum_track_type(const SightRead::Metadata& metadata,
                const std::vector<SightRead::Detail::NoteEvent>& note_events)
{
    using SightRead::Detail::DrumTrackType;

    if (metadata.pro_drums) {
        return DrumTrackType::FourLanePro;
    }
    if (metadata.five_lane_drums) {
        return DrumTrackType::FiveLane;
    }

    std::set<int> note_event_keys;
    for (const auto& note : note_events) {
        note_event_keys.insert(note.fret);
    }

    constexpr std::array CYMBAL_KEYS {66, 67, 68};
    if (std::ranges::any_of(CYMBAL_KEYS, [&](const auto key) {
            return note_event_keys.contains(key);
        })) {
        return DrumTrackType::FourLanePro;
    }

    constexpr int FIFTH_LANE_GREEN_KEY = 5;
    if (note_event_keys.contains(FIFTH_LANE_GREEN_KEY)) {
        return DrumTrackType::FiveLane;
    }

    return DrumTrackType::FourLanePro;
}

SightRead::NoteTrack
note_track_from_section(const SightRead::Detail::ChartSection& section,
                        std::shared_ptr<SightRead::SongGlobalData> global_data,
                        SightRead::TrackType track_type,
                        SightRead::SoloParsingBehaviour solo_parsing_behaviour,
                        bool allow_open_chords,
                        const SightRead::Metadata& metadata)
{
    constexpr int DRUM_FILL_KEY = 64;

    ForcingEvents forcing_events;
    std::vector<SightRead::Note> notes;
    const auto drum_type = drum_track_type(metadata, section.note_events);
    const auto resolution = global_data->resolution();
    const auto max_hopo_gap
        = metadata.hopo_threshold.chart_max_hopo_gap(resolution);
    for (const auto& note_event : section.note_events) {
        const auto note
            = note_from_note_colour(note_event.position, note_event.length,
                                    note_event.fret, track_type, drum_type);
        if (note.has_value()) {
            notes.push_back(*note);
        } else {
            forcing_events.add_force_event(note_event, track_type);
        }
    }
    forcing_events.apply_forcing(notes);
    notes
        = apply_drum_events(notes, section.note_events, track_type, drum_type);

    std::vector<SightRead::DrumFill> fills;
    std::vector<SightRead::StarPower> sp;
    for (const auto& phrase : section.special_events) {
        if (phrase.key == 2) {
            sp.push_back(SightRead::StarPower {
                .position = SightRead::Tick {phrase.position},
                .length = SightRead::Tick {phrase.length}});
        } else if (phrase.key == DRUM_FILL_KEY) {
            fills.push_back(SightRead::DrumFill {
                .position = SightRead::Tick {phrase.position},
                .length = SightRead::Tick {phrase.length}});
        }
    }
    if (track_type != SightRead::TrackType::Drums) {
        fills.clear();
        fills.shrink_to_fit();
    }

    std::vector<int> solo_on_events;
    std::vector<int> solo_off_events;
    std::vector<int> disco_flip_on_events;
    std::vector<int> disco_flip_off_events;
    for (const auto& event : section.events) {
        if (event.data == "solo") {
            solo_on_events.push_back(event.position);
        } else if (event.data == "soloend") {
            solo_off_events.push_back(event.position);
        } else if (is_event_disco_start(event.data)) {
            disco_flip_on_events.push_back(event.position);
        } else if (is_event_disco_end(event.data)) {
            disco_flip_off_events.push_back(event.position);
        }
    }
    std::ranges::sort(solo_on_events);
    std::ranges::sort(solo_off_events);
    auto solos = SightRead::Detail::form_solo_vector(
        solo_on_events, solo_off_events, notes, track_type,
        solo_parsing_behaviour, false);
    std::ranges::sort(disco_flip_on_events);
    std::ranges::sort(disco_flip_off_events);
    disco_flip_off_events.push_back(std::numeric_limits<int>::max());
    std::vector<SightRead::DiscoFlip> disco_flips;
    for (auto [start, end] : SightRead::Detail::combine_solo_events(
             disco_flip_on_events, disco_flip_off_events,
             SightRead::SoloParsingBehaviour::PreferEarlierStarts)) {
        disco_flips.push_back({.position = start, .length = end - start});
    }

    SightRead::NoteTrack note_track {std::move(notes), track_type,
                                     std::move(global_data), allow_open_chords,
                                     max_hopo_gap};
    note_track.sp_phrases(std::move(sp));
    note_track.solos(std::move(solos));
    note_track.drum_fills(std::move(fills));
    note_track.disco_flips(disco_flips);
    return note_track;
}

SightRead::TrackType
track_type_from_instrument(SightRead::Instrument instrument)
{
    switch (instrument) {
    case SightRead::Instrument::Guitar:
    case SightRead::Instrument::GuitarCoop:
    case SightRead::Instrument::Bass:
    case SightRead::Instrument::Rhythm:
    case SightRead::Instrument::Keys:
        return SightRead::TrackType::FiveFret;
    case SightRead::Instrument::GHLGuitar:
    case SightRead::Instrument::GHLBass:
    case SightRead::Instrument::GHLRhythm:
    case SightRead::Instrument::GHLGuitarCoop:
    case SightRead::Instrument::GHLKeys:
        return SightRead::TrackType::SixFret;
    case SightRead::Instrument::Drums:
        return SightRead::TrackType::Drums;
    case SightRead::Instrument::FortniteGuitar:
    case SightRead::Instrument::FortniteBass:
    case SightRead::Instrument::FortniteDrums:
    case SightRead::Instrument::FortniteVocals:
    case SightRead::Instrument::FortniteProGuitar:
    case SightRead::Instrument::FortniteProBass:
        throw std::invalid_argument(
            ".chart files not supported with Fortnite Festival");
    }

    throw std::invalid_argument("Invalid instrument");
}
}

SightRead::Detail::ChartConverter::ChartConverter(SightRead::Metadata metadata)
    : m_metadata {std::move(metadata)}
    , m_permitted_instruments {SightRead::all_instruments()}
    , m_solo_parsing_behaviour {SightRead::SoloParsingBehaviour::
                                    PreferLaterStarts}
    , m_allow_open_chords {true}
{
}

SightRead::Detail::ChartConverter&
SightRead::Detail::ChartConverter::permit_instruments(
    std::set<SightRead::Instrument> permitted_instruments)
{
    m_permitted_instruments = std::move(permitted_instruments);
    return *this;
}

SightRead::Detail::ChartConverter&
SightRead::Detail::ChartConverter::solo_parsing_behaviour(
    SightRead::SoloParsingBehaviour behaviour)
{
    m_solo_parsing_behaviour = behaviour;
    return *this;
}

SightRead::Detail::ChartConverter&
SightRead::Detail::ChartConverter::allow_open_chords(bool allow_open_chords)
{
    m_allow_open_chords = allow_open_chords;
    return *this;
}

SightRead::Song SightRead::Detail::ChartConverter::convert(
    const SightRead::Detail::Chart& chart) const
{
    SightRead::Song song;

    song.global_data().is_from_midi(false);
    song.global_data().name(m_metadata.name);
    song.global_data().artist(m_metadata.artist);
    song.global_data().charter(m_metadata.charter);

    for (const auto& section : chart.sections) {
        if (section.name == "Song") {
            try {
                const auto resolution = std::stoi(get_with_default(
                    section.key_value_pairs, "Resolution", "192"));
                song.global_data().resolution(resolution);
            } catch (const std::invalid_argument&) { // NOLINT
                // CH just ignores this kind of parsing mistake.
                // TODO: Use from_chars instead to avoid having to use
                // exceptions as control flow.
            }
        } else if (section.name == "SyncTrack") {
            song.global_data().tempo_map(tempo_map_from_section(
                section, song.global_data().resolution()));
        } else if (section.name == "Events") {
            song.global_data().practice_sections(
                practice_sections_from_section(section));
        } else {
            auto pair = diff_inst_from_header(section.name);
            if (!pair.has_value()) {
                continue;
            }
            auto [diff, inst] = *pair;
            if (!m_permitted_instruments.contains(inst)) {
                continue;
            }
            auto note_track = note_track_from_section(
                section, song.global_data_ptr(),
                track_type_from_instrument(inst), m_solo_parsing_behaviour,
                m_allow_open_chords, m_metadata);
            song.add_note_track(inst, diff, std::move(note_track));
        }
    }

    if (song.instruments().empty()) {
        throw SightRead::ParseError("Chart has no notes");
    }

    return song;
}
