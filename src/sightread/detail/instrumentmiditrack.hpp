#ifndef SIGHTREAD_DETAIL_INSTRUMENTMIDITRACK_HPP
#define SIGHTREAD_DETAIL_INSTRUMENTMIDITRACK_HPP

#include <cstdint>
#include <map>
#include <vector>

#include "sightread/detail/intervalset.hpp"
#include "sightread/metadata.hpp"
#include "sightread/songparts.hpp"

namespace SightRead::Detail {
struct NoteToggleEvent {
    int position;
    int velocity;
    int rank;
};

struct NoteEvent {
    int position;
    int length;
    std::uint8_t velocity;
};

struct MidiEventPosition {
    int tick_position;
    int order_position;
};

class NoteOnOffEvents {
private:
    std::vector<NoteToggleEvent> m_note_on_events;
    std::vector<NoteToggleEvent> m_note_off_events;
    int m_last_rank = 0;

public:
    void add_note_on_event(int position, std::uint8_t velocity);
    void add_note_off_event(int position, std::uint8_t velocity);

    [[nodiscard]] std::size_t on_event_count() const;

    // Like combine_solo_events, but never skips on events to suit Midi parsing
    // and checks if there is an unmatched on event.
    //
    // expand_length_zero_events is because some drum events have the length
    // increased by 1 if the start and end are at the same time.
    [[nodiscard]] std::vector<NoteEvent>
    combined_events(bool expand_length_zero_events = false) const;
    [[nodiscard]] std::vector<SightRead::Solo>
    track_solos(const std::vector<SightRead::Note>& notes,
                SightRead::TrackType track_type) const;

    [[nodiscard]] HalfOpenIntervalSet<int> interval_set() const;
};

enum class DrumTrackType : std::uint8_t { FourLane, FourLanePro, FiveLane };

class InstrumentMidiTrack {
private:
    static constexpr std::uint8_t SOLO_KEY = 103;
    static constexpr std::uint8_t SP_KEY = 116;

    [[nodiscard]] bool should_use_solos_for_sp() const;
    [[nodiscard]] NoteOnOffEvents solo_events() const;
    [[nodiscard]] NoteOnOffEvents sp_events() const;

    [[nodiscard]] bool has_tom_markers() const;

public:
    std::map<std::uint8_t, NoteOnOffEvents> note_events;
    std::map<SightRead::Difficulty, std::vector<MidiEventPosition>>
        open_on_events;
    std::map<SightRead::Difficulty, std::vector<MidiEventPosition>>
        open_off_events;
    std::map<SightRead::Difficulty, std::vector<MidiEventPosition>>
        tap_on_sysex_events;
    std::map<SightRead::Difficulty, std::vector<MidiEventPosition>>
        tap_off_sysex_events;
    std::map<SightRead::Difficulty, std::vector<MidiEventPosition>>
        disco_flip_on_events;
    std::map<SightRead::Difficulty, std::vector<MidiEventPosition>>
        disco_flip_off_events;

    InstrumentMidiTrack() = default;

    [[nodiscard]] NoteOnOffEvents events_with_key(std::uint8_t key) const;
    [[nodiscard]] std::vector<SightRead::StarPower> sp_phrases() const;
    [[nodiscard]] std::vector<SightRead::Solo>
    solos(const std::vector<SightRead::Note>& notes,
          SightRead::TrackType track_type, bool permit_solos) const;

    void add_note_off_event(std::uint8_t key, std::uint8_t velocity, int time);
    void add_note_on_event(std::uint8_t key, std::uint8_t velocity, int time);

    [[nodiscard]] DrumTrackType
    drum_track_type(const SightRead::Metadata& metadata) const;
};
}

#endif
