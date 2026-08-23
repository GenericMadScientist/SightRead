#include <stack>

#include "sightread/detail/instrumentmiditrack.hpp"
#include "sightread/detail/parserutil.hpp"

namespace SightRead::Detail {
void NoteOnOffEvents::add_note_on_event(int position, std::uint8_t velocity)
{
    m_note_on_events.emplace_back(position, velocity, ++m_last_rank);
}

void NoteOnOffEvents::add_note_off_event(int position, std::uint8_t velocity)
{
    m_note_off_events.emplace_back(position, velocity, ++m_last_rank);
}

std::size_t NoteOnOffEvents::on_event_count() const
{
    return m_note_on_events.size();
}

std::vector<NoteEvent>
NoteOnOffEvents::combined_events(bool expand_length_zero_events) const
{
    std::vector<NoteEvent> notes;
    std::stack<NoteToggleEvent, std::vector<NoteToggleEvent>>
        unmatched_on_events;

    auto on_iter = m_note_on_events.cbegin();
    for (auto off_event : m_note_off_events) {
        for (; on_iter < m_note_on_events.cend()
             && on_iter->rank < off_event.rank;
             ++on_iter) {
            unmatched_on_events.push(*on_iter);
        }

        if (unmatched_on_events.empty()) {
            continue;
        }

        const auto start = unmatched_on_events.top().position;
        const auto velocity = unmatched_on_events.top().velocity;
        unmatched_on_events.pop();
        auto end = off_event.position;
        if (start == end && expand_length_zero_events) {
            ++end;
        }
        notes.emplace_back(start, end - start, velocity);
    }

    return notes;
}

std::vector<SightRead::Solo>
NoteOnOffEvents::track_solos(const std::vector<SightRead::Note>& notes,
                             SightRead::TrackType track_type) const
{
    std::vector<int> solo_ons;
    std::vector<int> solo_offs;
    solo_ons.reserve(m_note_on_events.size());
    for (const auto& event : m_note_on_events) {
        solo_ons.push_back(event.position);
    }
    solo_offs.reserve(m_note_off_events.size());
    for (const auto& event : m_note_off_events) {
        solo_offs.push_back(event.position);
    }

    return SightRead::Detail::form_solo_vector(
        solo_ons, solo_offs, notes, track_type,
        SightRead::SoloParsingBehaviour::PreferEarlierStarts, true);
}

HalfOpenIntervalSet<int> NoteOnOffEvents::interval_set() const
{
    std::vector<std::tuple<int, int>> intervals;
    for (auto event : combined_events(true)) {
        intervals.emplace_back(event.position, event.position + event.length);
    }

    return {std::move(intervals)};
}

bool InstrumentMidiTrack::should_use_solos_for_sp() const
{
    const auto solo_iter = note_events.find(SOLO_KEY);
    if (solo_iter == note_events.cend()
        || solo_iter->second.on_event_count() <= 1) {
        return false;
    }

    const auto sp_iter = note_events.find(SP_KEY);
    return sp_iter == note_events.cend()
        || sp_iter->second.on_event_count() == 0;
}

NoteOnOffEvents InstrumentMidiTrack::solo_events() const
{
    if (should_use_solos_for_sp()) {
        return {};
    }
    return events_with_key(SOLO_KEY);
}

NoteOnOffEvents InstrumentMidiTrack::sp_events() const
{
    if (should_use_solos_for_sp()) {
        return events_with_key(SOLO_KEY);
    }
    return events_with_key(SP_KEY);
}

NoteOnOffEvents InstrumentMidiTrack::events_with_key(std::uint8_t key) const
{
    const auto iter = note_events.find(key);
    if (iter == note_events.cend()) {
        return {};
    }
    return iter->second;
}

std::vector<SightRead::StarPower> InstrumentMidiTrack::sp_phrases() const
{
    const auto combined_events = sp_events().combined_events();

    std::vector<SightRead::StarPower> sp_phrases;
    sp_phrases.reserve(combined_events.size());
    for (const auto& event : combined_events) {
        sp_phrases.push_back({.position = SightRead::Tick {event.position},
                              .length = SightRead::Tick {event.length}});
    }

    return sp_phrases;
}

std::vector<SightRead::Solo>
InstrumentMidiTrack::solos(const std::vector<SightRead::Note>& notes,
                           SightRead::TrackType track_type,
                           bool permit_solos) const
{
    if (!permit_solos) {
        return {};
    }

    return solo_events().track_solos(notes, track_type);
}

void InstrumentMidiTrack::add_note_off_event(std::uint8_t key,
                                             std::uint8_t velocity, int time)
{
    note_events[key].add_note_off_event(time, velocity);
}

void InstrumentMidiTrack::add_note_on_event(std::uint8_t key,
                                            std::uint8_t velocity, int time)
{
    // Velocity 0 Note On events are counted as Note Off events.
    if (velocity == 0) {
        note_events[key].add_note_off_event(time, velocity);
    } else {
        note_events[key].add_note_on_event(time, velocity);
    }
}
}
