#ifndef SIGHTREAD_DETAIL_TIMECONVERSIONMAP_HPP
#define SIGHTREAD_DETAIL_TIMECONVERSIONMAP_HPP

#include <vector>

namespace SightRead::Detail {
struct GradientChange {
    double position;
    double gradient;
};

// This class represents a strictly increasing piecewise linear function with
// finitely many changes of gradient. The objective is to preprocess such a
// function to allow for fast evaluation of both the function and its inverse.
// These functions are prevalent in converting between different units of time
// (seconds, beats, measures, etc.).
class TimeConversionMap {
private:
    struct PinnedValue {
        double position;
        double gradient;
        double value;
    };

    double m_initial_gradient;
    std::vector<PinnedValue> m_pinned_values;

public:
    TimeConversionMap(double initial_gradient,
                      const std::vector<GradientChange>& gradient_changes);
    [[nodiscard]] double operator()(double x) const;
    [[nodiscard]] double inverse(double y) const;
};
}

#endif
