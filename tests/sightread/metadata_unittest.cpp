#include <ostream>

#include <boost/test/unit_test.hpp>

#include "sightread/metadata.hpp"

namespace SightRead {
inline std::ostream& operator<<(std::ostream& stream, HopoThresholdType type)
{
    stream << "HopoFrequencyType " << static_cast<int>(type);
    return stream;
}
}

BOOST_AUTO_TEST_CASE(default_ini_values_are_correct)
{
    const auto ini_values = SightRead::parse_ini("");

    BOOST_CHECK_EQUAL(ini_values.name, "Unknown Song");
    BOOST_CHECK_EQUAL(ini_values.artist, "Unknown Artist");
    BOOST_CHECK_EQUAL(ini_values.charter, "Unknown Charter");
    BOOST_CHECK_EQUAL(ini_values.hopo_threshold.threshold_type,
                      SightRead::HopoThresholdType::Resolution);
    BOOST_CHECK(!ini_values.sustain_cutoff_threshold.has_value());
}

BOOST_AUTO_TEST_CASE(values_with_no_spaces_around_equals_are_read)
{
    const char* text
        = "name=Dummy Song\nartist=Dummy Artist\ncharter=Dummy Charter";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.name, "Dummy Song");
    BOOST_CHECK_EQUAL(ini_values.artist, "Dummy Artist");
    BOOST_CHECK_EQUAL(ini_values.charter, "Dummy Charter");
}

BOOST_AUTO_TEST_CASE(values_with_spaces_around_equals_are_read)
{
    const char* text = "name = Dummy Song 2\nartist = Dummy Artist 2\ncharter "
                       "= Dummy Charter 2";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.name, "Dummy Song 2");
    BOOST_CHECK_EQUAL(ini_values.artist, "Dummy Artist 2");
    BOOST_CHECK_EQUAL(ini_values.charter, "Dummy Charter 2");
}

BOOST_AUTO_TEST_CASE(equals_must_be_the_character_after_the_key)
{
    const char* text = "name_length=0\nartist_length=0\ncharter_length=0";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.name, "Unknown Song");
    BOOST_CHECK_EQUAL(ini_values.artist, "Unknown Artist");
    BOOST_CHECK_EQUAL(ini_values.charter, "Unknown Charter");
}

BOOST_AUTO_TEST_CASE(frets_is_a_synonym_for_charter)
{
    const char* text = "frets=GMS";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.charter, "GMS");
}

BOOST_AUTO_TEST_CASE(blank_frets_after_charter_is_ignored)
{
    const char* text = "charter = Haggis\nfrets = ";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.charter, "Haggis");
}

BOOST_AUTO_TEST_CASE(hopo_frequency_is_read)
{
    const char* text = "hopo_frequency = 120";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.hopo_threshold.threshold_type,
                      SightRead::HopoThresholdType::HopoFrequency);
    BOOST_CHECK_EQUAL(ini_values.hopo_threshold.hopo_frequency.value(), 120);
}

BOOST_AUTO_TEST_CASE(eighthnote_hopo_on_is_read)
{
    const char* text = "eighthnote_hopo = 1";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.hopo_threshold.threshold_type,
                      SightRead::HopoThresholdType::EighthNote);
}

BOOST_AUTO_TEST_CASE(eighthnote_hopo_off_is_read)
{
    const char* text = "eighthnote_hopo = 0";

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.hopo_threshold.threshold_type,
                      SightRead::HopoThresholdType::Resolution);
}

BOOST_AUTO_TEST_CASE(sustain_cutoff_threshold_is_read)
{
    const char* text = "sustain_cutoff_threshold = 1";

    const auto ini_values = SightRead::parse_ini(text);

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    BOOST_CHECK_EQUAL(ini_values.sustain_cutoff_threshold.value(), 1);
}

BOOST_AUTO_TEST_CASE(utf16le_inis_are_read_correctly)
{
    const std::string text {
        "\xFF\xFE\x6E\x00\x61\x00\x6D\x00\x65\x00\x3D\x00\x54\x00\x65\x00\x73"
        "\x00\x74\x00",
        20};

    const auto ini_values = SightRead::parse_ini(text);

    BOOST_CHECK_EQUAL(ini_values.name, "Test");
}
