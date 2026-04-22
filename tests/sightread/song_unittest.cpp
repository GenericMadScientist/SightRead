#include <boost/test/unit_test.hpp>

#include "sightread/song.hpp"
#include "testhelpers.hpp"

namespace SightRead {
std::ostream& operator<<(std::ostream& stream, Difficulty difficulty)
{
    stream << static_cast<int>(difficulty);
    return stream;
}
}

BOOST_AUTO_TEST_CASE(instruments_returns_the_supported_instruments)
{
    SightRead::NoteTrack guitar_track {
        {make_note(192)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    SightRead::NoteTrack drum_track {
        {make_drum_note(192)},
        SightRead::TrackType::Drums,
        std::make_shared<SightRead::SongGlobalData>()};
    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Drums,
                        SightRead::Difficulty::Expert, drum_track);

    std::vector<SightRead::Instrument> instruments {
        SightRead::Instrument::Guitar, SightRead::Instrument::Drums};

    const auto parsed_instruments = song.instruments();

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_instruments.cbegin(),
                                  parsed_instruments.cend(),
                                  instruments.cbegin(), instruments.cend());
}

BOOST_AUTO_TEST_CASE(difficulties_returns_the_difficulties_for_an_instrument)
{
    SightRead::NoteTrack guitar_track {
        {make_note(192)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    SightRead::NoteTrack drum_track {
        {make_drum_note(192)},
        SightRead::TrackType::Drums,
        std::make_shared<SightRead::SongGlobalData>()};
    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Hard, guitar_track);
    song.add_note_track(SightRead::Instrument::Drums,
                        SightRead::Difficulty::Expert, drum_track);

    std::vector<SightRead::Difficulty> guitar_difficulties {
        SightRead::Difficulty::Hard, SightRead::Difficulty::Expert};
    std::vector<SightRead::Difficulty> drum_difficulties {
        SightRead::Difficulty::Expert};

    const auto guitar_diffs = song.difficulties(SightRead::Instrument::Guitar);
    const auto drum_diffs = song.difficulties(SightRead::Instrument::Drums);

    BOOST_CHECK_EQUAL_COLLECTIONS(guitar_diffs.cbegin(), guitar_diffs.cend(),
                                  guitar_difficulties.cbegin(),
                                  guitar_difficulties.cend());
    BOOST_CHECK_EQUAL_COLLECTIONS(drum_diffs.cbegin(), drum_diffs.cend(),
                                  drum_difficulties.cbegin(),
                                  drum_difficulties.cend());
}

BOOST_AUTO_TEST_SUITE(unison_phrases)

// This is to test that unison bonuses can apply when at least 2
// SightRead::Instruments have the phrase. This happens with the first phrase on
// RB3 Last Dance guitar, the phrase is missing on bass.
BOOST_AUTO_TEST_CASE(not_all_instruments_need_to_participate)
{
    SightRead::NoteTrack guitar_track {
        {make_note(768), make_note(1024)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    guitar_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}},
         {.position = SightRead::Tick {1024},
          .length = SightRead::Tick {100}}});

    SightRead::NoteTrack bass_track {
        {make_note(768), make_note(2048)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    bass_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}},
         {.position = SightRead::Tick {2048},
          .length = SightRead::Tick {100}}});

    SightRead::NoteTrack drum_track {
        {make_drum_note(768), make_drum_note(4096)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    drum_track.sp_phrases({{.position = SightRead::Tick {4096},
                            .length = SightRead::Tick {100}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, bass_track);
    song.add_note_track(SightRead::Instrument::Drums,
                        SightRead::Difficulty::Expert, drum_track);

    const auto unison_phrases = song.rb3_unison_phrases();
    const SightRead::StarPower expected_phrase {
        .position = SightRead::Tick {768}, .length = SightRead::Tick {100}};

    BOOST_CHECK_EQUAL(unison_phrases.size(), 1U);
    BOOST_CHECK_EQUAL(unison_phrases.at(0), expected_phrase);
}

BOOST_AUTO_TEST_CASE(phrases_with_slightly_different_ends_still_combined)
{
    SightRead::NoteTrack guitar_track {
        {make_note(768), make_note(1024)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    guitar_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}},
         {.position = SightRead::Tick {1024},
          .length = SightRead::Tick {100}}});

    SightRead::NoteTrack bass_track {
        {make_note(768), make_note(2048)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    bass_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {99}},
         {.position = SightRead::Tick {2048},
          .length = SightRead::Tick {100}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, bass_track);

    const auto unison_phrases = song.rb3_unison_phrases();
    const std::vector<SightRead::StarPower> expected_phrases {
        {.position = SightRead::Tick {768}, .length = SightRead::Tick {99}},
        {.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}};

    BOOST_CHECK_EQUAL_COLLECTIONS(
        unison_phrases.cbegin(), unison_phrases.cend(),
        expected_phrases.cbegin(), expected_phrases.cend());
}

BOOST_AUTO_TEST_CASE(phrases_with_slightly_different_starts_still_combined)
{
    SightRead::NoteTrack guitar_track {
        {make_note(768), make_note(1024)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    guitar_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}});

    SightRead::NoteTrack bass_track {
        {make_note(768), make_note(2048)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    bass_track.sp_phrases(
        {{.position = SightRead::Tick {767}, .length = SightRead::Tick {101}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, bass_track);

    const auto unison_phrases = song.rb3_unison_phrases();
    const std::vector<SightRead::StarPower> expected_phrases {
        {.position = SightRead::Tick {767}, .length = SightRead::Tick {101}},
        {.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}};

    BOOST_CHECK_EQUAL_COLLECTIONS(
        unison_phrases.cbegin(), unison_phrases.cend(),
        expected_phrases.cbegin(), expected_phrases.cend());
}

BOOST_AUTO_TEST_CASE(phrases_need_similar_lengths_to_be_combined)
{
    SightRead::NoteTrack guitar_track {
        {make_note(768), make_note(1024)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    guitar_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}});

    SightRead::NoteTrack bass_track {
        {make_note(768), make_note(2048)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    bass_track.sp_phrases(
        {{.position = SightRead::Tick {767}, .length = SightRead::Tick {501}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, bass_track);

    const auto unison_phrases = song.rb3_unison_phrases();

    BOOST_CHECK(unison_phrases.empty());
}

// This is checking SightRead reproduces a bug in YARG 0.14.0. As of writing
// this is the latest stable release so we replicate it, even though it's fixed
// in nightly.
BOOST_AUTO_TEST_CASE(
    phrases_can_have_very_different_ends_on_yarg_if_they_have_same_starts)
{
    SightRead::NoteTrack guitar_track {
        {make_note(768), make_note(1024)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    guitar_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}});

    SightRead::NoteTrack bass_track {
        {make_note(768), make_note(2048)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    bass_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {501}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, bass_track);

    const auto unison_phrases = song.yarg_unison_phrases();

    BOOST_CHECK(!unison_phrases.empty());
}

BOOST_AUTO_TEST_CASE(instruments_own_phrase_takes_priority_over_shortest_length)
{
    SightRead::NoteTrack guitar_track {
        {make_note(768)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    guitar_track.sp_phrases({{.position = SightRead::Tick {768},
                              .length = SightRead::Tick {1920}}});

    SightRead::NoteTrack drums_track {
        {make_drum_note(768)},
        SightRead::TrackType::Drums,
        std::make_shared<SightRead::SongGlobalData>()};
    drums_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {192}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, guitar_track);
    song.add_note_track(SightRead::Instrument::Drums,
                        SightRead::Difficulty::Expert, drums_track);

    const auto unison_phrases = song.rb3_unison_phrases();

    BOOST_CHECK_EQUAL(unison_phrases.size(), 1U);
    BOOST_CHECK_EQUAL(unison_phrases.at(0).length, SightRead::Tick {1920});
}

BOOST_AUTO_TEST_CASE(only_highest_difficulties_affect_unisons)
{
    SightRead::NoteTrack hard_guitar_track {
        {make_note(768)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    hard_guitar_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}});

    SightRead::NoteTrack expert_guitar_track {
        {make_note(768)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};

    SightRead::NoteTrack bass_track {
        {make_note(768)},
        SightRead::TrackType::FiveFret,
        std::make_shared<SightRead::SongGlobalData>()};
    bass_track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Hard, hard_guitar_track);
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, expert_guitar_track);
    song.add_note_track(SightRead::Instrument::Bass,
                        SightRead::Difficulty::Expert, bass_track);

    const auto unison_phrases = song.rb3_unison_phrases();

    BOOST_CHECK(unison_phrases.empty());
}

BOOST_AUTO_TEST_CASE(similar_instruments_cant_unison_together)
{
    SightRead::NoteTrack track {{make_note(768)},
                                SightRead::TrackType::FiveFret,
                                std::make_shared<SightRead::SongGlobalData>()};
    track.sp_phrases(
        {{.position = SightRead::Tick {768}, .length = SightRead::Tick {192}}});

    SightRead::Song song;
    song.add_note_track(SightRead::Instrument::Guitar,
                        SightRead::Difficulty::Expert, track);
    song.add_note_track(SightRead::Instrument::Rhythm,
                        SightRead::Difficulty::Expert, track);

    const auto unison_phrases = song.rb3_unison_phrases();

    BOOST_CHECK(unison_phrases.empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(speedup)

BOOST_AUTO_TEST_CASE(song_name_is_updated)
{
    SightRead::Song song;
    song.global_data().name("TestName");

    song.speedup(200);

    BOOST_CHECK_EQUAL(song.global_data().name(), "TestName (200%)");
}

BOOST_AUTO_TEST_CASE(song_name_is_unaffected_by_normal_speed)
{
    SightRead::Song song;
    song.global_data().name("TestName");

    song.speedup(100);

    BOOST_CHECK_EQUAL(song.global_data().name(), "TestName");
}

BOOST_AUTO_TEST_CASE(tempo_map_affected_by_speedup)
{
    SightRead::Song song;

    song.speedup(200);
    const auto& tempo_map = song.global_data().tempo_map();

    BOOST_CHECK_CLOSE(tempo_map.bpms().front().millibeats_per_minute, 240000.0,
                      0.001);
}

BOOST_AUTO_TEST_CASE(throws_on_negative_speeds)
{
    SightRead::Song song;

    BOOST_CHECK_THROW([&] { song.speedup(-100); }(), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(throws_on_zero_speed)
{
    SightRead::Song song;

    BOOST_CHECK_THROW([&] { song.speedup(0); }(), std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
