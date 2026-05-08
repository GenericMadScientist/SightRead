#include <algorithm>

#include "sightread/tempomap.hpp"

namespace {
SightRead::Detail::TimeConversionMap
beats_to_seconds_map(const std::vector<SightRead::BPM>& bpms, int resolution)
{
    constexpr double INITIAL_SECONDS_PER_BEAT = 0.5;
    constexpr double MS_PER_MINUTE = 60000;

    std::vector<SightRead::Detail::GradientChange> bpm_gradients;
    bpm_gradients.reserve(bpms.size());
    for (const auto& bpm : bpms) {
        const auto beat
            = static_cast<double>(bpm.position.value()) / resolution;
        const auto seconds_per_beat = MS_PER_MINUTE / bpm.millibeats_per_minute;
        bpm_gradients.push_back(
            {.position = beat, .gradient = seconds_per_beat});
    }

    return {INITIAL_SECONDS_PER_BEAT, bpm_gradients};
}

SightRead::Detail::TimeConversionMap
beats_to_fretbars_map(const std::vector<SightRead::TimeSignature>& time_sigs,
                      int resolution)
{
    std::vector<SightRead::Detail::GradientChange> fretbar_gradients;
    fretbar_gradients.reserve(time_sigs.size());
    for (const auto& ts : time_sigs) {
        const auto beat = static_cast<double>(ts.position.value()) / resolution;
        const auto fretbars_per_beat = ts.denominator / 4.0;
        fretbar_gradients.push_back(
            {.position = beat, .gradient = fretbars_per_beat});
    }

    return {1.0, fretbar_gradients};
}

SightRead::Detail::TimeConversionMap
beats_to_measures_map(const std::vector<SightRead::TimeSignature>& time_sigs,
                      int resolution)
{
    constexpr double DEFAULT_MEASURES_PER_BEAT = 0.25;

    std::vector<SightRead::Detail::GradientChange> measure_gradients;
    measure_gradients.reserve(time_sigs.size());
    for (const auto& ts : time_sigs) {
        const auto beat = static_cast<double>(ts.position.value()) / resolution;
        const auto measures_per_beat
            = ts.denominator * DEFAULT_MEASURES_PER_BEAT / ts.numerator;
        measure_gradients.push_back(
            {.position = beat, .gradient = measures_per_beat});
    }

    return {DEFAULT_MEASURES_PER_BEAT, measure_gradients};
}

SightRead::Detail::TimeConversionMap
beats_to_od_beats_map(const std::vector<SightRead::Tick>& od_beats,
                      int resolution)
{
    constexpr double DEFAULT_OD_BEATS_PER_BEAT = 0.25;

    std::vector<SightRead::Detail::GradientChange> od_beat_gradients;
    od_beat_gradients.reserve(od_beats.size());
    for (auto i = 0U; i < od_beats.size(); ++i) {
        const auto beat
            = static_cast<double>(od_beats.at(i).value()) / resolution;
        const auto od_beat_diff = DEFAULT_OD_BEATS_PER_BEAT;
        auto beat_diff = 1.0;
        if (i + 1 < od_beats.size()) {
            beat_diff = static_cast<double>(
                            (od_beats.at(i + 1) - od_beats.at(i)).value())
                / resolution;
        }
        const auto od_beats_per_beat = od_beat_diff / beat_diff;
        od_beat_gradients.push_back(
            {.position = beat, .gradient = od_beats_per_beat});
    }

    return {DEFAULT_OD_BEATS_PER_BEAT, od_beat_gradients};
}
}

SightRead::TempoMap::TempoMap(std::vector<SightRead::TimeSignature> time_sigs,
                              std::vector<SightRead::BPM> bpms,
                              std::vector<SightRead::Tick> od_beats,
                              int resolution)
    : m_od_beats {std::move(od_beats)}
    , m_resolution {resolution}
    , m_beats_to_seconds {beats_to_seconds_map(bpms, resolution)}
    , m_beats_to_fretbars {beats_to_fretbars_map(time_sigs, resolution)}
    , m_beats_to_measures {beats_to_measures_map(time_sigs, resolution)}
    , m_beats_to_od_beats {beats_to_od_beats_map(m_od_beats, resolution)}
{
    constexpr double DEFAULT_MILLIBEATS_PER_MINUTE = 120000.0;

    if (resolution <= 0) {
        throw std::invalid_argument("Resolution must be positive");
    }

    for (const auto& bpm : bpms) {
        if (bpm.millibeats_per_minute <= 0.0) {
            throw ParseError("BPMs must be positive");
        }
    }
    for (const auto& ts : time_sigs) {
        if (ts.numerator <= 0 || ts.denominator <= 0) {
            throw ParseError("Time signatures must be positive/positive");
        }
    }

    std::ranges::stable_sort(bpms, {},
                             [](const auto& x) { return x.position; });
    BPM prev_bpm {.position = SightRead::Tick {0},
                  .millibeats_per_minute = DEFAULT_MILLIBEATS_PER_MINUTE};
    for (auto p = bpms.cbegin(); p < bpms.cend(); ++p) {
        if (p->position != prev_bpm.position) {
            m_bpms.push_back(prev_bpm);
        }
        prev_bpm = *p;
    }
    m_bpms.push_back(prev_bpm);

    std::ranges::stable_sort(time_sigs, {},
                             [](const auto& ts) { return ts.position; });
    TimeSignature prev_ts {
        .position = SightRead::Tick {0}, .numerator = 4, .denominator = 4};
    for (auto p = time_sigs.cbegin(); p < time_sigs.cend(); ++p) {
        if (p->position != prev_ts.position) {
            m_time_sigs.push_back(prev_ts);
        }
        prev_ts = *p;
    }
    m_time_sigs.push_back(prev_ts);
}

SightRead::TempoMap SightRead::TempoMap::speedup(int speed) const
{
    constexpr auto DEFAULT_SPEED = 100;

    auto bpms = m_bpms;
    for (auto& bpm : bpms) {
        bpm.millibeats_per_minute
            = (bpm.millibeats_per_minute * speed) / DEFAULT_SPEED;
    }

    return {m_time_sigs, std::move(bpms), m_od_beats, m_resolution};
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::Fretbar fretbars) const
{
    return SightRead::Beat {m_beats_to_fretbars.inverse(fretbars.value())};
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::Measure measures) const
{
    return SightRead::Beat {m_beats_to_measures.inverse(measures.value())};
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::OdBeat od_beats) const
{
    return SightRead::Beat {m_beats_to_od_beats.inverse(od_beats.value())};
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::Second seconds) const
{
    return SightRead::Beat {m_beats_to_seconds.inverse(seconds.value())};
}

SightRead::Fretbar SightRead::TempoMap::to_fretbars(SightRead::Beat beats) const
{
    return SightRead::Fretbar {m_beats_to_fretbars(beats.value())};
}

SightRead::Fretbar SightRead::TempoMap::to_fretbars(SightRead::Tick ticks) const
{
    return to_fretbars(to_beats(ticks));
}

SightRead::Measure SightRead::TempoMap::to_measures(SightRead::Beat beats) const
{
    return SightRead::Measure {m_beats_to_measures(beats.value())};
}

SightRead::Measure
SightRead::TempoMap::to_measures(SightRead::Second seconds) const
{
    return to_measures(to_beats(seconds));
}

SightRead::OdBeat SightRead::TempoMap::to_od_beats(SightRead::Beat beats) const
{
    return SightRead::OdBeat {m_beats_to_od_beats(beats.value())};
}

SightRead::Second SightRead::TempoMap::to_seconds(SightRead::Beat beats) const
{
    return SightRead::Second {m_beats_to_seconds(beats.value())};
}

SightRead::Second
SightRead::TempoMap::to_seconds(SightRead::Measure measures) const
{
    return to_seconds(to_beats(measures));
}

SightRead::Second SightRead::TempoMap::to_seconds(SightRead::Tick ticks) const
{
    return to_seconds(to_beats(ticks));
}

SightRead::Tick SightRead::TempoMap::to_ticks(SightRead::Second seconds) const
{
    return to_ticks(to_beats(seconds));
}
