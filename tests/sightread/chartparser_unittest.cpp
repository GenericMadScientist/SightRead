#include <boost/test/unit_test.hpp>

#include "sightread/chartparser.hpp"
#include "sightread/detail/chart.hpp"
#include "testhelpers.hpp"

namespace {
std::string section_string(
    const std::string& section_type,
    const std::vector<SightRead::Detail::NoteEvent>& note_events,
    const std::vector<SightRead::Detail::SpecialEvent>& special_events = {},
    const std::vector<SightRead::Detail::Event>& events = {})
{
    using namespace std::string_literals;

    auto section = "["s + section_type + "]\n{\n";
    for (const auto& note : note_events) {
        section += "    ";
        section += std::to_string(note.position);
        section += " = N ";
        section += std::to_string(note.fret);
        section += ' ';
        section += std::to_string(note.length);
        section += '\n';
    }
    for (const auto& event : special_events) {
        section += "    ";
        section += std::to_string(event.position);
        section += " = S ";
        section += std::to_string(event.key);
        section += ' ';
        section += std::to_string(event.length);
        section += '\n';
    }
    for (const auto& event : events) {
        section += "    ";
        section += std::to_string(event.position);
        section += " = E ";
        section += event.data;
        section += '\n';
    }
    section += '}';
    return section;
}

std::string
header_string(const std::map<std::string, std::string>& key_value_pairs)
{
    using namespace std::string_literals;

    auto section = "[Song]\n{\n"s;
    for (const auto& [key, value] : key_value_pairs) {
        section += "    ";
        section += key;
        section += " = ";
        section += value;
        section += '\n';
    }
    section += '}';
    return section;
}

std::string
sync_track_string(const std::vector<SightRead::Detail::BpmEvent>& bpm_events,
                  const std::vector<SightRead::Detail::TimeSigEvent>& ts_events)
{
    using namespace std::string_literals;

    auto section = "[SyncTrack]\n{\n"s;
    for (const auto& bpm : bpm_events) {
        section += "    ";
        section += std::to_string(bpm.position);
        section += " = B ";
        section += std::to_string(bpm.bpm);
        section += '\n';
    }
    for (const auto& ts : ts_events) {
        section += "    ";
        section += std::to_string(ts.position);
        section += " = TS ";
        section += std::to_string(ts.numerator);
        section += ' ';
        section += std::to_string(ts.denominator);
        section += '\n';
    }
    section += '}';
    return section;
}
}

BOOST_AUTO_TEST_CASE(chart_to_song_has_correct_value_for_is_from_midi)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_TEST(!song.global_data().is_from_midi());
}

BOOST_AUTO_TEST_SUITE(chart_reads_resolution)

BOOST_AUTO_TEST_CASE(default_is_192_resolution)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto resolution = global_data.resolution();

    BOOST_CHECK_EQUAL(resolution, 192);
}

BOOST_AUTO_TEST_CASE(default_is_overriden_by_specified_value)
{
    const auto header
        = header_string({{"Resolution", "200"}, {"Offset", "100"}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = header + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto resolution = global_data.resolution();

    BOOST_CHECK_EQUAL(resolution, 200);
}

BOOST_AUTO_TEST_CASE(default_is_overriden_by_specified_value_even_with_bom)
{
    using namespace std::string_literals;

    const auto header
        = header_string({{"Resolution", "200"}, {"Offset", "100"}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = "\xEF\xBB\xBF"s + header + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto resolution = global_data.resolution();

    BOOST_CHECK_EQUAL(resolution, 200);
}

BOOST_AUTO_TEST_CASE(default_is_overriden_by_specified_value_even_with_utf16le)
{
    using namespace std::string_literals;

    const auto header = header_string({{"Resolution", "200"}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto utf8_chart_file = header + '\n' + guitar_track;
    std::vector<char> utf16_chart_bytes {'\xFF', '\xFE'};
    for (auto c : utf8_chart_file) {
        utf16_chart_bytes.push_back(c);
        utf16_chart_bytes.push_back('\0');
    }
    const std::string_view utf16_chart_file {utf16_chart_bytes.data(),
                                             utf16_chart_bytes.size()};

    const auto global_data
        = SightRead::ChartParser({}).parse(utf16_chart_file).global_data();
    const auto resolution = global_data.resolution();

    BOOST_CHECK_EQUAL(resolution, 200);
}

BOOST_AUTO_TEST_CASE(bad_values_are_ignored)
{
    const auto header = header_string({{"Resolution", "\"480\""}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = header + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto resolution = global_data.resolution();

    BOOST_CHECK_EQUAL(resolution, 192);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(practice_mode_sections_are_read)

BOOST_AUTO_TEST_CASE(sections_prefixed_with_section_space_are_read)
{
    using namespace std::string_literals;

    const std::vector<SightRead::PracticeSection> expected_sections {
        {.name = "Start"s, .start = SightRead::Tick {768}}};
    const auto events = "[Events]\n{\n    768 = E \"section Start\"\n}"s;
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = events + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto& practice_sections = global_data.practice_sections();

    BOOST_CHECK_EQUAL_COLLECTIONS(
        practice_sections.cbegin(), practice_sections.cend(),
        expected_sections.cbegin(), expected_sections.cend());
}

BOOST_AUTO_TEST_CASE(sections_prefixed_with_section_underscore_are_read)
{
    using namespace std::string_literals;

    const std::vector<SightRead::PracticeSection> expected_sections {
        {.name = "Start"s, .start = SightRead::Tick {768}}};
    const auto events = "[Events]\n{\n    768 = E \"section_Start\"\n}"s;
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = events + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto& practice_sections = global_data.practice_sections();

    BOOST_CHECK_EQUAL_COLLECTIONS(
        practice_sections.cbegin(), practice_sections.cend(),
        expected_sections.cbegin(), expected_sections.cend());
}

BOOST_AUTO_TEST_CASE(sections_prefixed_with_prc_underscore_are_read)
{
    using namespace std::string_literals;

    const std::vector<SightRead::PracticeSection> expected_sections {
        {.name = "Start"s, .start = SightRead::Tick {768}}};
    const auto events = "[Events]\n{\n    768 = E \"prc_Start\"\n}"s;
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = events + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto& practice_sections = global_data.practice_sections();

    BOOST_CHECK_EQUAL_COLLECTIONS(
        practice_sections.cbegin(), practice_sections.cend(),
        expected_sections.cbegin(), expected_sections.cend());
}

BOOST_AUTO_TEST_CASE(sections_with_other_prefixes_are_ignored)
{
    using namespace std::string_literals;

    const auto events = "[Events]\n{\n    768 = E \"ignored Start\"\n}"s;
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = events + '\n' + guitar_track;

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto& practice_sections = global_data.practice_sections();

    BOOST_TEST(practice_sections.empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(chart_header_values_besides_resolution_are_discarded)
{
    const auto header = header_string({{"Name", "\"TestName\""},
                                       {"Artist", "\"GMS\""},
                                       {"Charter", "\"NotGMS\""}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = header + '\n' + guitar_track;

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_CHECK_NE(song.global_data().name(), "TestName");
    BOOST_CHECK_NE(song.global_data().artist(), "GMS");
    BOOST_CHECK_NE(song.global_data().charter(), "NotGMS");
}

BOOST_AUTO_TEST_CASE(ini_values_are_used_for_converting_from_chart_files)
{
    const auto header = header_string({});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = header + '\n' + guitar_track;
    const SightRead::Metadata metadata {.name = "TestName",
                                        .artist = "GMS",
                                        .charter = "NotGMS",
                                        .hopo_threshold = {}};

    const auto song = SightRead::ChartParser(metadata).parse(chart_file);

    BOOST_CHECK_EQUAL(song.global_data().name(), "TestName");
    BOOST_CHECK_EQUAL(song.global_data().artist(), "GMS");
    BOOST_CHECK_EQUAL(song.global_data().charter(), "NotGMS");
}

BOOST_AUTO_TEST_CASE(chart_reads_sync_track_correctly)
{
    const auto sync_track = sync_track_string(
        {{.position = 0, .bpm = 200000}},
        {{.position = 0, .numerator = 4, .denominator = 2},
         {.position = 768, .numerator = 4, .denominator = 1}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = sync_track + '\n' + guitar_track;

    std::vector<SightRead::TimeSignature> time_sigs {
        {.position = SightRead::Tick {0}, .numerator = 4, .denominator = 4},
        {.position = SightRead::Tick {768}, .numerator = 4, .denominator = 2}};
    std::vector<SightRead::BPM> bpms {
        {.position = SightRead::Tick {0}, .millibeats_per_minute = 200000}};

    const auto global_data
        = SightRead::ChartParser({}).parse(chart_file).global_data();
    const auto& tempo_map = global_data.tempo_map();

    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.time_sigs().cbegin(),
                                  tempo_map.time_sigs().cend(),
                                  time_sigs.cbegin(), time_sigs.cend());
    BOOST_CHECK_EQUAL_COLLECTIONS(tempo_map.bpms().cbegin(),
                                  tempo_map.bpms().cend(), bpms.cbegin(),
                                  bpms.cend());
}

BOOST_AUTO_TEST_CASE(large_time_sig_denominators_cause_an_exception)
{
    const auto sync_track = sync_track_string(
        {}, {{.position = 0, .numerator = 4, .denominator = 32}});
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto chart_file = sync_track + '\n' + guitar_track;

    const SightRead::ChartParser parser {{}};

    BOOST_CHECK_THROW([&] { return parser.parse(chart_file); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(easy_note_track_read_correctly)
{
    const auto chart_file = section_string(
        "EasySingle", {{.position = 768, .fret = 0, .length = 0}},
        {{.position = 768, .key = 2, .length = 100}});
    std::vector<SightRead::Note> notes {
        make_note(768, 0, SightRead::FIVE_FRET_GREEN)};
    std::vector<SightRead::StarPower> sp_phrases {
        {.position = SightRead::Tick {768}, .length = SightRead::Tick {100}}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Guitar,
                                   SightRead::Difficulty::Easy);

    BOOST_CHECK_EQUAL(track.global_data().resolution(), 192);
    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
    BOOST_CHECK_EQUAL_COLLECTIONS(track.sp_phrases().cbegin(),
                                  track.sp_phrases().cend(),
                                  sp_phrases.cbegin(), sp_phrases.cend());
}

BOOST_AUTO_TEST_CASE(invalid_note_values_are_ignored)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 768, .fret = 0, .length = 0},
                          {.position = 768, .fret = 13, .length = 0}});
    std::vector<SightRead::Note> notes {
        make_note(768, 0, SightRead::FIVE_FRET_GREEN)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& parsed_notes = song.track(SightRead::Instrument::Guitar,
                                          SightRead::Difficulty::Expert)
                                   .notes();

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_notes.cbegin(), parsed_notes.cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(non_sp_phrase_special_events_are_ignored)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}},
        {{.position = 768, .key = 1, .length = 100}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_TEST(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .sp_phrases()
            .empty());
}

BOOST_AUTO_TEST_SUITE(chart_does_not_need_sections_in_usual_order)

BOOST_AUTO_TEST_CASE(non_note_sections_need_not_be_present)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});

    const SightRead::ChartParser parser {{}};

    BOOST_CHECK_NO_THROW([&] { return parser.parse(chart_file); }());
}

BOOST_AUTO_TEST_CASE(at_least_one_nonempty_note_section_must_be_present)
{
    const auto chart_file = section_string(
        "ExpertSingle", {}, {{.position = 768, .key = 2, .length = 100}});

    const SightRead::ChartParser parser {{}};

    BOOST_CHECK_THROW([&] { return parser.parse({}); }(),
                      SightRead::ParseError);
    BOOST_CHECK_THROW([&] { return parser.parse(chart_file); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(non_note_sections_can_be_in_any_order)
{
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto sync_track
        = sync_track_string({{.position = 0, .bpm = 200000}}, {});
    const auto header = header_string({{"Resolution", "200"}});
    const auto chart_file = sync_track + '\n' + guitar_track + '\n' + header;

    std::vector<SightRead::Note> notes {make_note(768)};
    std::vector<SightRead::BPM> expected_bpms {
        {.position = SightRead::Tick {0}, .millibeats_per_minute = 200000}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& parsed_notes = song.track(SightRead::Instrument::Guitar,
                                          SightRead::Difficulty::Expert)
                                   .notes();
    const auto bpms = song.global_data().tempo_map().bpms();

    BOOST_CHECK_EQUAL(song.global_data().resolution(), 200);
    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_notes.cbegin(), parsed_notes.cend(),
                                  notes.cbegin(), notes.cend());
    BOOST_CHECK_EQUAL_COLLECTIONS(bpms.cbegin(), bpms.cend(),
                                  expected_bpms.cbegin(), expected_bpms.cend());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(only_first_nonempty_part_of_note_sections_matter)

BOOST_AUTO_TEST_CASE(later_nonempty_sections_are_ignored)
{
    const auto guitar_track_one = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto guitar_track_two = section_string(
        "ExpertSingle", {{.position = 768, .fret = 1, .length = 0}});
    const auto chart_file = guitar_track_one + '\n' + guitar_track_two;
    std::vector<SightRead::Note> notes {
        make_note(768, 0, SightRead::FIVE_FRET_GREEN)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& parsed_notes = song.track(SightRead::Instrument::Guitar,
                                          SightRead::Difficulty::Expert)
                                   .notes();

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_notes.cbegin(), parsed_notes.cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(leading_empty_sections_are_ignored)
{
    const auto guitar_track_one = section_string("ExpertSingle", {});
    const auto guitar_track_two = section_string(
        "ExpertSingle", {{.position = 768, .fret = 1, .length = 0}});
    const auto chart_file = guitar_track_one + '\n' + guitar_track_two;
    std::vector<SightRead::Note> notes {
        make_note(768, 0, SightRead::FIVE_FRET_RED)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& parsed_notes = song.track(SightRead::Instrument::Guitar,
                                          SightRead::Difficulty::Expert)
                                   .notes();

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_notes.cbegin(), parsed_notes.cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(solos_are_read_properly)

BOOST_AUTO_TEST_CASE(expected_solos_are_read_properly)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 100, .fret = 0, .length = 0},
                          {.position = 300, .fret = 0, .length = 0},
                          {.position = 400, .fret = 0, .length = 0}},
                         {},
                         {{.position = 0, .data = "solo"},
                          {.position = 200, .data = "soloend"},
                          {.position = 300, .data = "solo"},
                          {.position = 400, .data = "soloend"}});
    std::vector<SightRead::Solo> required_solos {
        {.start = SightRead::Tick {0},
         .end = SightRead::Tick {201},
         .value = 100},
        {.start = SightRead::Tick {300},
         .end = SightRead::Tick {401},
         .value = 200}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_solos.cbegin(), parsed_solos.cend(),
                                  required_solos.cbegin(),
                                  required_solos.cend());
}

BOOST_AUTO_TEST_CASE(chords_are_not_counted_double)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 100, .fret = 0, .length = 0},
                          {.position = 100, .fret = 1, .length = 0}},
                         {},
                         {{.position = 0, .data = "solo"},
                          {.position = 200, .data = "soloend"}});
    std::vector<SightRead::Solo> required_solos {{.start = SightRead::Tick {0},
                                                  .end = SightRead::Tick {201},
                                                  .value = 100}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_solos.cbegin(), parsed_solos.cend(),
                                  required_solos.cbegin(),
                                  required_solos.cend());
}

BOOST_AUTO_TEST_CASE(empty_solos_are_ignored)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 0, .fret = 0, .length = 0}}, {},
        {{.position = 100, .data = "solo"},
         {.position = 200, .data = "soloend"}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_TEST(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .solos(SightRead::DrumSettings::default_settings())
            .empty());
}

BOOST_AUTO_TEST_CASE(repeated_solo_starts_and_ends_dont_matter)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 100, .fret = 0, .length = 0}}, {},
        {{.position = 0, .data = "solo"},
         {.position = 100, .data = "solo"},
         {.position = 200, .data = "soloend"},
         {.position = 300, .data = "soloend"}});
    std::vector<SightRead::Solo> required_solos {{.start = SightRead::Tick {0},
                                                  .end = SightRead::Tick {201},
                                                  .value = 100}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_solos.cbegin(), parsed_solos.cend(),
                                  required_solos.cbegin(),
                                  required_solos.cend());
}

BOOST_AUTO_TEST_CASE(solo_markers_are_sorted)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 192, .fret = 0, .length = 0}}, {},
        {{.position = 384, .data = "soloend"},
         {.position = 0, .data = "solo"}});
    std::vector<SightRead::Solo> required_solos {{.start = SightRead::Tick {0},
                                                  .end = SightRead::Tick {385},
                                                  .value = 100}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_CHECK_EQUAL_COLLECTIONS(parsed_solos.cbegin(), parsed_solos.cend(),
                                  required_solos.cbegin(),
                                  required_solos.cend());
}

BOOST_AUTO_TEST_CASE(solos_with_no_soloend_event_are_ignored)
{
    const auto chart_file = section_string(
        "ExpertSingle", {{.position = 192, .fret = 0, .length = 0}}, {},
        {{.position = 0, .data = "solo"}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_TEST(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .solos(SightRead::DrumSettings::default_settings())
            .empty());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(other_five_fret_instruments_are_read_from_chart)

BOOST_AUTO_TEST_CASE(guitar_coop_is_read)
{
    const auto chart_file = section_string(
        "ExpertDoubleGuitar", {{.position = 192, .fret = 0, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::GuitarCoop,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_CASE(bass_is_read)
{
    const auto chart_file = section_string(
        "ExpertDoubleBass", {{.position = 192, .fret = 0, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::Bass,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_CASE(rhythm_is_read)
{
    const auto chart_file = section_string(
        "ExpertDoubleRhythm", {{.position = 192, .fret = 0, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::Rhythm,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_CASE(keys_is_read)
{
    const auto chart_file = section_string(
        "ExpertKeyboard", {{.position = 192, .fret = 0, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);

    BOOST_CHECK_NO_THROW([&] {
        return song.track(SightRead::Instrument::Keys,
                          SightRead::Difficulty::Expert);
    }());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(six_fret_instruments_are_read_correctly_from_chart)

BOOST_AUTO_TEST_CASE(six_fret_guitar_is_read_correctly)
{
    const auto chart_file
        = section_string("ExpertGHLGuitar",
                         {{.position = 192, .fret = 0, .length = 0},
                          {.position = 384, .fret = 3, .length = 0}});
    const std::vector<SightRead::Note> notes {
        make_ghl_note(192, 0, SightRead::SIX_FRET_WHITE_LOW),
        make_ghl_note(384, 0, SightRead::SIX_FRET_BLACK_LOW)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::GHLGuitar,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(six_fret_bass_is_read_correctly)
{
    const auto chart_file
        = section_string("ExpertGHLBass",
                         {{.position = 192, .fret = 0, .length = 0},
                          {.position = 384, .fret = 3, .length = 0}});
    const std::vector<SightRead::Note> notes {
        make_ghl_note(192, 0, SightRead::SIX_FRET_WHITE_LOW),
        make_ghl_note(384, 0, SightRead::SIX_FRET_BLACK_LOW)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::GHLBass,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(six_fret_rhythm_is_read_correctly)
{
    const auto chart_file
        = section_string("ExpertGHLRhythm",
                         {{.position = 192, .fret = 0, .length = 0},
                          {.position = 384, .fret = 3, .length = 0}});
    const std::vector<SightRead::Note> notes {
        make_ghl_note(192, 0, SightRead::SIX_FRET_WHITE_LOW),
        make_ghl_note(384, 0, SightRead::SIX_FRET_BLACK_LOW)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::GHLRhythm,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(six_fret_guitar_coop_is_read_correctly)
{
    const auto chart_file
        = section_string("ExpertGHLCoop",
                         {{.position = 192, .fret = 0, .length = 0},
                          {.position = 384, .fret = 3, .length = 0}});
    const std::vector<SightRead::Note> notes {
        make_ghl_note(192, 0, SightRead::SIX_FRET_WHITE_LOW),
        make_ghl_note(384, 0, SightRead::SIX_FRET_BLACK_LOW)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::GHLGuitarCoop,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(drum_notes_are_read_correctly_from_chart)
{
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 192, .fret = 1, .length = 0},
                          {.position = 384, .fret = 2, .length = 0},
                          {.position = 384, .fret = 66, .length = 0},
                          {.position = 768, .fret = 32, .length = 0}});
    std::vector<SightRead::Note> notes {
        make_drum_note(192, SightRead::DRUM_RED),
        make_drum_note(384, SightRead::DRUM_YELLOW, SightRead::FLAGS_CYMBAL),
        make_drum_note(768, SightRead::DRUM_DOUBLE_KICK)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(cymbal_events_do_not_override_lengths)
{
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 192, .fret = 3, .length = 12},
                          {.position = 192, .fret = 67, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL(track.notes().front().lengths.at(2),
                      SightRead::Tick {12});
}

BOOST_AUTO_TEST_CASE(dynamics_are_read_correctly_from_chart)
{
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 192, .fret = 1, .length = 0},
                          {.position = 192, .fret = 34, .length = 0},
                          {.position = 384, .fret = 1, .length = 0},
                          {.position = 384, .fret = 40, .length = 0}});
    std::vector<SightRead::Note> notes {
        make_drum_note(192, SightRead::DRUM_RED, SightRead::FLAGS_ACCENT),
        make_drum_note(384, SightRead::DRUM_RED, SightRead::FLAGS_GHOST)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(drum_solos_are_read_correctly_from_chart)
{
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 192, .fret = 1, .length = 0},
                          {.position = 192, .fret = 2, .length = 0}},
                         {},
                         {{.position = 0, .data = "solo"},
                          {.position = 200, .data = "soloend"}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL(
        track.solos(SightRead::DrumSettings::default_settings()).at(0).value,
        200);
}

BOOST_AUTO_TEST_CASE(fifth_lane_notes_are_read_correctly_from_chart)
{
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 192, .fret = 5, .length = 0},
                          {.position = 384, .fret = 4, .length = 0},
                          {.position = 384, .fret = 5, .length = 0}});
    const std::vector<SightRead::Note> notes {
        make_drum_note(192, SightRead::DRUM_GREEN),
        make_drum_note(384, SightRead::DRUM_GREEN),
        make_drum_note(384, SightRead::DRUM_BLUE)};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL_COLLECTIONS(track.notes().cbegin(), track.notes().cend(),
                                  notes.cbegin(), notes.cend());
}

BOOST_AUTO_TEST_CASE(invalid_drum_notes_are_ignored)
{
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 192, .fret = 1, .length = 0},
                          {.position = 192, .fret = 6, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL(track.notes().size(), 1U);
}

BOOST_AUTO_TEST_CASE(drum_fills_are_read_from_chart)
{
    const auto chart_file = section_string(
        "ExpertDrums", {{.position = 192, .fret = 1, .length = 0}},
        {{.position = 192, .key = 64, .length = 1}});
    const SightRead::DrumFill fill {.position = SightRead::Tick {192},
                                    .length = SightRead::Tick {1}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL(track.drum_fills().size(), 1U);
    BOOST_CHECK_EQUAL(track.drum_fills().at(0), fill);
}

BOOST_AUTO_TEST_SUITE(disco_flips)

BOOST_AUTO_TEST_CASE(disco_flips_without_brackets_are_read_from_chart)
{
    const auto chart_file = section_string(
        "ExpertDrums", {{.position = 192, .fret = 1, .length = 0}}, {},
        {{.position = 100, .data = "mix_3_drums0d"},
         {.position = 105, .data = "mix_3_drums0"}});
    const SightRead::DiscoFlip flip {.position = SightRead::Tick {100},
                                     .length = SightRead::Tick {5}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL(track.disco_flips().size(), 1U);
    BOOST_CHECK_EQUAL(track.disco_flips().at(0), flip);
}

BOOST_AUTO_TEST_CASE(disco_flips_with_brackets_are_read_from_chart)
{
    const auto chart_file = section_string(
        "ExpertDrums", {{.position = 192, .fret = 1, .length = 0}}, {},
        {{.position = 100, .data = "[mix_3_drums0d]"},
         {.position = 105, .data = "[mix_3_drums0]"}});
    const SightRead::DiscoFlip flip {.position = SightRead::Tick {100},
                                     .length = SightRead::Tick {5}};

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto& track = song.track(SightRead::Instrument::Drums,
                                   SightRead::Difficulty::Expert);

    BOOST_CHECK_EQUAL(track.disco_flips().size(), 1U);
    BOOST_CHECK_EQUAL(track.disco_flips().at(0), flip);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(instruments_not_permitted_are_dropped_from_charts)
{
    const auto guitar_track = section_string(
        "ExpertSingle", {{.position = 768, .fret = 0, .length = 0}});
    const auto bass_track = section_string(
        "ExpertDoubleBass", {{.position = 192, .fret = 0, .length = 0}});
    const auto chart_file = guitar_track + '\n' + bass_track;
    const std::vector<SightRead::Instrument> expected_instruments {
        SightRead::Instrument::Guitar};

    const auto parser = SightRead::ChartParser({}).permit_instruments(
        {SightRead::Instrument::Guitar});
    const auto song = parser.parse(chart_file);
    const auto instruments = song.instruments();

    BOOST_CHECK_EQUAL_COLLECTIONS(instruments.cbegin(), instruments.cend(),
                                  expected_instruments.cbegin(),
                                  expected_instruments.cend());
}

BOOST_AUTO_TEST_CASE(solos_ignored_from_charts_if_not_permitted)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 100, .fret = 0, .length = 0},
                          {.position = 300, .fret = 0, .length = 0},
                          {.position = 400, .fret = 0, .length = 0}},
                         {},
                         {{.position = 0, .data = "solo"},
                          {.position = 200, .data = "soloend"},
                          {.position = 300, .data = "solo"},
                          {.position = 400, .data = "soloend"}});

    const auto parser = SightRead::ChartParser({}).parse_solos(false);
    const auto song = parser.parse(chart_file);
    const auto parsed_solos
        = song.track(SightRead::Instrument::Guitar,
                     SightRead::Difficulty::Expert)
              .solos(SightRead::DrumSettings::default_settings());

    BOOST_TEST(parsed_solos.empty());
}

BOOST_AUTO_TEST_CASE(open_chords_disallowed_by_default)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 100, .fret = 0, .length = 0},
                          {.position = 100, .fret = 7, .length = 0}},
                         {}, {});

    const auto parser = SightRead::ChartParser({});
    const auto song = parser.parse(chart_file);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .front()
            .colours(),
        32);
}

BOOST_AUTO_TEST_CASE(open_chords_allowed_if_allow_open_chords_set)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 100, .fret = 0, .length = 0},
                          {.position = 100, .fret = 7, .length = 0}},
                         {}, {});

    const auto parser = SightRead::ChartParser({}).allow_open_chords(true);
    const auto song = parser.parse(chart_file);

    BOOST_CHECK_EQUAL(
        song.track(SightRead::Instrument::Guitar, SightRead::Difficulty::Expert)
            .notes()
            .front()
            .colours(),
        33);
}

BOOST_AUTO_TEST_SUITE(chart_hopos_and_taps)

BOOST_AUTO_TEST_CASE(automatically_set_based_on_distance)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 65, .fret = 1, .length = 0},
                          {.position = 131, .fret = 2, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
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
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 65, .fret = 0, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(forcing_is_handled_correctly)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 0, .fret = 5, .length = 0},
                          {.position = 65, .fret = 1, .length = 0},
                          {.position = 65, .fret = 5, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags,
                      SightRead::FLAGS_FORCE_FLIP | SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_FORCE_FLIP
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(chords_are_not_hopos_due_to_proximity)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 64, .fret = 1, .length = 0},
                          {.position = 64, .fret = 2, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags, SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(chords_can_be_forced)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 64, .fret = 1, .length = 0},
                          {.position = 64, .fret = 2, .length = 0},
                          {.position = 64, .fret = 5, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_FORCE_FLIP | SightRead::FLAGS_HOPO
                          | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(taps_are_read)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 0, .fret = 6, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags,
                      SightRead::FLAGS_TAP | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(taps_take_precedence_over_hopos)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 64, .fret = 1, .length = 0},
                          {.position = 64, .fret = 6, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_TAP | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(chords_can_be_taps)
{
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 64, .fret = 1, .length = 0},
                          {.position = 64, .fret = 2, .length = 0},
                          {.position = 64, .fret = 6, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Guitar,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(1).flags,
                      SightRead::FLAGS_TAP | SightRead::FLAGS_FIVE_FRET_GUITAR);
}

BOOST_AUTO_TEST_CASE(other_resolutions_are_handled_correctly)
{
    const auto header = header_string({{"Resolution", "480"}});
    const auto guitar_track
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 162, .fret = 1, .length = 0},
                          {.position = 325, .fret = 2, .length = 0}});
    const auto chart_file = header + '\n' + guitar_track;

    const auto song = SightRead::ChartParser({}).parse(chart_file);
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
    const auto chart_file
        = section_string("ExpertSingle",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 65, .fret = 1, .length = 0},
                          {.position = 131, .fret = 2, .length = 0}});

    const auto song
        = SightRead::ChartParser(
              {.name = "",
               .artist = "",
               .charter = "",
               .hopo_threshold
               = {.threshold_type = SightRead::HopoThresholdType::HopoFrequency,
                  .hopo_frequency = SightRead::Tick {96}}})
              .parse(chart_file);
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
    const auto chart_file
        = section_string("ExpertDrums",
                         {{.position = 0, .fret = 0, .length = 0},
                          {.position = 65, .fret = 1, .length = 0},
                          {.position = 131, .fret = 2, .length = 0},
                          {.position = 131, .fret = 6, .length = 0}});

    const auto song = SightRead::ChartParser({}).parse(chart_file);
    const auto notes = song.track(SightRead::Instrument::Drums,
                                  SightRead::Difficulty::Expert)
                           .notes();

    BOOST_CHECK_EQUAL(notes.at(0).flags, SightRead::FLAGS_DRUMS);
    BOOST_CHECK_EQUAL(notes.at(1).flags, SightRead::FLAGS_DRUMS);
    BOOST_CHECK_EQUAL(notes.at(2).flags, SightRead::FLAGS_DRUMS);
}

BOOST_AUTO_TEST_SUITE_END()
