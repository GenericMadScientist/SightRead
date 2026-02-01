#include <tuple>

#include <boost/test/unit_test.hpp>

#include "sightread/detail/midi.hpp"
#include "sightread/tempomap.hpp"

namespace SightRead::Detail {
bool operator==(const MetaEvent& lhs, const MetaEvent& rhs)
{
    return std::tie(lhs.type, lhs.data) == std::tie(rhs.type, rhs.data);
}

std::ostream& operator<<(std::ostream& stream, const MetaEvent& event)
{
    stream << "{Type " << event.type << ", Data {";
    for (auto i = 0U; i < event.data.size(); ++i) {
        stream << event.data.at(i);
        if (i + 1 != event.data.size()) {
            stream << ", ";
        }
    }
    stream << "}}";
    return stream;
}

bool operator==(const MidiEvent& lhs, const MidiEvent& rhs)
{
    return std::tie(lhs.status, lhs.data) == std::tie(rhs.status, rhs.data);
}

std::ostream& operator<<(std::ostream& stream, const MidiEvent& event)
{
    stream << "{Status " << event.status << ", Data {";
    stream << event.data.at(0) << ", " << event.data.at(1) << "}}";
    return stream;
}

bool operator==(const SysexEvent& lhs, const SysexEvent& rhs)
{
    return lhs.data == rhs.data;
}

std::ostream& operator<<(std::ostream& stream, const SysexEvent& event)
{
    stream << "{Data {";
    for (auto i = 0U; i < event.data.size(); ++i) {
        stream << event.data.at(i);
        if (i + 1 != event.data.size()) {
            stream << ", ";
        }
    }
    stream << "}}";
    return stream;
}

bool operator==(const TimedEvent& lhs, const TimedEvent& rhs)
{
    return std::tie(lhs.time, lhs.event) == std::tie(rhs.time, rhs.event);
}

std::ostream& operator<<(std::ostream& stream, const TimedEvent& event)
{
    stream << "{Time " << event.time << ", ";
    if (std::holds_alternative<MetaEvent>(event.event)) {
        stream << "MetaEvent " << std::get<MetaEvent>(event.event);
    } else if (std::holds_alternative<MidiEvent>(event.event)) {
        stream << "MidiEvent " << std::get<MidiEvent>(event.event);
    } else {
        stream << "SysexEvent " << std::get<SysexEvent>(event.event);
    }
    stream << '}';
    return stream;
}
}

namespace {
std::vector<std::uint8_t>
midi_from_tracks(const std::vector<std::vector<std::uint8_t>>& track_sections)
{
    std::vector<std::uint8_t> data {0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1};
    auto count = track_sections.size();
    data.push_back((count >> 8) & 0xFF);
    data.push_back(count & 0xFF);
    data.push_back(1);
    data.push_back(0xE0);
    for (const auto& track : track_sections) {
        for (auto byte : track) {
            data.push_back(byte);
        }
    }
    return data;
}
}

BOOST_AUTO_TEST_CASE(parse_midi_reads_header_correctly)
{
    std::vector<std::uint8_t> data {0x4D, 0x54, 0x68, 0x64, 0, 0, 0,
                                    6,    0,    1,    0,    0, 1, 0xE0};
    std::vector<std::uint8_t> bad_data {0x4D, 0x53, 0x68, 0x64, 0, 0, 0,
                                        6,    0,    1,    0,    0, 1, 0xE0};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL(midi.ticks_per_quarter_note, 0x1E0);
    BOOST_TEST(midi.tracks.empty());
    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(bad_data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(division_must_not_be_in_smpte_format)
{
    std::vector<std::uint8_t> bad_data {0x4D, 0x54, 0x68, 0x64, 0, 0,    0,
                                        6,    0,    1,    0,    0, 0x80, 0};

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(bad_data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(track_lengths_are_read_correctly)
{
    std::vector<std::uint8_t> track_one {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0, 0};
    std::vector<std::uint8_t> track_two {0x4D, 0x54, 0x72, 0x6B, 0,    0,
                                         0,    4,    0,    0x85, 0x60, 0};
    auto data = midi_from_tracks({track_one, track_two});

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL(midi.tracks.size(), 2);
    BOOST_TEST(midi.tracks[0].events.empty());
    BOOST_CHECK_EQUAL(midi.tracks[1].events.size(), 1);
}

BOOST_AUTO_TEST_CASE(track_magic_number_is_checked)
{
    std::vector<std::uint8_t> bad_track {0x40, 0x54, 0x72, 0x6B, 0, 0, 0, 0};
    auto data = midi_from_tracks({bad_track});

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_CASE(extra_tracks_in_header_are_ignored)
{
    std::vector<std::uint8_t> track_one {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0, 0};
    std::vector<std::uint8_t> track_two {0x4D, 0x54, 0x72, 0x6B, 0,    0,
                                         0,    4,    0,    0x85, 0x60, 0};
    auto data = midi_from_tracks({track_one, track_two});
    data[11] = 3;

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL(midi.tracks.size(), 2);
    BOOST_TEST(midi.tracks[0].events.empty());
    BOOST_CHECK_EQUAL(midi.tracks[1].events.size(), 1);
}

BOOST_AUTO_TEST_SUITE(event_times_are_handled_correctly)

BOOST_AUTO_TEST_CASE(multi_byte_delta_times_are_parsed_correctly)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0,
                                     5,    0x8F, 0x10, 0xFF, 2, 0};
    auto data = midi_from_tracks({track});

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL(midi.tracks[0].events[0].time, 0x790);
}

BOOST_AUTO_TEST_CASE(times_are_absolute_not_delta_times)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0,    0, 8,
                                     0x60, 0xFF, 2,    0,    0, 0xFF, 2, 0};
    auto data = midi_from_tracks({track});

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL(midi.tracks[0].events[1].time, 0x60);
}

BOOST_AUTO_TEST_CASE(five_byte_multi_byte_delta_times_throw)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0,    0,    0, 8,
                                     0x8F, 0x8F, 0x8F, 0x8F, 0x10, 0xFF, 2, 0};
    const auto data = midi_from_tracks({track});

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(meta_events_are_read)

BOOST_AUTO_TEST_CASE(simple_meta_event_is_read)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0,    0,   7,
                                     0x60, 0xFF, 0x51, 3,    8, 0x6B, 0xC3};
    auto data = midi_from_tracks({track});
    std::vector<SightRead::Detail::TimedEvent> events {
        {0x60,
         SightRead::Detail::MetaEvent {.type = 0x51, .data = {8, 0x6B, 0xC3}}}};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL_COLLECTIONS(midi.tracks[0].events.cbegin(),
                                  midi.tracks[0].events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(meta_event_with_multi_byte_length_is_read)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0,    8,
                                     0x60, 0xFF, 0x51, 0x80, 3, 8, 0x6B, 0xC3};
    const auto data = midi_from_tracks({track});
    std::vector<SightRead::Detail::TimedEvent> events {
        {0x60,
         SightRead::Detail::MetaEvent {.type = 0x51, .data = {8, 0x6B, 0xC3}}}};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL_COLLECTIONS(midi.tracks[0].events.cbegin(),
                                  midi.tracks[0].events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(too_long_meta_events_throw)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0,    0,
                                     0,    8,    0x60, 0xFF, 0x51, 0x80,
                                     100,  8,    0x6B, 0xC3};
    const auto data = midi_from_tracks({track});

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(midi_events_are_read)

BOOST_AUTO_TEST_CASE(a_single_event_is_read)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0,    0,
                                     0,    4,    0,    0x94, 0x7F, 0x64};
    auto data = midi_from_tracks({track});
    std::vector<SightRead::Detail::TimedEvent> events {
        {0,
         SightRead::Detail::MidiEvent {.status = 0x94, .data = {0x7F, 0x64}}}};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL_COLLECTIONS(midi.tracks[0].events.cbegin(),
                                  midi.tracks[0].events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(running_status_is_parsed)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0,    0,    0,   7,
                                     0,    0x94, 0x7F, 0x64, 0x10, 0x7F, 0x64};
    auto data = midi_from_tracks({track});
    std::vector<SightRead::Detail::TimedEvent> events {
        {0,
         SightRead::Detail::MidiEvent {.status = 0x94, .data = {0x7F, 0x64}}},
        {0x10,
         SightRead::Detail::MidiEvent {.status = 0x94, .data = {0x7F, 0x64}}}};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL_COLLECTIONS(midi.tracks[0].events.cbegin(),
                                  midi.tracks[0].events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(running_status_is_not_stopped_by_meta_events)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0,    0, 0,
                                     11,   0,    0x94, 0x7F, 0x64, 0, 0xFF,
                                     2,    0,    0x10, 0x7F, 0x64};
    auto data = midi_from_tracks({track});

    BOOST_CHECK_NO_THROW([&] { return SightRead::Detail::parse_midi(data); }());
}

BOOST_AUTO_TEST_CASE(running_status_is_not_stopped_by_sysex_events)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0,    0, 0,
                                     11,   0,    0x94, 0x7F, 0x64, 0, 0xF0,
                                     1,    0,    0x10, 0x7F, 0x64};
    auto data = midi_from_tracks({track});

    BOOST_CHECK_NO_THROW([&] { return SightRead::Detail::parse_midi(data); }());
}

BOOST_AUTO_TEST_CASE(not_all_midi_events_take_two_data_bytes)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0,    0,
                                     6,    0,    0xC0, 0,    0, 0xD0, 0};
    auto data = midi_from_tracks({track});

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL(midi.tracks[0].events.size(), 2);
}

BOOST_AUTO_TEST_CASE(midi_events_with_status_byte_high_nibble_f_throw)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0,
                                     0,    4,    0,    0xF0, 0, 0};
    auto data = midi_from_tracks({track});

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(sysex_events_are_read)

BOOST_AUTO_TEST_CASE(simple_sysex_event_is_read)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0,
                                     6,    0x0,  0xF0, 3,    1, 2, 3};
    const auto data = midi_from_tracks({track});
    std::vector<SightRead::Detail::TimedEvent> events {
        {0, SightRead::Detail::SysexEvent {{1, 2, 3}}}};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL_COLLECTIONS(midi.tracks[0].events.cbegin(),
                                  midi.tracks[0].events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(sysex_event_with_multi_byte_length_is_read)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0, 7,
                                     0x0,  0xF0, 0x80, 3,    1, 2, 3};
    const auto data = midi_from_tracks({track});
    std::vector<SightRead::Detail::TimedEvent> events {
        {0, SightRead::Detail::SysexEvent {{1, 2, 3}}}};

    const auto midi = SightRead::Detail::parse_midi(data);

    BOOST_CHECK_EQUAL_COLLECTIONS(midi.tracks[0].events.cbegin(),
                                  midi.tracks[0].events.cend(), events.cbegin(),
                                  events.cend());
}

BOOST_AUTO_TEST_CASE(sysex_event_with_too_high_length_throws)
{
    std::vector<std::uint8_t> track {0x4D, 0x54, 0x72, 0x6B, 0, 0, 0,
                                     6,    0x0,  0xF0, 100,  1, 2, 3};
    const auto data = midi_from_tracks({track});

    BOOST_CHECK_THROW([&] { return SightRead::Detail::parse_midi(data); }(),
                      SightRead::ParseError);
}

BOOST_AUTO_TEST_SUITE_END()
