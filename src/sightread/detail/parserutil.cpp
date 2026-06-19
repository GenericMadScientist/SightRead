#include <algorithm>
#include <array>
#include <set>

#include "sightread/detail/parserutil.hpp"

bool SightRead::Detail::is_six_fret_instrument(SightRead::Instrument instrument)
{
    constexpr std::array SIX_FRET_INSTRUMENTS {
        SightRead::Instrument::GHLGuitar, SightRead::Instrument::GHLBass,
        SightRead::Instrument::GHLRhythm, SightRead::Instrument::GHLGuitarCoop,
        SightRead::Instrument::GHLKeys};
    return std::ranges::find(SIX_FRET_INSTRUMENTS, instrument)
        != std::ranges::end(SIX_FRET_INSTRUMENTS);
}

std::vector<std::tuple<SightRead::Tick, SightRead::Tick>>
SightRead::Detail::combine_solo_events(
    const std::vector<int>& on_events, const std::vector<int>& off_events,
    SightRead::SoloParsingBehaviour solo_parsing_behaviour)
{
    std::vector<std::tuple<SightRead::Tick, SightRead::Tick>> ranges;

    if (solo_parsing_behaviour
        == SightRead::SoloParsingBehaviour::PreferLaterStarts) {
        std::set<int> on_event_set {on_events.cbegin(), on_events.cend()};
        for (auto off : off_events) {
            auto matching_on = on_event_set.upper_bound(off);
            if (matching_on == on_event_set.begin()) {
                continue;
            }
            --matching_on;
            ranges.emplace_back(*matching_on, off);
            on_event_set.erase(matching_on);
        }
    } else {
        auto on_iter = on_events.cbegin();
        auto off_iter = off_events.cbegin();

        while (on_iter < on_events.cend() && off_iter < off_events.cend()) {
            if (*on_iter >= *off_iter) {
                ++off_iter;
                continue;
            }
            ranges.emplace_back(*on_iter, *off_iter);
            on_iter
                = std::find_if(on_iter, on_events.cend(),
                               [=](const auto on) { return on >= *off_iter; });
        }
    }

    return ranges;
}

std::vector<SightRead::Solo> SightRead::Detail::form_solo_vector(
    const std::vector<int>& solo_on_events,
    const std::vector<int>& solo_off_events,
    const std::vector<SightRead::Note>& notes, SightRead::TrackType track_type,
    SightRead::SoloParsingBehaviour solo_parsing_behaviour, bool is_midi)
{
    constexpr int SOLO_NOTE_VALUE = 100;

    if (solo_parsing_behaviour == SightRead::SoloParsingBehaviour::NoSolos) {
        return {};
    }

    auto ranges = combine_solo_events(solo_on_events, solo_off_events,
                                      solo_parsing_behaviour);
    if (!is_midi) {
        for (auto& [start, end] : ranges) {
            end += SightRead::Tick {1};
        }
    }
    std::ranges::stable_sort(ranges);
    for (auto i = 0U; i + 1 < ranges.size(); ++i) {
        auto& range = ranges.at(i);
        auto& next_range = ranges.at(i + 1);
        std::get<1>(next_range)
            = std::max(std::get<1>(next_range), std::get<1>(range));
        std::get<1>(range)
            = std::min(std::get<1>(range), std::get<0>(next_range));
    }

    std::vector<SightRead::Solo> solos;
    for (auto [start, end] : ranges) {
        std::set<SightRead::Tick> positions_in_solo;
        auto note_count = 0;
        for (const auto& note : notes) {
            if (note.position >= start && note.position < end) {
                positions_in_solo.insert(note.position);
                ++note_count;
            }
        }
        if (positions_in_solo.empty()) {
            continue;
        }
        if (track_type != SightRead::TrackType::Drums) {
            note_count = static_cast<int>(positions_in_solo.size());
        }
        solos.push_back({.start = start,
                         .end = end,
                         .value = SOLO_NOTE_VALUE * note_count});
    }

    return solos;
}
