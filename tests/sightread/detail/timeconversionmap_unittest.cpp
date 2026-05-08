#include <boost/test/unit_test.hpp>

#include "sightread/detail/timeconversionmap.hpp"

BOOST_AUTO_TEST_SUITE(empty_gradient_vector)

BOOST_AUTO_TEST_CASE(value_is_zero_at_zero)
{
    const SightRead::Detail::TimeConversionMap map {10.0, {}};

    BOOST_CHECK_EQUAL(map(0.0), 0.0);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_negative_x)
{
    const SightRead::Detail::TimeConversionMap map {10.0, {}};

    BOOST_CHECK_EQUAL(map(-1.0), -10.0);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_positive_x)
{
    const SightRead::Detail::TimeConversionMap map {10.0, {}};

    BOOST_CHECK_EQUAL(map(1.0), 10.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(single_gradient_change_at_zero)

BOOST_AUTO_TEST_CASE(value_is_zero_at_zero)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(0.0), 0.0);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_negative_x)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(-1.0), -10.0);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_positive_x)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(1.0), 15.0);
}

BOOST_AUTO_TEST_CASE(inverse_is_zero_at_zero)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map.inverse(0.0), 0.0);
}

BOOST_AUTO_TEST_CASE(inverse_is_correct_for_negative_x)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map.inverse(-10.0), -1.0);
}

BOOST_AUTO_TEST_CASE(inverse_is_correct_for_positive_x)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map.inverse(15.0), 1.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(single_gradient_change_after_zero)

BOOST_AUTO_TEST_CASE(value_is_correct_for_x_before_gradients)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 5.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(1.0), 10.0);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_x_after_gradients)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = 5.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(10.0), 125.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(single_gradient_change_before_zero)

BOOST_AUTO_TEST_CASE(value_is_correct_for_x_before_gradients)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0, {{.position = -5.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(-10.0), -125.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(value_is_correct_for_x_after_multiple_gradients)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0,
        {{.position = 0.0, .gradient = 15.0},
         {.position = 1.0, .gradient = 20.0}}};

    BOOST_CHECK_EQUAL(map(2.0), 35.0);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_x_between_multiple_gradients)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0,
        {{.position = 0.0, .gradient = 15.0},
         {.position = 1.0, .gradient = 20.0}}};

    BOOST_CHECK_EQUAL(map(0.5), 7.5);
}

BOOST_AUTO_TEST_CASE(value_is_correct_for_unsorted_gradients)
{
    const SightRead::Detail::TimeConversionMap map {
        10.0,
        {{.position = 1.0, .gradient = 20.0},
         {.position = 0.0, .gradient = 15.0}}};

    BOOST_CHECK_EQUAL(map(0.5), 7.5);
}
