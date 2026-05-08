#include <algorithm>

#include "sightread/detail/timeconversionmap.hpp"

SightRead::Detail::TimeConversionMap::TimeConversionMap(
    double initial_gradient,
    const std::vector<GradientChange>& gradient_changes)
    : m_initial_gradient {initial_gradient}
{
    if (gradient_changes.empty()) {
        return;
    }

    m_pinned_values.reserve(gradient_changes.size());
    for (const auto& change : gradient_changes) {
        m_pinned_values.push_back({.position = change.position,
                                   .gradient = change.gradient,
                                   .value = 0.0});
    }
    std::ranges::stable_sort(m_pinned_values, {}, [](const auto& change) {
        return change.position;
    });

    auto value = m_pinned_values.front().position * m_initial_gradient;
    m_pinned_values.front().value = value;
    for (auto i = 0U; i + 1 < m_pinned_values.size(); ++i) {
        const auto x_diff = m_pinned_values.at(i + 1).position
            - m_pinned_values.at(i).position;
        value += m_pinned_values.at(i).gradient * x_diff;
        m_pinned_values.at(i + 1).value = value;
    }

    const auto value_at_zero = operator()(0.0);
    for (auto& pinned_value : m_pinned_values) {
        pinned_value.value -= value_at_zero;
    }
}

double SightRead::Detail::TimeConversionMap::operator()(double x) const
{
    if (m_pinned_values.empty()) {
        return m_initial_gradient * x;
    }

    const auto next_pinned_value = std::ranges::upper_bound(
        m_pinned_values, x, {},
        [](const auto& value) { return value.position; });
    if (next_pinned_value == std::ranges::begin(m_pinned_values)) {
        return next_pinned_value->value
            + (x - next_pinned_value->position) * m_initial_gradient;
    }

    const auto last_pinned_value = std::prev(next_pinned_value);
    return last_pinned_value->value
        + (x - last_pinned_value->position) * last_pinned_value->gradient;
}

double SightRead::Detail::TimeConversionMap::inverse(double y) const
{
    if (m_pinned_values.empty()) {
        return y / m_initial_gradient;
    }

    const auto next_pinned_value = std::ranges::upper_bound(
        m_pinned_values, y, {}, [](const auto& value) { return value.value; });
    if (next_pinned_value == std::ranges::begin(m_pinned_values)) {
        return next_pinned_value->position
            + (y - next_pinned_value->value) / m_initial_gradient;
    }

    const auto last_pinned_value = std::prev(next_pinned_value);
    return last_pinned_value->position
        + (y - last_pinned_value->value) / last_pinned_value->gradient;
}
