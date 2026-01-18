#include <array>

#include <boost/test/unit_test.hpp>

#include "sightread/tempomap.hpp"
#include "testhelpers.hpp"

BOOST_AUTO_TEST_CASE(bpm_method_on_bpm_struct_returns_correct_value)
{
    SightRead::BPM bpm {SightRead::Tick {0}, 120000.0};

    BOOST_CHECK_CLOSE(bpm.bpm(), 120.0, 0.001);
}

BOOST_AUTO_TEST_SUITE(sync_track_ctor_maintains_invariants)

BOOST_AUTO_TEST_CASE(bpms_are_sorted_by_position)
{
    SightRead::TempoMap tempo_map {{},
                                   {{SightRead::Tick {0}, 150000},
                                    {SightRead::Tick {2000}, 200000},
                                    {SightRead::Tick {1000}, 225000}},
                                   {},
                                   192};
    std::vector<SightRead::BPM> expected_bpms {
        {SightRead::Tick {0}, 150000},
        {SightRead::Tick {1000}, 225000},
        {SightRead::Tick {2000}, 200000}};

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.bpms().cbegin(),
                                  tempo_map.bpms().cend(),
                                  expected_bpms.cbegin(), expected_bpms.cend());
}

BOOST_AUTO_TEST_CASE(no_two_bpms_have_the_same_position)
{
    SightRead::TempoMap tempo_map {
        {},
        {{SightRead::Tick {0}, 150000}, {SightRead::Tick {0}, 200000}},
        {},
        192};
    std::vector<SightRead::BPM> expected_bpms {{SightRead::Tick {0}, 200000}};

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.bpms().cbegin(),
                                  tempo_map.bpms().cend(),
                                  expected_bpms.cbegin(), expected_bpms.cend());
}

BOOST_AUTO_TEST_CASE(bpms_is_never_empty)
{
    SightRead::TempoMap tempo_map;
    std::vector<SightRead::BPM> expected_bpms {{SightRead::Tick {0}, 120000}};

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.bpms().cbegin(),
                                  tempo_map.bpms().cend(),
                                  expected_bpms.cbegin(), expected_bpms.cend());
}

BOOST_AUTO_TEST_CASE(time_signatures_are_sorted_by_position)
{
    SightRead::TempoMap tempo_map {{{SightRead::Tick {0}, 4, 4},
                                    {SightRead::Tick {2000}, 3, 3},
                                    {SightRead::Tick {1000}, 2, 2}},
                                   {},
                                   {},
                                   192};
    std::vector<SightRead::TimeSignature> expected_tses {
        {SightRead::Tick {0}, 4, 4},
        {SightRead::Tick {1000}, 2, 2},
        {SightRead::Tick {2000}, 3, 3}};

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.time_sigs().cbegin(),
                                  tempo_map.time_sigs().cend(),
                                  expected_tses.cbegin(), expected_tses.cend());
}

BOOST_AUTO_TEST_CASE(no_two_time_signatures_have_the_same_position)
{
    SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 4, 4}, {SightRead::Tick {0}, 3, 4}},
        {},
        {},
        192};
    std::vector<SightRead::TimeSignature> expected_tses {
        {SightRead::Tick {0}, 3, 4}};

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.time_sigs().cbegin(),
                                  tempo_map.time_sigs().cend(),
                                  expected_tses.cbegin(), expected_tses.cend());
}

BOOST_AUTO_TEST_CASE(time_sigs_is_never_empty)
{
    SightRead::TempoMap tempo_map;
    std::vector<SightRead::TimeSignature> expected_tses {
        {SightRead::Tick {0}, 4, 4}};

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.time_sigs().cbegin(),
                                  tempo_map.time_sigs().cend(),
                                  expected_tses.cbegin(), expected_tses.cend());
}

BOOST_AUTO_TEST_CASE(bpms_must_be_positive)
{
    BOOST_CHECK_THROW(([&] {
                          return SightRead::TempoMap {
                              {}, {{SightRead::Tick {192}, 0}}, {}, 192};
                      })(),
                      SightRead::ParseError);
    BOOST_CHECK_THROW(([&] {
                          return SightRead::TempoMap {
                              {}, {{SightRead::Tick {192}, -1}}, {}, 192};
                      })(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(time_signatures_must_be_positive_positive)
{
    BOOST_CHECK_THROW(([&] {
                          return SightRead::TempoMap {
                              {{SightRead::Tick {0}, 0, 4}}, {}, {}, 192};
                      })(),
                      SightRead::ParseError);
    BOOST_CHECK_THROW(([&] {
                          return SightRead::TempoMap {
                              {{SightRead::Tick {0}, -1, 4}}, {}, {}, 192};
                      })(),
                      SightRead::ParseError);
    BOOST_CHECK_THROW(([&] {
                          return SightRead::TempoMap {
                              {{SightRead::Tick {0}, 4, 0}}, {}, {}, 192};
                      })(),
                      SightRead::ParseError);
    BOOST_CHECK_THROW(([&] {
                          return SightRead::TempoMap {
                              {{SightRead::Tick {0}, 4, -1}}, {}, {}, 192};
                      })(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(speedup_returns_correct_tempo_map)
{
    const SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 4, 4}},
        {{SightRead::Tick {0}, 120000}, {SightRead::Tick {192}, 240000}},
        {},
        192};
    const std::vector<SightRead::BPM> expected_bpms {
        {SightRead::Tick {0}, 180000}, {SightRead::Tick {192}, 360000}};
    const std::vector<SightRead::TimeSignature> expected_tses {
        {SightRead::Tick {0}, 4, 4}};

    const auto speedup = tempo_map.speedup(150);

    BOOST_CHECK_EQUAL_COLLECTIONS(speedup.bpms().cbegin(),
                                  speedup.bpms().cend(), expected_bpms.cbegin(),
                                  expected_bpms.cend());
    BOOST_CHECK_EQUAL_COLLECTIONS(speedup.time_sigs().cbegin(),
                                  speedup.time_sigs().cend(),
                                  expected_tses.cbegin(), expected_tses.cend());
}

BOOST_AUTO_TEST_CASE(speedup_updates_time_conversion_correctly)
{
    const SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 4, 4}},
        {{SightRead::Tick {0}, 120000}, {SightRead::Tick {192}, 240000}},
        {},
        192};

    const auto speedup = tempo_map.speedup(150);

    BOOST_CHECK_CLOSE(speedup.to_beats(SightRead::Second {0.5}).value(), 2.0,
                      0.0001);
}

BOOST_AUTO_TEST_CASE(speedup_doesnt_overflow)
{
    const SightRead::TempoMap tempo_map {
        {}, {{SightRead::Tick {0}, 200000000}}, {}, 192};
    const std::vector<SightRead::BPM> expected_bpms {
        {SightRead::Tick {0}, 400000000}};

    const auto speedup = tempo_map.speedup(200);

    BOOST_CHECK_EQUAL_COLLECTIONS(speedup.bpms().cbegin(),
                                  speedup.bpms().cend(), expected_bpms.cbegin(),
                                  expected_bpms.cend());
}

BOOST_AUTO_TEST_CASE(seconds_to_beats_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 4, 4}},
        {{SightRead::Tick {0}, 150000}, {SightRead::Tick {800}, 200000}},
        {},
        200};
    constexpr std::array beats {-1.0, 0.0, 3.0, 5.0};
    constexpr std::array seconds {-0.5, 0.0, 1.2, 1.9};

    for (auto i = 0U; i < beats.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_beats(SightRead::Second {seconds.at(i)}).value(),
            beats.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(seconds_to_beats_conversion_works_correctly_after_speedup)
{
    SightRead::TempoMap tempo_map = SightRead::TempoMap().speedup(200);

    BOOST_CHECK_CLOSE(tempo_map.to_beats(SightRead::Second {1}).value(), 4.0,
                      0.0001);
}

BOOST_AUTO_TEST_CASE(beats_to_seconds_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 4, 4}},
        {{SightRead::Tick {0}, 150000}, {SightRead::Tick {800}, 200000}},
        {},
        200};
    constexpr std::array beats {-1.0, 0.0, 3.0, 5.0};
    constexpr std::array seconds {-0.5, 0.0, 1.2, 1.9};

    for (auto i = 0U; i < beats.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_seconds(SightRead::Beat(beats.at(i))).value(),
            seconds.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(beats_to_seconds_conversion_works_correctly_after_speedup)
{
    SightRead::TempoMap tempo_map = SightRead::TempoMap().speedup(200);

    BOOST_CHECK_CLOSE(tempo_map.to_seconds(SightRead::Beat {4}).value(), 1.0,
                      0.0001);
}

BOOST_AUTO_TEST_CASE(beats_to_measures_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {{{SightRead::Tick {0}, 5, 4},
                                    {SightRead::Tick {1000}, 4, 4},
                                    {SightRead::Tick {1200}, 4, 16}},
                                   {},
                                   {},
                                   200};
    constexpr std::array beats {-1.0, 0.0, 3.0, 5.5, 6.5};
    constexpr std::array measures {-0.25, 0.0, 0.6, 1.125, 1.75};

    for (auto i = 0U; i < beats.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_measures(SightRead::Beat(beats.at(i))).value(),
            measures.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(measures_to_beats_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {{{SightRead::Tick {0}, 5, 4},
                                    {SightRead::Tick {1000}, 4, 4},
                                    {SightRead::Tick {1200}, 4, 16}},
                                   {},
                                   {},
                                   200};
    constexpr std::array beats {-1.0, 0.0, 3.0, 5.5, 6.5};
    constexpr std::array measures {-0.25, 0.0, 0.6, 1.125, 1.75};

    for (auto i = 0U; i < beats.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_beats(SightRead::Measure(measures.at(i))).value(),
            beats.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(measures_to_seconds_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 5, 4},
         {SightRead::Tick {1000}, 4, 4},
         {SightRead::Tick {1200}, 4, 16}},
        {{SightRead::Tick {0}, 150000}, {SightRead::Tick {800}, 200000}},
        {},
        200};
    constexpr std::array measures {-0.25, 0.0, 0.6, 1.125, 1.75};
    constexpr std::array seconds {-0.5, 0.0, 1.2, 2.05, 2.35};

    for (auto i = 0U; i < measures.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_seconds(SightRead::Measure(measures.at(i))).value(),
            seconds.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(seconds_to_measures_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {
        {{SightRead::Tick {0}, 5, 4},
         {SightRead::Tick {1000}, 4, 4},
         {SightRead::Tick {1200}, 4, 16}},
        {{SightRead::Tick {0}, 150000}, {SightRead::Tick {800}, 200000}},
        {},
        200};
    constexpr std::array measures {-0.25, 0.0, 0.6, 1.125, 1.75};
    constexpr std::array seconds {-0.5, 0.0, 1.2, 2.05, 2.35};

    for (auto i = 0U; i < measures.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_measures(SightRead::Second(seconds.at(i))).value(),
            measures.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(fretbars_to_beats_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {{{SightRead::Tick {0}, 5, 4},
                                    {SightRead::Tick {1000}, 4, 8},
                                    {SightRead::Tick {1200}, 4, 16}},
                                   {},
                                   {},
                                   200};
    constexpr std::array beats {-1.0, 0.0, 3.0, 5.5, 6.5};
    constexpr std::array fretbars {-1.0, 0.0, 3.0, 6.0, 9.0};

    for (auto i = 0U; i < beats.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_beats(SightRead::Fretbar(fretbars.at(i))).value(),
            beats.at(i), 0.0001);
    }
}

BOOST_AUTO_TEST_CASE(beats_to_fretbars_conversion_works_correctly)
{
    SightRead::TempoMap tempo_map {{{SightRead::Tick {0}, 5, 4},
                                    {SightRead::Tick {1000}, 4, 8},
                                    {SightRead::Tick {1200}, 4, 16}},
                                   {},
                                   {},
                                   200};
    constexpr std::array beats {-1.0, 0.0, 3.0, 5.5, 6.5};
    constexpr std::array fretbars {-1.0, 0.0, 3.0, 6.0, 9.0};

    for (auto i = 0U; i < fretbars.size(); ++i) {
        BOOST_CHECK_CLOSE(
            tempo_map.to_fretbars(SightRead::Beat(beats.at(i))).value(),
            fretbars.at(i), 0.0001);
    }
}
