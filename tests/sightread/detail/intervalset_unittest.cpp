#include <boost/test/unit_test.hpp>

#include "sightread/detail/intervalset.hpp"

BOOST_AUTO_TEST_CASE(contains_works_correctly_for_empty_set)
{
    IntervalSet set {{}};

    BOOST_TEST(!set.contains(0));
}

BOOST_AUTO_TEST_CASE(contains_works_correctly_for_first_interval)
{
    IntervalSet set {{{1, 5}}};

    BOOST_TEST(set.contains(1));
}

BOOST_AUTO_TEST_CASE(contains_works_correctly_for_second_interval)
{
    IntervalSet set {{{1, 5}, {10, 20}}};

    BOOST_TEST(set.contains(15));
}

BOOST_AUTO_TEST_CASE(contains_intervals_are_half_open)
{
    IntervalSet set {{{1, 5}}};

    BOOST_TEST(!set.contains(15));
}

BOOST_AUTO_TEST_CASE(works_correctly_for_unsorted_intervals)
{
    IntervalSet set {{{10, 20}, {1, 5}}};

    BOOST_TEST(set.contains(1));
}
