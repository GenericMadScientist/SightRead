#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <tuple>

#include "sightread/songparts.hpp"

namespace {
SightRead::Note
combined_note(std::vector<SightRead::Note>::const_iterator begin,
              std::vector<SightRead::Note>::const_iterator end)
{
    SightRead::Note note = *begin;
    ++begin;
    while (begin < end) {
        for (auto i = 0U; i < note.lengths.size(); ++i) {
            const auto new_length = begin->lengths.at(i);
            if (new_length != SightRead::Tick {-1}) {
                note.lengths.at(i) = new_length;
            }
        }
        ++begin;
    }
    return note;
}

bool is_chord(const SightRead::Note& note)
{
    auto count = 0;
    for (auto length : note.lengths) {
        if (length != SightRead::Tick {-1}) {
            ++count;
        }
    }
    return count >= 2;
}
}

namespace SightRead {
std::set<Instrument> all_instruments()
{
    return {SightRead::Instrument::Guitar,
            SightRead::Instrument::GuitarCoop,
            SightRead::Instrument::Bass,
            SightRead::Instrument::Rhythm,
            SightRead::Instrument::Keys,
            SightRead::Instrument::GHLGuitar,
            SightRead::Instrument::GHLBass,
            SightRead::Instrument::GHLRhythm,
            SightRead::Instrument::GHLGuitarCoop,
            SightRead::Instrument::Drums,
            SightRead::Instrument::FortniteGuitar,
            SightRead::Instrument::FortniteBass,
            SightRead::Instrument::FortniteDrums,
            SightRead::Instrument::FortniteVocals,
            SightRead::Instrument::FortniteProGuitar,
            SightRead::Instrument::FortniteProBass};
}
}

int SightRead::Note::open_index() const
{
    if ((flags & FLAGS_FIVE_FRET_GUITAR) != 0U) {
        return FIVE_FRET_OPEN;
    }
    if ((flags & FLAGS_SIX_FRET_GUITAR) != 0U) {
        return SIX_FRET_OPEN;
    }
    return -1;
}

int SightRead::Note::colours() const
{
    auto colour_flags = 0;
    for (auto i = 0U; i < lengths.size(); ++i) {
        if (lengths.at(i) != SightRead::Tick {-1}) {
            colour_flags |= 1 << i;
        }
    }
    return colour_flags;
}

void SightRead::Note::merge_non_opens_into_open()
{
    const auto index = open_index();
    if (index == -1) {
        return;
    }
    const auto open_length = lengths.at(static_cast<unsigned int>(index));
    if (open_length == SightRead::Tick {-1}) {
        return;
    }
    for (auto i = 0U; i < lengths.size(); ++i) {
        if (static_cast<int>(i) != index && lengths.at(i) == open_length) {
            lengths.at(i) = SightRead::Tick {-1};
        }
    }
}

void SightRead::Note::disable_cymbals()
{
    flags = static_cast<NoteFlags>(flags & ~FLAGS_CYMBAL);
}

void SightRead::Note::disable_dynamics()
{
    flags = static_cast<NoteFlags>(flags & ~(FLAGS_GHOST | FLAGS_ACCENT));
}

bool SightRead::Note::is_kick_note() const
{
    return ((flags & FLAGS_DRUMS) != 0U)
        && (lengths.at(DRUM_KICK) != SightRead::Tick {-1}
            || lengths.at(DRUM_DOUBLE_KICK) != SightRead::Tick {-1});
}

bool SightRead::Note::is_skipped_kick(
    const SightRead::DrumSettings& settings) const
{
    if (!is_kick_note()) {
        return false;
    }
    if (lengths.at(DRUM_KICK) != SightRead::Tick {-1}) {
        return settings.disable_kick;
    }
    return !settings.enable_double_kick;
}

void SightRead::NoteTrack::compute_base_score_ticks()
{
    constexpr int BASE_SUSTAIN_DENSITY = 25;

    SightRead::Tick total_ticks {0};
    for (const auto& note : m_notes) {
        std::vector<SightRead::Tick> constituent_lengths;
        for (auto length : note.lengths) {
            if (length != SightRead::Tick {-1}) {
                constituent_lengths.push_back(length);
            }
        }
        std::ranges::sort(constituent_lengths);
        if (constituent_lengths.front() == constituent_lengths.back()) {
            total_ticks += constituent_lengths.front();
        } else {
            for (auto length : constituent_lengths) {
                total_ticks += length;
            }
        }
    }

    const auto resolution = m_global_data->resolution();
    m_base_score_ticks
        = (total_ticks.value() * BASE_SUSTAIN_DENSITY + resolution - 1)
        / resolution;
}

void SightRead::NoteTrack::merge_same_time_notes()
{
    if (m_track_type == TrackType::Drums
        || m_track_type == TrackType::FortniteFestival) {
        return;
    }

    std::vector<Note> notes;
    for (auto p = m_notes.cbegin(); p < m_notes.cend();) {
        auto q = p;
        while (q < m_notes.cend() && p->position == q->position) {
            ++q;
        }
        notes.push_back(combined_note(p, q));
        p = q;
    }
    m_notes = std::move(notes);
}

void SightRead::NoteTrack::add_hopos(SightRead::Tick max_hopo_gap)
{
    if (m_track_type == TrackType::Drums) {
        return;
    }

    for (auto i = 0U; i < m_notes.size(); ++i) {
        if ((m_notes.at(i).flags & (FLAGS_TAP | FLAGS_FORCE_STRUM)) != 0U) {
            continue;
        }
        bool is_hopo = (m_notes.at(i).flags & FLAGS_FORCE_FLIP) != 0U;
        if (i != 0U) {
            const auto note_gap
                = m_notes.at(i).position - m_notes.at(i - 1).position;
            if (!is_chord(m_notes.at(i))
                && m_notes.at(i).colours() != m_notes.at(i - 1).colours()
                && (((m_notes.at(i).colours() & m_notes.at(i - 1).colours())
                     == 0)
                    || !m_global_data->is_from_midi())
                && note_gap <= max_hopo_gap) {
                is_hopo = !is_hopo;
            }
        }
        if ((m_notes.at(i).flags & FLAGS_FORCE_HOPO) != 0U) {
            is_hopo = true;
        }
        if (is_hopo) {
            m_notes.at(i).flags
                = static_cast<NoteFlags>(m_notes.at(i).flags | FLAGS_HOPO);
        }
    }
}

SightRead::NoteTrack::NoteTrack(std::vector<Note> notes,
                                const std::vector<StarPower>& sp_phrases,
                                TrackType track_type,
                                std::shared_ptr<SongGlobalData> global_data,
                                bool allow_open_chords,
                                SightRead::Tick max_hopo_gap)
    : m_track_type {track_type}
    , m_global_data {std::move(global_data)}
    , m_base_score_ticks {0}
{
    if (m_global_data == nullptr) {
        throw std::runtime_error("Non-null global data required");
    }

    std::ranges::stable_sort(notes, {},
                             [](const auto& x) { return x.position; });

    if (!notes.empty()) {
        auto prev_note = notes.cbegin();
        for (auto p = notes.cbegin() + 1; p < notes.cend(); ++p) {
            if (p->position != prev_note->position
                || p->colours() != prev_note->colours()) {
                m_notes.push_back(*prev_note);
            }
            prev_note = p;
        }
        m_notes.push_back(*prev_note);
    }

    std::vector<SightRead::Tick> sp_starts;
    std::vector<SightRead::Tick> sp_ends;
    sp_starts.reserve(sp_phrases.size());
    sp_ends.reserve(sp_phrases.size());

    for (const auto& phrase : sp_phrases) {
        sp_starts.push_back(phrase.position);
        sp_ends.push_back(phrase.position + phrase.length);
    }

    std::ranges::sort(sp_starts);
    std::ranges::sort(sp_ends);

    m_sp_phrases.reserve(sp_phrases.size());
    for (auto i = 0U; i < sp_phrases.size(); ++i) {
        auto start = sp_starts.at(i);
        if (i > 0) {
            start = std::max(sp_starts.at(i), sp_ends.at(i - 1));
        }
        const auto length = sp_ends.at(i) - start;
        m_sp_phrases.push_back({.position = start, .length = length});
    }

    merge_same_time_notes();
    compute_base_score_ticks();

    // We handle open note merging at the end because in v23 the removed
    // notes still affect the base score.
    if (!allow_open_chords) {
        for (auto& note : m_notes) {
            note.merge_non_opens_into_open();
        }
    }

    add_hopos(max_hopo_gap);
}

void SightRead::NoteTrack::generate_drum_fills(
    const SightRead::TempoMap& tempo_map)
{
    const SightRead::Second FILL_DELAY {0.25};
    const SightRead::Measure FILL_GAP {4.0};

    if (m_notes.empty()) {
        return;
    }

    std::vector<std::tuple<SightRead::Second, SightRead::Tick>> note_times;
    for (const auto& n : m_notes) {
        const auto seconds = tempo_map.to_seconds(n.position);
        note_times.emplace_back(seconds, n.position);
    }
    const auto final_note_s = tempo_map.to_seconds(m_notes.back().position);
    const auto measure_bound = tempo_map.to_measures(final_note_s + FILL_DELAY);
    SightRead::Measure m {1.0};
    while (m <= measure_bound) {
        const auto fill_seconds = tempo_map.to_seconds(m);
        const auto measure_ticks = tempo_map.to_ticks(tempo_map.to_beats(m));
        bool exists_close_note = false;
        SightRead::Tick close_note_position {0};
        for (const auto& [s, pos] : note_times) {
            const auto s_diff = s - fill_seconds;
            if (s_diff > FILL_DELAY) {
                break;
            }
            if (s_diff + FILL_DELAY < SightRead::Second {0}) {
                continue;
            }
            if (!exists_close_note) {
                exists_close_note = true;
                close_note_position = pos;
            } else if (std::abs((measure_ticks - pos).value()) <= std::abs(
                           (measure_ticks - close_note_position).value())) {
                close_note_position = pos;
            }
        }
        if (!exists_close_note) {
            m += SightRead::Measure(1.0);
            continue;
        }
        const auto m_seconds = tempo_map.to_seconds(m);
        const auto prev_m_seconds
            = tempo_map.to_seconds(m - SightRead::Measure(1.0));
        const auto mid_m_seconds = (m_seconds + prev_m_seconds) * 0.5;
        const auto fill_start = tempo_map.to_ticks(mid_m_seconds);
        m_drum_fills.push_back(DrumFill {.position = fill_start,
                                         .length = measure_ticks - fill_start});
        m += FILL_GAP;
    }
}

void SightRead::NoteTrack::disable_cymbals()
{
    for (auto& n : m_notes) {
        n.disable_cymbals();
    }
}

void SightRead::NoteTrack::disable_dynamics()
{
    for (auto& n : m_notes) {
        n.disable_dynamics();
    }
}

std::vector<SightRead::Solo>
SightRead::NoteTrack::solos(const SightRead::DrumSettings& drum_settings) const
{
    constexpr int SOLO_NOTE_VALUE = 100;

    if (m_track_type != TrackType::Drums) {
        return m_solos;
    }
    auto solos = m_solos;
    auto p = m_notes.cbegin();
    auto q = solos.begin();
    while (p < m_notes.cend() && q < solos.end()) {
        if (p->position < q->start) {
            ++p;
            continue;
        }
        if (p->position > q->end) {
            ++q;
            continue;
        }
        if (p->is_skipped_kick(drum_settings)) {
            q->value -= SOLO_NOTE_VALUE;
        }
        ++p;
    }
    std::erase_if(solos, [](const auto& solo) { return solo.value == 0; });
    return solos;
}

void SightRead::NoteTrack::solos(std::vector<Solo> solos)
{
    std::ranges::stable_sort(solos, {}, [](const auto& x) { return x.start; });
    m_solos = std::move(solos);
}

int SightRead::NoteTrack::base_score(
    SightRead::DrumSettings drum_settings) const
{
    constexpr int BASE_NOTE_VALUE = 50;

    auto note_count = 0;
    for (const auto& note : m_notes) {
        if (note.is_skipped_kick(drum_settings)) {
            continue;
        }
        for (auto l : note.lengths) {
            if (l != SightRead::Tick {-1}) {
                ++note_count;
            }
        }
    }

    return BASE_NOTE_VALUE * note_count + m_base_score_ticks;
}

SightRead::NoteTrack
SightRead::NoteTrack::snap_chords(SightRead::Tick snap_gap) const
{
    auto new_track = *this;
    auto& new_notes = new_track.m_notes;
    for (auto it = new_notes.begin(); std::next(it) < new_notes.end(); ++it) {
        auto next_note = std::next(it);
        if (next_note->position - it->position <= snap_gap) {
            next_note->position = it->position;
        }
    }
    new_track.merge_same_time_notes();
    return new_track;
}

void SightRead::NoteTrack::disco_flips(std::vector<DiscoFlip> disco_flips)
{
    m_disco_flips = std::move(disco_flips);

    for (auto& note : m_notes) {
        if ((note.flags & SightRead::FLAGS_DRUMS) == 0) {
            continue;
        }
        if (std::ranges::none_of(m_disco_flips, [&](const auto& flip) {
                return (flip.position <= note.position)
                    && (flip.position + flip.length >= note.position);
            })) {
            continue;
        }

        if ((note.lengths.at(SightRead::DRUM_RED) != SightRead::Tick {-1})
            || (note.lengths.at(SightRead::DRUM_YELLOW)
                != SightRead::Tick {-1})) {
            note.flags = static_cast<SightRead::NoteFlags>(
                note.flags | SightRead::FLAGS_DISCO);
        }
    }
}

void SightRead::NoteTrack::apply_disco_flips()
{
    for (auto& note : m_notes) {
        if ((note.flags & SightRead::FLAGS_DRUMS) == 0) {
            continue;
        }
        if (std::ranges::none_of(m_disco_flips, [&](const auto& flip) {
                return (flip.position <= note.position)
                    && (flip.position + flip.length >= note.position);
            })) {
            continue;
        }
        if (std::ranges::binary_search(m_prohibited_disco_flip_positions,
                                       note.position)) {
            continue;
        }

        if (note.lengths.at(SightRead::DRUM_RED) != SightRead::Tick {-1}) {
            note.lengths.at(SightRead::DRUM_RED) = SightRead::Tick {-1};
            note.lengths.at(SightRead::DRUM_YELLOW) = SightRead::Tick {0};
            note.flags = static_cast<SightRead::NoteFlags>(
                note.flags | SightRead::FLAGS_CYMBAL);
        } else if (note.lengths.at(SightRead::DRUM_YELLOW)
                   != SightRead::Tick {-1}) {
            note.lengths.at(SightRead::DRUM_RED) = SightRead::Tick {0};
            note.lengths.at(SightRead::DRUM_YELLOW) = SightRead::Tick {-1};
            note.flags = static_cast<SightRead::NoteFlags>(
                note.flags & ~SightRead::FLAGS_CYMBAL);
        }
        note.flags = static_cast<SightRead::NoteFlags>(
            note.flags & ~SightRead::FLAGS_DISCO);
    }

    m_disco_flips.clear();
}

void SightRead::NoteTrack::apply_flam_markers()
{
    constexpr auto FLAM_SWAP_LOOKUP
        = std::array {std::tuple {SightRead::DRUM_RED, SightRead::DRUM_YELLOW},
                      std::tuple {SightRead::DRUM_YELLOW, SightRead::DRUM_BLUE},
                      std::tuple {SightRead::DRUM_BLUE, SightRead::DRUM_GREEN},
                      std::tuple {SightRead::DRUM_GREEN, SightRead::DRUM_BLUE}};

    std::vector<SightRead::Note> new_notes;
    auto next_pos_it = m_notes.begin();
    for (auto it = m_notes.begin(); it < m_notes.end(); it = next_pos_it) {
        if (((it->flags & SightRead::FLAGS_DRUMS) == 0) || it->is_kick_note()) {
            next_pos_it = std::next(it);
            continue;
        }
        next_pos_it
            = std::find_if_not(it, m_notes.end(), [=](const auto& note) {
                  return note.position == it->position;
              });
        if (std::count_if(it, next_pos_it,
                          [](const auto& note) { return !note.is_kick_note(); })
            > 1) {
            continue;
        }
        if (std::ranges::none_of(m_flam_markers, [=](const auto& flam) {
                return (flam.position <= it->position)
                    && (flam.position + flam.length >= it->position);
            })) {
            continue;
        }

        auto new_note = *it;
        for (const auto& [orig_col, new_col] : FLAM_SWAP_LOOKUP) {
            if (new_note.lengths.at(orig_col) != SightRead::Tick {-1}) {
                std::swap(new_note.lengths.at(orig_col),
                          new_note.lengths.at(new_col));
                break;
            }
        }
        new_notes.push_back(new_note);

        if (((it->flags & SightRead::FLAGS_CYMBAL) == 0)
            && (it->colours() == (1 << SightRead::DRUM_BLUE))) {
            std::swap(it->lengths.at(SightRead::DRUM_BLUE),
                      it->lengths.at(SightRead::DRUM_YELLOW));
            m_prohibited_disco_flip_positions.push_back(it->position);
        }
    }

    for (auto note : new_notes) {
        m_notes.push_back(note);
    }

    std::ranges::sort(m_notes, [](const auto& x, const auto& y) {
        return std::tuple {x.position, x.colours()}
        < std::tuple {y.position, y.colours()};
    });
    std::ranges::sort(m_prohibited_disco_flip_positions);
}
