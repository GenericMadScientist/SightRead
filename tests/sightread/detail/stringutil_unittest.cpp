#include <stdexcept>

#include <boost/test/unit_test.hpp>

#include "sightread/detail/stringutil.hpp"

BOOST_AUTO_TEST_CASE(break_off_newline_works_correctly)
{
    std::string_view data = "Hello\nThe\rre\r\nWorld!";

    BOOST_CHECK_EQUAL(SightRead::Detail::break_off_newline(data), "Hello");
    BOOST_CHECK_EQUAL(data, "The\rre\r\nWorld!");
    BOOST_CHECK_EQUAL(SightRead::Detail::break_off_newline(data), "The\rre");
    BOOST_CHECK_EQUAL(data, "World!");
}

BOOST_AUTO_TEST_CASE(skip_whitespace_works_correctly)
{
    BOOST_CHECK_EQUAL(SightRead::Detail::skip_whitespace("Hello"), "Hello");
    BOOST_CHECK_EQUAL(SightRead::Detail::skip_whitespace("  Hello"), "Hello");
    BOOST_CHECK_EQUAL(SightRead::Detail::skip_whitespace("H  ello"), "H  ello");
}

BOOST_AUTO_TEST_CASE(to_utf8_string_strips_utf8_bom)
{
    const std::string text {"\xEF\xBB\xBF\x6E"};

    BOOST_CHECK_EQUAL(SightRead::Detail::to_utf8_string(text), "n");
}

BOOST_AUTO_TEST_CASE(to_utf8_string_treats_no_bom_string_as_utf8)
{
    const std::string text {"Hello"};

    BOOST_CHECK_EQUAL(SightRead::Detail::to_utf8_string(text), "Hello");
}

BOOST_AUTO_TEST_CASE(to_utf8_string_correctly_converts_from_utf16le_to_utf8)
{
    const std::string text {
        "\xFF\xFE\x6E\x00\x61\x00\x6D\x00\x65\x00\x3D\x00\x54\x00\x65\x00\x73"
        "\x00\x74\x00",
        20};

    BOOST_CHECK_EQUAL(SightRead::Detail::to_utf8_string(text), "name=Test");
}

BOOST_AUTO_TEST_CASE(to_utf8_string_correctly_converts_from_latin_1_strings)
{
    const std::string text {"\xE9\x30"};

    BOOST_CHECK_EQUAL(SightRead::Detail::to_utf8_string(text), "é0");
}
