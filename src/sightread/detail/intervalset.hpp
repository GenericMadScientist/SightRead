#ifndef SIGHTREAD_DETAIL_INTERVALSET_HPP
#define SIGHTREAD_DETAIL_INTERVALSET_HPP

#include <algorithm>
#include <iterator>
#include <tuple>
#include <vector>

struct HalfOpenInterval {
    int start;
    int end;

    bool contains(int position) const
    {
        return start <= position && position < end;
    }
    bool empty() const { return start >= end; }
};

class IntervalSet {
private:
    std::vector<HalfOpenInterval> m_intervals;

public:
    IntervalSet(std::vector<std::tuple<int, int>> intervals)
    {
        std::sort(intervals.begin(), intervals.end(),
                  [](const auto& x, const auto& y) {
                      return std::get<0>(x) < std::get<0>(y);
                  });

        for (auto i = intervals.cbegin(); i < intervals.cend();) {
            HalfOpenInterval new_interval {std::get<0>(*i), std::get<1>(*i)};
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

    bool contains(int position) const
    {
        const auto it = std::lower_bound(
            m_intervals.cbegin(), m_intervals.cend(), position,
            [](const auto interval, auto pos) { return pos >= interval.end; });
        return it != m_intervals.cend() && it->start <= position;
    }
};

#endif
