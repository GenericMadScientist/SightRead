#include <ostream>

#include <boost/test/unit_test.hpp>

#include "sightread/detail/instrumentmiditrack.hpp"

namespace SightRead::Detail {
inline std::ostream& operator<<(std::ostream& stream, DrumTrackType type)
{
    stream << "DrumTrackType " << static_cast<int>(type);
    return stream;
}
}

BOOST_AUTO_TEST_SUITE(drum_track_type)

BOOST_AUTO_TEST_CASE(is_four_lane_pro_when_pro_drums_set)
{
    SightRead::Metadata metadata;
    metadata.pro_drums = true;

    SightRead::Detail::InstrumentMidiTrack track {false};

    BOOST_CHECK_EQUAL(track.drum_track_type(metadata),
                      SightRead::Detail::DrumTrackType::FourLanePro);
}

BOOST_AUTO_TEST_CASE(is_five_lane_when_five_lane_drums_set)
{
    SightRead::Metadata metadata;
    metadata.five_lane_drums = true;

    SightRead::Detail::InstrumentMidiTrack track {false};

    BOOST_CHECK_EQUAL(track.drum_track_type(metadata),
                      SightRead::Detail::DrumTrackType::FiveLane);
}

BOOST_AUTO_TEST_CASE(pro_drums_overrides_five_lane_drums)
{
    SightRead::Metadata metadata;
    metadata.five_lane_drums = true;
    metadata.pro_drums = true;

    SightRead::Detail::InstrumentMidiTrack track {false};

    BOOST_CHECK_EQUAL(track.drum_track_type(metadata),
                      SightRead::Detail::DrumTrackType::FourLanePro);
}

BOOST_AUTO_TEST_CASE(is_four_lane_pro_in_presence_of_tom_markers)
{
    SightRead::Detail::InstrumentMidiTrack track {false};
    track.add_note_on_event(110, 127, 0, 0);
    track.add_note_off_event(110, 0, 0, 480);

    BOOST_CHECK_EQUAL(track.drum_track_type({}),
                      SightRead::Detail::DrumTrackType::FourLanePro);
}

BOOST_AUTO_TEST_CASE(is_five_lane_in_presence_of_fifth_lane_greens)
{
    SightRead::Detail::InstrumentMidiTrack track {false};
    track.add_note_on_event(101, 127, 0, 0);
    track.add_note_off_event(101, 0, 0, 480);

    BOOST_CHECK_EQUAL(track.drum_track_type({}),
                      SightRead::Detail::DrumTrackType::FiveLane);
}

BOOST_AUTO_TEST_CASE(defaults_to_four_lane_non_pro)
{
    SightRead::Detail::InstrumentMidiTrack track {false};

    BOOST_CHECK_EQUAL(track.drum_track_type({}),
                      SightRead::Detail::DrumTrackType::FourLane);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(combined_events)

BOOST_AUTO_TEST_CASE(handles_overlapping_notes_when_note_on_cancel_is_false)
{
    SightRead::Detail::NoteOnOffEvents events {false};
    events.add_note_on_event(0, 127, 0);
    events.add_note_on_event(192, 127, 0);
    events.add_note_off_event(384, 0, 0);
    events.add_note_off_event(576, 0, 0);

    const auto notes = events.combined_events();

    BOOST_CHECK_EQUAL(notes.size(), 2U);
}

BOOST_AUTO_TEST_CASE(handles_overlapping_notes_when_note_on_cancel_is_true)
{
    SightRead::Detail::NoteOnOffEvents events {true};
    events.add_note_on_event(0, 127, 0);
    events.add_note_on_event(192, 127, 0);
    events.add_note_off_event(384, 0, 0);
    events.add_note_off_event(576, 0, 0);

    const auto notes = events.combined_events();

    BOOST_CHECK_EQUAL(notes.size(), 1U);
}

BOOST_AUTO_TEST_CASE(handles_simultaneous_on_off_events_when_cancel_is_true)
{
    SightRead::Detail::NoteOnOffEvents events {true};
    events.add_note_on_event(0, 127, 0);
    events.add_note_on_event(192, 127, 0);
    events.add_note_off_event(192, 0, 0);
    events.add_note_off_event(384, 0, 0);

    const auto notes = events.combined_events();

    BOOST_CHECK_EQUAL(notes.size(), 2U);
}

BOOST_AUTO_TEST_CASE(handles_length_zero_notes_when_cancel_is_true)
{
    SightRead::Detail::NoteOnOffEvents events {true};
    events.add_note_on_event(0, 127, 0);
    events.add_note_off_event(0, 0, 0);

    const auto notes = events.combined_events();

    BOOST_CHECK_EQUAL(notes.size(), 1U);
}

BOOST_AUTO_TEST_SUITE_END()
