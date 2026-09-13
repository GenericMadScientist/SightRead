#ifndef SIGHTREAD_DETAIL_INTERVALSET_HPP
#define SIGHTREAD_DETAIL_INTERVALSET_HPP

#include <algorithm>
#include <iterator>
#include <tuple>
#include <vector>

template <typename T> struct Interval {
    T start;
    T end;

    [[nodiscard]] bool contains(T position) const
    {
        return start <= position && position < end;
    }
    [[nodiscard]] bool empty() const { return start >= end; }
};

template <typename T> class IntervalSet {
private:
    std::vector<Interval<T>> m_intervals;

public:
    IntervalSet(std::vector<std::tuple<T, T>> intervals)
    {
        std::ranges::sort(intervals);

        for (auto i = intervals.cbegin(); i < intervals.cend();) {
            Interval<T> new_interval {.start = std::get<0>(*i),
                                      .end = std::get<1>(*i)};
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

    [[nodiscard]] bool contains(T position) const
    {
        const auto it = std::ranges::lower_bound(
            m_intervals, position, {},
            [](const auto& interval) { return interval.end; });
        return it != std::ranges::end(m_intervals) && it->contains(position);
    }
};

#endif
