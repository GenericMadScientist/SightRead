#ifndef SIGHTREAD_DETAIL_INTERVALSET_HPP
#define SIGHTREAD_DETAIL_INTERVALSET_HPP

#include <algorithm>
#include <iterator>
#include <tuple>
#include <vector>

template <typename T> struct ClosedInterval {
    T start;
    T end;

    using value_type = T;

    [[nodiscard]] bool contains(T position) const
    {
        return start <= position && position <= end;
    }
    [[nodiscard]] bool empty() const { return start > end; }
};

template <typename T> struct HalfOpenInterval {
    T start;
    T end;

    using value_type = T;

    [[nodiscard]] bool contains(T position) const
    {
        return start <= position && position < end;
    }
    [[nodiscard]] bool empty() const { return start >= end; }
};

template <typename I> class IntervalSet {
private:
    std::vector<I> m_intervals;

public:
    IntervalSet(
        std::vector<std::tuple<typename I::value_type, typename I::value_type>>
            intervals)
    {
        std::ranges::sort(intervals);

        for (auto i = intervals.cbegin(); i < intervals.cend();) {
            I new_interval {.start = std::get<0>(*i), .end = std::get<1>(*i)};
            auto j = std::next(i);
            for (; j < intervals.cend() && std::get<0>(*j) <= new_interval.end;
                 ++j) {
                new_interval.end = std::max(new_interval.end, std::get<1>(*j));
            }
            if (!new_interval.empty()) {
                m_intervals.push_back(new_interval);
            }
            i = j;
        }
    }

    [[nodiscard]] bool contains(I::value_type position) const
    {
        const auto it = std::ranges::lower_bound(
            m_intervals, position, {},
            [](const auto& interval) { return interval.end; });
        return it != std::ranges::end(m_intervals) && it->contains(position);
    }
};

template <typename T>
using HalfOpenIntervalSet = IntervalSet<HalfOpenInterval<T>>;

template <typename T> using ClosedIntervalSet = IntervalSet<ClosedInterval<T>>;

#endif
