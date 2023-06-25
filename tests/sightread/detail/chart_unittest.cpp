#include <tuple>

#include <boost/test/unit_test.hpp>

#include "sightread/detail/chart.hpp"
#include "sightread/tempomap.hpp"

namespace SightRead::Detail {
bool operator!=(const BpmEvent& lhs, const BpmEvent& rhs)
{
    return std::tie(lhs.position, lhs.bpm) != std::tie(rhs.position, rhs.bpm);
}

std::ostream& operator<<(std::ostream& stream, const BpmEvent& event)
{
    stream << "{Pos " << event.position << ", BPM " << event.bpm << '}';
    return stream;
}

bool operator!=(const Event& lhs, const Event& rhs)
{
    return std::tie(lhs.position, lhs.data) != std::tie(rhs.position, rhs.data);
}

std::ostream& operator<<(std::ostream& stream, const Event& event)
{
    stream << "{Pos " << event.position << ", Data " << event.data << '}';
    return stream;
}

bool operator!=(const NoteEvent& lhs, const NoteEvent& rhs)
{
    return std::tie(lhs.position, lhs.fret, lhs.length)
        != std::tie(rhs.position, rhs.fret, rhs.length);
}

std::ostream& operator<<(std::ostream& stream, const NoteEvent& event)
{
    stream << "{Pos " << event.position << ", Fret " << event.fret << ", Length"
           << event.length << '}';
    return stream;
}

bool operator!=(const SpecialEvent& lhs, const SpecialEvent& rhs)
{
    return std::tie(lhs.position, lhs.key, lhs.length)
        != std::tie(rhs.position, rhs.key, rhs.length);
}

std::ostream& operator<<(std::ostream& stream, const SpecialEvent& event)
{
    stream << "{Pos " << event.position << ", Key " << event.key << ", Length"
           << event.length << '}';
    return stream;
}

bool operator!=(const TimeSigEvent& lhs, const TimeSigEvent& rhs)
{
    return std::tie(lhs.position, lhs.numerator, lhs.denominator)
        != std::tie(rhs.position, rhs.numerator, rhs.denominator);
}

std::ostream& operator<<(std::ostream& stream, const TimeSigEvent& ts)
{
    stream << "{Pos " << ts.position << ", " << ts.numerator << '/'
           << ts.denominator << '}';
    return stream;
}
}

BOOST_AUTO_TEST_CASE(section_names_are_read)
{
    const char* text = "[SectionA]\n{\n}\n[SectionB]\n{\n}\n";

    const auto chart = SightRead::Detail::parse_chart(text);

    BOOST_CHECK_EQUAL(chart.sections.size(), 2);
    BOOST_CHECK_EQUAL(chart.sections[0].name, "SectionA");
    BOOST_CHECK_EQUAL(chart.sections[1].name, "SectionB");
}

BOOST_AUTO_TEST_CASE(chart_can_end_without_newline)
{
    const char* text = "[Song]\n{\n}";

    BOOST_CHECK_NO_THROW(
        [&] { return SightRead::Detail::parse_chart(text); }());
}

BOOST_AUTO_TEST_CASE(parser_does_not_infinite_loop_due_to_unfinished_section)
{
    const char* text = "[UnrecognisedSection]\n{\n";

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_chart(text); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(lone_carriage_return_does_not_break_line)
{
    const char* text = "[Section]\r\n{\r\nKey = Value\rOops\r\n}";

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL(section.key_value_pairs.size(), 1);
    BOOST_CHECK_EQUAL(section.key_value_pairs.at("Key"), "Value\rOops");
}

BOOST_AUTO_TEST_CASE(key_value_pairs_are_read)
{
    const char* text = "[Section]\n{\nKey = Value\nKey2 = Value2\n}";

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL(section.key_value_pairs.size(), 2);
    BOOST_CHECK_EQUAL(section.key_value_pairs.at("Key"), "Value");
    BOOST_CHECK_EQUAL(section.key_value_pairs.at("Key2"), "Value2");
}

BOOST_AUTO_TEST_CASE(note_events_are_read)
{
    const char* text = "[Section]\n{\n1000 = N 1 0\n}";
    const std::vector<SightRead::Detail::NoteEvent> events {{1000, 1, 0}};

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL_COLLECTIONS(section.note_events.cbegin(),
                                  section.note_events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(note_events_with_extra_spaces_throw)
{
    const char* text = "[Section]\n{\n768 = N  0 0\n}";

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_chart(text); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(bpm_events_are_read)
{
    const char* text = "[Section]\n{\n1000 = B 150000\n}";
    const std::vector<SightRead::Detail::BpmEvent> events {{1000, 150000}};

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL_COLLECTIONS(section.bpm_events.cbegin(),
                                  section.bpm_events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(timesig_events_are_read)
{
    const char* text = "[Section]\n{\n1000 = TS 4\n2000 = TS 3 3\n}";
    const std::vector<SightRead::Detail::TimeSigEvent> events {{1000, 4, 2},
                                                               {2000, 3, 3}};

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL_COLLECTIONS(section.ts_events.cbegin(),
                                  section.ts_events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(special_events_are_read)
{
    const char* text = "[Section]\n{\n1000 = S 2 700\n}";
    const std::vector<SightRead::Detail::SpecialEvent> events {{1000, 2, 700}};

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL_COLLECTIONS(section.special_events.cbegin(),
                                  section.special_events.cend(),
                                  events.cbegin(), events.cend());
}

BOOST_AUTO_TEST_CASE(e_events_are_read)
{
    const char* text = "[Section]\n{\n1000 = E soloing\n}";
    const std::vector<SightRead::Detail::Event> events {{1000, "soloing"}};

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_CHECK_EQUAL_COLLECTIONS(section.events.cbegin(),
                                  section.events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(other_events_are_ignored)
{
    const char* text = "[Section]\n{\n1105 = A 133\n}";

    const auto section = SightRead::Detail::parse_chart(text).sections[0];

    BOOST_TEST(section.note_events.empty());
    BOOST_TEST(section.note_events.empty());
    BOOST_TEST(section.bpm_events.empty());
    BOOST_TEST(section.ts_events.empty());
    BOOST_TEST(section.events.empty());
}

BOOST_AUTO_TEST_CASE(single_character_headers_should_throw)
{
    BOOST_CHECK_THROW([] { return SightRead::Detail::parse_chart("\n"); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(short_mid_section_lines_throw)
{
    BOOST_CHECK_THROW(
        [&] {
            return SightRead::Detail::parse_chart("[ExpertGuitar]\n{\n1 1\n}");
        }(),
        SightRead::ParseError);
    BOOST_CHECK_THROW(
        [&] {
            return SightRead::Detail::parse_chart(
                "[ExpertGuitar]\n{\n1 = N 1\n}");
        }(),
        SightRead::ParseError);
}
