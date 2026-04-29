#include <boost/test/unit_test.hpp>

#include "sightread/detail/midiconverter.hpp"
#include "testhelpers.hpp"

namespace {
SightRead::Detail::MidiConverter drums_only_converter()
{
    return SightRead::Detail::MidiConverter({}).permit_instruments(
        {SightRead::Instrument::Drums});
}

SightRead::Detail::MidiConverter
guitar_only_converter(SightRead::HopoThreshold hopo_threshold = {},
                      std::optional<int> sustain_cutoff_threshold = {})
{
    return SightRead::Detail::MidiConverter(
               {.name = "",
                .artist = "",
                .charter = "",
                .hopo_threshold = hopo_threshold,
                .sustain_cutoff_threshold = sustain_cutoff_threshold})
        .permit_instruments({SightRead::Instrument::Guitar});
}

SightRead::Detail::MetaEvent part_event(std::string_view name)
{
    std::vector<std::uint8_t> bytes {name.cbegin(), name.cend()};
    return SightRead::Detail::MetaEvent {.type = 3, .data = bytes};
}

SightRead::Detail::MetaEvent text_event(std::string_view text)
{
    std::vector<std::uint8_t> bytes {text.cbegin(), text.cend()};
    return SightRead::Detail::MetaEvent {.type = 1, .data = bytes};
}
}

BOOST_AUTO_TEST_CASE(midi_to_song_has_correct_value_for_is_from_midi)
{
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_TEST(song.global_data().is_from_midi());
}

BOOST_AUTO_TEST_SUITE(midi_resolution_is_read_correctly)

BOOST_AUTO_TEST_CASE(midi_resolution_is_read)
{
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 200,
                                        .tracks = {}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_CHECK_EQUAL(song.global_data().resolution(), 200);
}

BOOST_AUTO_TEST_CASE(resolution_gt_zero_invariant_is_upheld)
{
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 0,
                                        .tracks = {}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(first_track_is_read_correctly)

BOOST_AUTO_TEST_CASE(tempos_are_read_correctly)
{
    SightRead::Detail::MidiTrack tempo_track {
        {{.time = 0,
          .event = {SightRead::Detail::MetaEvent {.type = 0x51,
                                                  .data = {6, 0x1A, 0x80}}}},
         {.time = 1920,
          .event = {SightRead::Detail::MetaEvent {.type = 0x51,
                                                  .data = {4, 0x93, 0xE0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {tempo_track}};
    const std::vector<SightRead::BPM> bpms {
        {.position = SightRead::Tick {0}, .millibeats_per_minute = 150000},
        {.position = SightRead::Tick {1920}, .millibeats_per_minute = 200000}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& tempo_map = song.global_data().tempo_map();

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.bpms().cbegin(),
                                  tempo_map.bpms().cend(), bpms.cbegin(),
                                  bpms.cend());
}

BOOST_AUTO_TEST_CASE(too_short_tempo_events_cause_an_exception)
{
    SightRead::Detail::MidiTrack tempo_track {
        {{.time = 0,
          .event
          = {SightRead::Detail::MetaEvent {.type = 0x51, .data = {6, 0x1A}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {tempo_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(time_signatures_are_read_correctly)
{
    SightRead::Detail::MidiTrack ts_track {
        {{.time = 0,
          .event = {SightRead::Detail::MetaEvent {.type = 0x58,
                                                  .data = {6, 2, 24, 8}}}},
         {.time = 1920,
          .event = {SightRead::Detail::MetaEvent {.type = 0x58,
                                                  .data = {3, 3, 24, 8}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {ts_track}};
    const std::vector<SightRead::TimeSignature> tses {
        {.position = SightRead::Tick {0}, .numerator = 6, .denominator = 4},
        {.position = SightRead::Tick {1920}, .numerator = 3, .denominator = 8}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& tempo_map = song.global_data().tempo_map();

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.time_sigs().cbegin(),
                                  tempo_map.time_sigs().cend(), tses.cbegin(),
                                  tses.cend());
}

BOOST_AUTO_TEST_CASE(time_signatures_with_large_denominators_cause_an_exception)
{
    SightRead::Detail::MidiTrack ts_track {
        {{.time = 0,
          .event = {SightRead::Detail::MetaEvent {.type = 0x58,
                                                  .data = {6, 32, 24, 8}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {ts_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(too_short_time_sig_events_cause_an_exception)
{
    SightRead::Detail::MidiTrack ts_track {
        {{.time = 0,
          .event
          = {SightRead::Detail::MetaEvent {.type = 0x58, .data = {6}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {ts_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

// This is to catch imprecision caused by SightRead previously using integers
// for BPM * 1000 internally, like .chart files do. This isn't generally a huge
// loss but still needless. If a 500,002 us/quarter note MIDI tempo is
// represented this way as 119.999 BPM, over 10 minutes this causes a loss of
// precision of around 2.6 ms.
BOOST_AUTO_TEST_CASE(loss_of_precision_reading_bpms_is_negligible)
{
    SightRead::Detail::MidiTrack tempo_track {
        {{.time = 0,
          .event = {SightRead::Detail::MetaEvent {.type = 0x51,
                                                  .data = {7, 0xA1, 0x22}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {tempo_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& tempo_map = song.global_data().tempo_map();

    BOOST_CHECK_CLOSE(
        tempo_map.to_seconds(SightRead::Tick {1200 * 480}).value(), 600.0024,
        0.000001);
}

BOOST_AUTO_TEST_CASE(song_name_is_not_read_from_midi)
{
    SightRead::Detail::MidiTrack name_track {
        {{.time = 0,
          .event = {SightRead::Detail::MetaEvent {
              .type = 1, .data = {72, 101, 108, 108, 111}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {name_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_CHECK_NE(song.global_data().name(), "Hello");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(ini_values_are_used_when_converting_mid_files)
{
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {}};
    const SightRead::Metadata metadata {.name = "TestName",
                                        .artist = "GMS",
                                        .charter = "NotGMS",
                                        .hopo_threshold = {},
                                        .sustain_cutoff_threshold = {}};

    const auto song = SightRead::Detail::MidiConverter(metadata).convert(midi);

    BOOST_CHECK_EQUAL(song.global_data().name(), "TestName");
    BOOST_CHECK_EQUAL(song.global_data().artist(), "GMS");
    BOOST_CHECK_EQUAL(song.global_data().charter(), "NotGMS");
}

BOOST_AUTO_TEST_SUITE(practice_mode_sections_are_read_from_mid_files)

BOOST_AUTO_TEST_CASE(sections_prefixed_with_section_space_are_read)
{
    const std::vector<SightRead::PracticeSection> expected_sections {
        {.name = "Start", .start = SightRead::Tick {768}}};
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 768, .event = {text_event("[section Start]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {events_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& practice_sections = song.global_data().practice_sections();

    BOOST_CHECK_EQUAL_COLLECTIONS(
        practice_sections.cbegin(), practice_sections.cend(),
        expected_sections.cbegin(), expected_sections.cend());
}

BOOST_AUTO_TEST_CASE(sections_prefixed_with_section_underscore_are_read)
{
    const std::vector<SightRead::PracticeSection> expected_sections {
        {.name = "Start", .start = SightRead::Tick {768}}};
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 768, .event = {text_event("[section_Start]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {events_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& practice_sections = song.global_data().practice_sections();

    BOOST_CHECK_EQUAL_COLLECTIONS(
        practice_sections.cbegin(), practice_sections.cend(),
        expected_sections.cbegin(), expected_sections.cend());
}

BOOST_AUTO_TEST_CASE(sections_prefixed_with_prc_underscore_are_read)
{
    const std::vector<SightRead::PracticeSection> expected_sections {
        {.name = "Start", .start = SightRead::Tick {768}}};
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 768, .event = {text_event("[prc_Start]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {events_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& practice_sections = song.global_data().practice_sections();

    BOOST_CHECK_EQUAL_COLLECTIONS(
        practice_sections.cbegin(), practice_sections.cend(),
        expected_sections.cbegin(), expected_sections.cend());
}

BOOST_AUTO_TEST_CASE(sections_with_other_prefixes_are_ignored)
{
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 768, .event = {text_event("[ignored Start]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {events_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& practice_sections = song.global_data().practice_sections();

    BOOST_TEST(practice_sections.empty());
}

BOOST_AUTO_TEST_CASE(non_text_events_are_ignored)
{
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 768, .event = {part_event("[section Start]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {events_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& practice_sections = song.global_data().practice_sections();

    BOOST_TEST(practice_sections.empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(notes_are_read_from_mids_correctly)

BOOST_AUTO_TEST_CASE(notes_of_every_difficulty_are_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {84, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {72, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {60, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {84, 0}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {72, 0}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {60, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const std::vector<SightRead::Note> green_note {
        make_note(768, 192, SightRead::FIVE_FRET_GREEN)};
    const std::array<SightRead::Difficulty, 4> diffs {
        SightRead::Difficulty::Easy, SightRead::Difficulty::Medium,
        SightRead::Difficulty::Hard, SightRead::Difficulty::Expert};

    const auto song = guitar_only_converter().convert(midi);

    for (auto diff : diffs) {
        const auto& notes
            = song.track(SightRead::Instrument::Guitar, diff).notes();
        BOOST_CHECK_EQUAL_COLLECTIONS(notes.cbegin(), notes.cend(),
                                      green_note.cbegin(), green_note.cend());
    }
}

BOOST_AUTO_TEST_CASE(notes_are_read_from_part_guitar)
{
    SightRead::Detail::MidiTrack other_track {
        {{.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {other_track, note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_RED);
}

BOOST_AUTO_TEST_CASE(part_guitar_event_need_not_be_the_first_event)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0,
          .event = {SightRead::Detail::MetaEvent {
              .type = 0x7F, .data = {0x05, 0x0F, 0x09, 0x08, 0x40}}}},
         {.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_RED);
}

BOOST_AUTO_TEST_CASE(guitar_notes_are_also_read_from_t1_gems)
{
    SightRead::Detail::MidiTrack other_track {
        {{.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("T1 GEMS")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {other_track, note_track}};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_RED);
}

BOOST_AUTO_TEST_CASE(note_on_events_must_have_a_corresponding_note_off_event)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
         {.time = 1152,
          .event = {SightRead::Detail::MidiEvent {.status = 0x90,
                                                  .data = {96, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(corresponding_note_off_events_are_after_note_on_events)
{
    SightRead::Detail::MidiTrack note_track {{
        {.time = 0, .event = {part_event("PART GUITAR")}},
        {.time = 480,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
        {.time = 480,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 960,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
        {.time = 960,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 1440,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
    }};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.size(), 2U);
    BOOST_CHECK_EQUAL(notes.at(0).lengths.at(0), SightRead::Tick {480});
}

BOOST_AUTO_TEST_CASE(note_on_events_with_velocity_zero_count_as_note_off_events)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_NO_THROW([&] { return converter.convert(midi); }());
}

BOOST_AUTO_TEST_CASE(
    note_on_events_with_no_intermediate_note_off_events_are_not_merged)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 769,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 800,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
         {.time = 801,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {96, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.size(), 2U);
}

BOOST_AUTO_TEST_CASE(each_note_on_event_consumes_the_following_note_off_event)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 769,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 800,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
         {.time = 1000,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {96, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.size(), 2U);
    BOOST_CHECK_GT(notes.at(1).lengths.at(0), SightRead::Tick {0});
}

BOOST_AUTO_TEST_CASE(note_off_events_can_be_zero_ticks_after_the_note_on_events)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {96, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.size(), 1U);
}

BOOST_AUTO_TEST_CASE(
    parseerror_thrown_if_note_on_has_no_corresponding_note_off_track)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event = {SightRead::Detail::MidiEvent {.status = 0x90,
                                                  .data = {96, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(open_notes_are_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 1, 1, 0xF7}}}},
         {.time = 770,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 1, 0, 0xF7}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_OPEN);
}

BOOST_AUTO_TEST_CASE(length_zero_open_events_are_handled_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 1, 1, 0xF7}}}},
         {.time = 768,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 1, 0, 0xF7}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_OPEN);
}

BOOST_AUTO_TEST_CASE(parseerror_thrown_if_open_note_ons_have_no_note_offs)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 1, 1, 0xF7}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    BOOST_CHECK_THROW([&] { return guitar_only_converter().convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(extra_opens_are_read_with_enhanced_opens)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0, .event = {text_event("[ENHANCED_OPENS]")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_OPEN);
}

BOOST_AUTO_TEST_CASE(extra_opens_are_ignored_without_enhanced_opens)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 0}}}},
         {.time = 961,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 962,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_GREEN);
}

BOOST_AUTO_TEST_CASE(open_chords_permitted_with_allow_open_chords_true)
{
    SightRead::Detail::MidiTrack note_track {{
        {.time = 0, .event = {part_event("PART GUITAR")}},
        {.time = 0, .event = {text_event("[ENHANCED_OPENS]")}},
        {.time = 768,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 64}}}},
        {.time = 768,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 960,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 0}}}},
        {.time = 960,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}},
    }};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song
        = guitar_only_converter().allow_open_chords(true).convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        (1 << SightRead::FIVE_FRET_OPEN) | (1 << SightRead::FIVE_FRET_GREEN));
}

BOOST_AUTO_TEST_CASE(open_chords_forbidden_by_default)
{
    SightRead::Detail::MidiTrack note_track {{
        {.time = 0, .event = {part_event("PART GUITAR")}},
        {.time = 0, .event = {text_event("[ENHANCED_OPENS]")}},
        {.time = 768,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 64}}}},
        {.time = 768,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 960,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 0}}}},
        {.time = 960,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}},
    }};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_OPEN);
}

BOOST_AUTO_TEST_SUITE_END()

// Note that a note at the very end of a solo event is not considered part of
// the solo for a .mid, but it is for a .chart.
BOOST_AUTO_TEST_CASE(solos_are_read_from_mids_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {103, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 900,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 900,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {103, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 960,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {97, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const std::vector<SightRead::Solo> solos {{.start = SightRead::Tick {768},
                                               .end = SightRead::Tick {900},
                                               .value = 100}};

    const auto song = guitar_only_converter().convert(midi);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_solos.cbegin(), parsed_solos.cend(),
                                  solos.cbegin(), solos.cend());
}

BOOST_AUTO_TEST_SUITE(star_power_is_read)

BOOST_AUTO_TEST_CASE(a_single_phrase_is_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {116, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 900,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {116, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const std::vector<SightRead::StarPower> sp_phrases {
        {.position = SightRead::Tick {768}, .length = SightRead::Tick {132}}};

    const auto song = guitar_only_converter().convert(midi);
    const auto& parsed_sp = song.track(SightRead::Instrument::Guitar,
                                       SightRead::Difficulty::Expert)
                                .sp_phrases();

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_sp.cbegin(), parsed_sp.cend(),
                                  sp_phrases.cbegin(), sp_phrases.cend());
}

BOOST_AUTO_TEST_CASE(note_off_event_is_required_for_every_phrase)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {116, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const SightRead::Detail::MidiConverter converter {{}};

    BOOST_CHECK_THROW([&] { return converter.convert(midi); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(mids_with_multiple_solos_and_no_sp_have_solos_read_as_sp)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {103, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 800,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 64}}}},
         {.time = 900,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {103, 64}}}},
         {.time = 950,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {103, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 1000,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 64}}}},
         {.time = 1000,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {103, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert);

    BOOST_TEST(
        track.solos(SightRead::DrumSettings::default_settings()).empty());
    BOOST_CHECK_EQUAL(track.sp_phrases().size(), 2U);
}

BOOST_AUTO_TEST_SUITE(sustain_trimming)

BOOST_AUTO_TEST_CASE(short_midi_sustains_are_trimmed)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 100,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 170,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 200,
                                        .tracks = {note_track}};
    const auto song = guitar_only_converter().convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.at(0).lengths.at(0), SightRead::Tick {0});
    BOOST_CHECK_EQUAL(notes.at(1).lengths.at(0), SightRead::Tick {70});
}

BOOST_AUTO_TEST_CASE(metadata_cutoff_is_ignored_by_default)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 100,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 170,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 200,
                                        .tracks = {note_track}};
    const auto song = guitar_only_converter({}, 100).convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.at(0).lengths.at(0), SightRead::Tick {70});
}

BOOST_AUTO_TEST_CASE(metadata_cutoff_is_used_if_flagged_to_use)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 100,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 200,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 301,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 200,
                                        .tracks = {note_track}};
    const auto song = guitar_only_converter({}, 100)
                          .use_sustain_cutoff_threshold(true)
                          .convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.at(0).lengths.at(0), SightRead::Tick {0});
    BOOST_CHECK_EQUAL(notes.at(1).lengths.at(0), SightRead::Tick {101});
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(bres_are_read_correctly_on_non_drum_instruments)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {120, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 75,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {120, 0}}}}}};
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 45, .event = {text_event("[coda]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track, events_track}};
    const auto song = guitar_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::BigRockEnding> bres {
        {.start = SightRead::Tick {45}, .end = SightRead::Tick {75}}};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.bres().cbegin(), track.bres().cend(),
                                  bres.cbegin(), bres.cend());
}

BOOST_AUTO_TEST_SUITE(midi_hopos_and_taps)

BOOST_AUTO_TEST_CASE(automatically_set_based_on_distance)
{
    SightRead::Detail::MidiTrack note_track {{
        {.time = 0, .event = {part_event("PART GUITAR")}},
        {.time = 0,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 1,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
        {.time = 161,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
        {.time = 162,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
        {.time = 323,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
        {.time = 324,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
    }};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
    BOOST_CHECK_EQUAL(notes.at(2).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(does_not_do_it_on_same_note)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(note_after_chord_not_automatically_hopo_with_shared_lane)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(forcing_is_handled_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {101, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {101, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {102, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 162,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {102, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags,
                      SightRead::FLAGS_FORCE_HOPO | SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_FORCE_STRUM
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(chords_are_not_hopos_due_to_proximity)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(chords_can_be_forced)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {101, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 162,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {101, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_FORCE_HOPO | SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(length_zero_hopo_force_hopo_events_include_notes)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {101, 64}}}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {101, 0}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags,
                      SightRead::FLAGS_FORCE_HOPO | SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(length_zero_hopo_force_strum_events_include_notes)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 2,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 2,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {102, 64}}}},
         {.time = 2,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {102, 0}}}},
         {.time = 3,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_FORCE_STRUM
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(
    positive_length_hopo_force_strum_events_exclude_notes_at_end)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 2,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {102, 64}}}},
         {.time = 10,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {102, 0}}}},
         {.time = 10,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 11,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(tap_note_sysex_events_are_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 4, 1, 0xF7}}}},
         {.time = 770,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 4, 0, 0xF7}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto flags = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes()
                           .at(0)
                           .flags;

    BOOST_CHECK_EQUAL(flags & SightRead::NoteFlags::FLAGS_TAP,
                      SightRead::NoteFlags::FLAGS_TAP);
}

BOOST_AUTO_TEST_CASE(tap_note_sysex_events_spanning_all_diffs_are_handled)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 3, 4, 1, 0xF7}}}},
         {.time = 770,
          .event = {SightRead::Detail::SysexEvent {
              {0x50, 0x53, 0, 0, 0xFF, 4, 0, 0xF7}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto flags = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes()
                           .at(0)
                           .flags;

    BOOST_CHECK_EQUAL(flags & SightRead::NoteFlags::FLAGS_TAP,
                      SightRead::NoteFlags::FLAGS_TAP);
}

BOOST_AUTO_TEST_CASE(tap_note_events_are_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {104, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 1,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {104, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags,
                      SightRead::FLAGS_TAP | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(taps_take_precedence_over_hopos)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {104, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 162,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {104, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_TAP | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(chords_can_be_taps)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 160,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 160,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 160,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {104, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 161,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {104, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_TAP | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(other_resolutions_are_handled_correctly)
{
    SightRead::Detail::MidiTrack note_track {{
        {.time = 0, .event = {part_event("PART GUITAR")}},
        {.time = 0,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 1,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
        {.time = 65,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
        {.time = 66,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
        {.time = 131,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
        {.time = 132,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
    }};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song = guitar_only_converter().convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
    BOOST_CHECK_EQUAL(notes.at(2).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(custom_hopo_threshold_is_handled_correctly)
{
    SightRead::Detail::MidiTrack note_track {{
        {.time = 0, .event = {part_event("PART GUITAR")}},
        {.time = 0,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
        {.time = 1,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
        {.time = 161,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
        {.time = 162,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
        {.time = 323,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
        {.time = 324,
         .event
         = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
    }};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};

    const auto song
        = guitar_only_converter(
              {.threshold_type = SightRead::HopoThresholdType::HopoFrequency,
               .hopo_frequency = SightRead::Tick {240}})
              .convert(midi);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
    BOOST_CHECK_EQUAL(notes.at(2).flags,
                      SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(not_done_on_drums)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 64}}}},
         {.time = 161,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 162,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};
    const auto converter
        = SightRead::Detail::MidiConverter({}).permit_instruments(
            {SightRead::Instrument::Drums});
    const auto song = converter.convert(midi);
    const auto& notes = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert)
                            .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags, SightRead::FLAGS_DRUMS);
    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_DRUMS | SightRead::FLAGS_CYMBAL);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(other_five_fret_instruments_are_read_from_mid)

BOOST_AUTO_TEST_CASE(guitar_coop_is_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR COOP")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::GuitarCoop,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_CASE(bass_is_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART BASS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto converter
        = SightRead::Detail::MidiConverter({}).permit_instruments(
            {SightRead::Instrument::Bass});
    const auto song = converter.convert(midi);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::Bass,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_CASE(rhythm_is_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART RHYTHM")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::Rhythm,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_CASE(keys_is_read)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART KEYS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::Keys,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(six_fret_instruments_are_read_correctly_from_mid)

BOOST_AUTO_TEST_CASE(six_fret_guitar_is_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR GHL")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {94, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {94, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& track = song.track(SightRead::Instrument::GHLGuitar,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_ghl_note(0, 65, SightRead::SIX_FRET_OPEN)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(six_fret_bass_is_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART BASS GHL")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {94, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {94, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& track = song.track(SightRead::Instrument::GHLBass,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_ghl_note(0, 65, SightRead::SIX_FRET_OPEN)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(six_fret_rhythm_is_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART RHYTHM GHL")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {94, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {94, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& track = song.track(SightRead::Instrument::GHLRhythm,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_ghl_note(0, 65, SightRead::SIX_FRET_OPEN)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(six_fret_guitar_coop_is_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR COOP GHL")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {94, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {94, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto& track = song.track(SightRead::Instrument::GHLGuitarCoop,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_ghl_note(0, 65, SightRead::SIX_FRET_OPEN)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(drums_are_read_correctly_from_mid)

BOOST_AUTO_TEST_CASE(drums_track_read_correctly_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {110, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 65,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {110, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_drum_note(0, SightRead::DRUM_YELLOW)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(notes_at_end_of_tom_markers_are_cymbals)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {110, 64}}}},
         {.time = 480,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {110, 0}}}},
         {.time = 480,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 481,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    const auto& note = track.notes().at(0);

    BOOST_CHECK_EQUAL(note.flags,
                      SightRead::FLAGS_DRUMS | SightRead::FLAGS_CYMBAL);
}

BOOST_AUTO_TEST_CASE(notes_at_end_of_zero_length_tom_markers_are_toms)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 480,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {110, 64}}}},
         {.time = 480,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {110, 0}}}},
         {.time = 480,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 481,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 480,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    const auto& note = track.notes().at(0);

    BOOST_CHECK_EQUAL(note.flags, SightRead::FLAGS_DRUMS);
}

BOOST_AUTO_TEST_CASE(double_kicks_are_read_correctly_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {95, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {95, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_drum_note(0, SightRead::DRUM_DOUBLE_KICK)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(drum_fills_are_read_correctly_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {120, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 75,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {120, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::DrumFill> fills {
        {.position = SightRead::Tick {45}, .length = SightRead::Tick {30}}};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.drum_fills().cbegin(),
                                  track.drum_fills().cend(), fills.cbegin(),
                                  fills.cend());
}

BOOST_AUTO_TEST_CASE(bres_are_read_correctly_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {120, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 75,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {120, 0}}}}}};
    SightRead::Detail::MidiTrack events_track {
        {{.time = 0, .event = {part_event("EVENTS")}},
         {.time = 45, .event = {text_event("[coda]")}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track, events_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::BigRockEnding> bres {
        {.start = SightRead::Tick {45}, .end = SightRead::Tick {75}}};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.bres().cbegin(), track.bres().cend(),
                                  bres.cbegin(), bres.cend());
}

BOOST_AUTO_TEST_CASE(disco_flips_are_read_correctly_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 15,
          .event = {SightRead::Detail::MetaEvent {
              .type = 1,
              .data = {0x5B, 0x6D, 0x69, 0x78, 0x20, 0x33, 0x20, 0x64, 0x72,
                       0x75, 0x6D, 0x73, 0x30, 0x64, 0x5D}}}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}},
         {.time = 75,
          .event = {SightRead::Detail::MetaEvent {
              .type = 1,
              .data = {0x5B, 0x6D, 0x69, 0x78, 0x20, 0x33, 0x20, 0x64, 0x72,
                       0x75, 0x6D, 0x73, 0x30, 0x5D}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);
    const auto& note = track.notes().at(0);

    BOOST_CHECK_EQUAL(note.flags,
                      SightRead::FLAGS_CYMBAL | SightRead::FLAGS_DISCO
                          | SightRead::FLAGS_DRUMS);
}

BOOST_AUTO_TEST_CASE(
    missing_disco_flip_end_event_causes_all_subsequent_notes_to_flip)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 15,
          .event = {SightRead::Detail::MetaEvent {
              .type = 1,
              .data = {0x5B, 0x6D, 0x69, 0x78, 0x20, 0x33, 0x20, 0x64, 0x72,
                       0x75, 0x6D, 0x73, 0x30, 0x64, 0x5D}}}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);
    const auto& note = track.notes().at(0);

    BOOST_CHECK_EQUAL(note.flags,
                      SightRead::FLAGS_CYMBAL | SightRead::FLAGS_DISCO
                          | SightRead::FLAGS_DRUMS);
}

BOOST_AUTO_TEST_CASE(drum_five_lane_to_four_lane_conversion_is_done_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {101, 64}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {101, 0}}}},
         {.time = 2,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {100, 64}}}},
         {.time = 3,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {100, 0}}}},
         {.time = 4,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {101, 64}}}},
         {.time = 4,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {100, 64}}}},
         {.time = 5,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {101, 0}}}},
         {.time = 5,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {100, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_drum_note(0, SightRead::DRUM_GREEN),
        make_drum_note(2, SightRead::DRUM_GREEN, SightRead::FLAGS_CYMBAL),
        make_drum_note(4, SightRead::DRUM_BLUE),
        make_drum_note(4, SightRead::DRUM_GREEN, SightRead::FLAGS_CYMBAL)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(flam_sections_are_read_correctly)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {109, 64}}}},
         {.time = 45,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {98, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {109, 0}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {98, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);
    const auto& note = track.notes().at(0);

    BOOST_CHECK_EQUAL(note.flags,
                      SightRead::FLAGS_CYMBAL | SightRead::FLAGS_DRUMS
                          | SightRead::FLAGS_FLAM);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(dynamics_parsing)

BOOST_AUTO_TEST_CASE(dynamics_are_parsed_from_mid)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0, .event = {text_event("[ENABLE_CHART_DYNAMICS]")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 1}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 2,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 3,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}},
         {.time = 4,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 127}}}},
         {.time = 5,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {
        make_drum_note(0, SightRead::DRUM_RED, SightRead::FLAGS_GHOST),
        make_drum_note(2, SightRead::DRUM_RED),
        make_drum_note(4, SightRead::DRUM_RED, SightRead::FLAGS_ACCENT)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(dynamics_not_parsed_from_mid_without_ENABLE_CHART_DYNAMICS)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 1}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {make_drum_note(0, SightRead::DRUM_RED)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(ENABLE_CHART_DYNAMICS_works_without_braces)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART DRUMS")}},
         {.time = 0, .event = {text_event("ENABLE_CHART_DYNAMICS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 1}}}},
         {.time = 1,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const auto song = drums_only_converter().convert(midi);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    std::vector<SightRead::Note> notes {make_drum_note(
        0, SightRead::DRUM_RED, SightRead::NoteFlags::FLAGS_GHOST)};

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(instruments_not_permitted_are_dropped_from_midis)
{
    SightRead::Detail::MidiTrack guitar_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    SightRead::Detail::MidiTrack bass_track {
        {{.time = 0, .event = {part_event("PART BASS")}},
         {.time = 0,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 65,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {guitar_track, bass_track}};
    const std::vector<SightRead::Instrument> expected_instruments {
        SightRead::Instrument::Guitar};

    const auto song = guitar_only_converter().convert(midi);
    const auto instruments = song.instruments();

    BOOST_CHECK_EQUAL_COLLECTIONS(instruments.cbegin(), instruments.cend(),
                                  expected_instruments.cbegin(),
                                  expected_instruments.cend());
}

BOOST_AUTO_TEST_CASE(solos_ignored_from_midis_if_not_permitted)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {103, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 900,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 900,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {103, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 960,
          .event = {SightRead::Detail::MidiEvent {.status = 0x80,
                                                  .data = {97, 64}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto converter = guitar_only_converter().parse_solos(false);
    const auto song = converter.convert(midi);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_CHECK(parsed_solos.empty());
}

BOOST_AUTO_TEST_SUITE(fortnite_instruments)

BOOST_AUTO_TEST_CASE(ch_instruments_have_priority_over_fortnite)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};
    const std::vector<SightRead::Instrument> expected_instruments {
        SightRead::Instrument::Guitar};

    const auto song = SightRead::Detail::MidiConverter({}).convert(midi);
    const auto instruments = song.instruments();

    BOOST_CHECK_EQUAL_COLLECTIONS(instruments.cbegin(), instruments.cend(),
                                  expected_instruments.cbegin(),
                                  expected_instruments.cend());
    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .at(0)
            .colours(),
        1 << SightRead::FIVE_FRET_RED);
}

BOOST_AUTO_TEST_CASE(fortnite_instrument_notes_are_separated)
{
    SightRead::Detail::MidiTrack note_track {
        {{.time = 0, .event = {part_event("PART GUITAR")}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {96, 64}}}},
         {.time = 768,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x90, .data = {97, 64}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {96, 0}}}},
         {.time = 960,
          .event
          = {SightRead::Detail::MidiEvent {.status = 0x80, .data = {97, 0}}}}}};
    const SightRead::Detail::Midi midi {.ticks_per_quarter_note = 192,
                                        .tracks = {note_track}};

    const auto song
        = SightRead::Detail::MidiConverter({})
              .permit_instruments({SightRead::Instrument::FortniteGuitar})
              .convert(midi);

    BOOST_CHECK_EQUAL(song.track(SightRead::Instrument::FortniteGuitar,
                                 SightRead::Difficulty::Expert)
                          .notes()
                          .size(),
                      2U);
}

BOOST_AUTO_TEST_SUITE_END()
