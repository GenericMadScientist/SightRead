#include <boost/test/unit_test.hpp>

#include "sightread/detail/intervalset.hpp"

BOOST_AUTO_TEST_CASE(contains_works_correctly_for_empty_set)
{
    IntervalSet<int> set {{}};

    BOOST_TEST(!set.contains(0));
}

BOOST_AUTO_TEST_CASE(contains_intervals_contain_left_endpoint)
{
    IntervalSet<int> set {{{1, 5}}};

    BOOST_TEST(set.contains(1));
}

BOOST_AUTO_TEST_CASE(contains_intervals_do_not_contain_right_endpoint)
{
    IntervalSet<int> set {{{1, 5}}};

    BOOST_TEST(!set.contains(5));
}

BOOST_AUTO_TEST_CASE(contains_works_correctly_for_second_interval)
{
    IntervalSet<int> set {{{1, 5}, {10, 20}}};

    BOOST_TEST(set.contains(15));
}

BOOST_AUTO_TEST_CASE(works_correctly_for_unsorted_intervals)
{
    IntervalSet<int> set {{{10, 20}, {1, 5}}};

    BOOST_TEST(set.contains(1));
}
