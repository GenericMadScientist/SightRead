#include <algorithm>

#include "sightread/tempomap.hpp"

SightRead::TempoMap::TempoMap(std::vector<SightRead::TimeSignature> time_sigs,
                              std::vector<SightRead::BPM> bpms,
                              std::vector<SightRead::Tick> od_beats,
                              int resolution)
    : m_od_beats {std::move(od_beats)}
    , m_resolution {resolution}
{
    constexpr double DEFAULT_TIMESIG_DENOM = 4.0;
    constexpr double MS_PER_MINUTE = 60000.0;

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
                             [](const auto& x) { return x.position; });
    TimeSignature prev_ts {
        .position = SightRead::Tick {0}, .numerator = 4, .denominator = 4};
    for (auto p = time_sigs.cbegin(); p < time_sigs.cend(); ++p) {
        if (p->position != prev_ts.position) {
            m_time_sigs.push_back(prev_ts);
        }
        prev_ts = *p;
    }
    m_time_sigs.push_back(prev_ts);

    SightRead::Tick last_tick {0};
    auto last_bpm = DEFAULT_MILLIBEATS_PER_MINUTE;
    auto last_time = 0.0;

    for (const auto& bpm : m_bpms) {
        last_time += to_beats(bpm.position - last_tick).value()
            * (MS_PER_MINUTE / last_bpm);
        const auto beat = to_beats(bpm.position);
        m_beat_timestamps.push_back({beat, SightRead::Second(last_time)});
        last_bpm = bpm.millibeats_per_minute;
        last_tick = bpm.position;
    }

    m_last_bpm = last_bpm;

    last_tick = SightRead::Tick {0};
    auto last_beat_rate = DEFAULT_BEAT_RATE;
    auto last_fretbar_rate = DEFAULT_FRETBAR_RATE;
    auto last_fretbar = 0.0;
    auto last_measure = 0.0;

    for (const auto& ts : m_time_sigs) {
        const auto beat_increment = to_beats(ts.position - last_tick).value();
        last_fretbar += beat_increment * last_fretbar_rate;
        last_measure += beat_increment / last_beat_rate;
        const auto beat = to_beats(ts.position);
        m_fretbar_timestamps.push_back(
            {SightRead::Fretbar(last_fretbar), beat});
        m_measure_timestamps.push_back(
            {SightRead::Measure(last_measure), beat});
        last_beat_rate = (ts.numerator * DEFAULT_BEAT_RATE) / ts.denominator;
        last_fretbar_rate = ts.denominator / DEFAULT_TIMESIG_DENOM;
        last_tick = ts.position;
    }

    m_last_beat_rate = last_beat_rate;
    m_last_fretbar_rate = last_fretbar_rate;

    if (!m_od_beats.empty()) {
        for (auto i = 0U; i < m_od_beats.size(); ++i) {
            const auto beat = to_beats(m_od_beats[i]);
            m_od_beat_timestamps.push_back(
                {SightRead::OdBeat(i / DEFAULT_BEAT_RATE), beat});
        }
    } else {
        m_od_beat_timestamps.push_back(
            {SightRead::OdBeat(0.0), SightRead::Beat(0.0)});
    }

    m_last_od_beat_rate = DEFAULT_BEAT_RATE;
}

SightRead::TempoMap SightRead::TempoMap::speedup(int speed) const
{
    constexpr auto DEFAULT_SPEED = 100;

    SightRead::TempoMap speedup {m_time_sigs, m_bpms, m_od_beats, m_resolution};
    for (auto& bpm : speedup.m_bpms) {
        bpm.millibeats_per_minute
            = (bpm.millibeats_per_minute * speed) / DEFAULT_SPEED;
    }

    const auto timestamp_factor = DEFAULT_SPEED / static_cast<double>(speed);
    for (auto& timestamp : speedup.m_beat_timestamps) {
        timestamp.time *= timestamp_factor;
    }
    speedup.m_last_bpm = (speedup.m_last_bpm * speed) / DEFAULT_SPEED;

    return speedup;
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::Fretbar fretbars) const
{
    const auto pos = std::ranges::lower_bound(
        m_fretbar_timestamps, fretbars,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.fretbar; });
    if (pos == m_fretbar_timestamps.cend()) {
        const auto& back = m_fretbar_timestamps.back();
        return back.beat
            + (fretbars - back.fretbar).to_beat(m_last_fretbar_rate);
    }
    if (pos == m_fretbar_timestamps.cbegin()) {
        return pos->beat
            - (pos->fretbar - fretbars).to_beat(DEFAULT_FRETBAR_RATE);
    }
    const auto prev = pos - 1;
    return prev->beat
        + (pos->beat - prev->beat)
        * ((fretbars - prev->fretbar) / (pos->fretbar - prev->fretbar));
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::Measure measures) const
{
    const auto pos = std::ranges::lower_bound(
        m_measure_timestamps, measures,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.measure; });
    if (pos == m_measure_timestamps.cend()) {
        const auto& back = m_measure_timestamps.back();
        return back.beat + (measures - back.measure).to_beat(m_last_beat_rate);
    }
    if (pos == m_measure_timestamps.cbegin()) {
        return pos->beat - (pos->measure - measures).to_beat(DEFAULT_BEAT_RATE);
    }
    const auto prev = pos - 1;
    return prev->beat
        + (pos->beat - prev->beat)
        * ((measures - prev->measure) / (pos->measure - prev->measure));
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::OdBeat od_beats) const
{
    const auto pos = std::ranges::lower_bound(
        m_od_beat_timestamps, od_beats,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.od_beat; });
    if (pos == m_od_beat_timestamps.cend()) {
        const auto& back = m_od_beat_timestamps.back();
        return back.beat
            + (od_beats - back.od_beat).to_beat(m_last_od_beat_rate);
    }
    if (pos == m_od_beat_timestamps.cbegin()) {
        return pos->beat - (pos->od_beat - od_beats).to_beat(DEFAULT_BEAT_RATE);
    }
    const auto prev = pos - 1;
    return prev->beat
        + (pos->beat - prev->beat)
        * ((od_beats - prev->od_beat) / (pos->od_beat - prev->od_beat));
}

SightRead::Beat SightRead::TempoMap::to_beats(SightRead::Second seconds) const
{
    const auto pos = std::ranges::lower_bound(
        m_beat_timestamps, seconds,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.time; });
    if (pos == m_beat_timestamps.cend()) {
        const auto& back = m_beat_timestamps.back();
        return back.beat + (seconds - back.time).to_beat(m_last_bpm);
    }
    if (pos == m_beat_timestamps.cbegin()) {
        return pos->beat
            - (pos->time - seconds).to_beat(DEFAULT_MILLIBEATS_PER_MINUTE);
    }
    const auto prev = pos - 1;
    return prev->beat
        + (pos->beat - prev->beat)
        * ((seconds - prev->time) / (pos->time - prev->time));
}

SightRead::Fretbar SightRead::TempoMap::to_fretbars(SightRead::Beat beats) const
{
    const auto pos = std::ranges::lower_bound(
        m_fretbar_timestamps, beats,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.beat; });
    if (pos == m_fretbar_timestamps.cend()) {
        const auto& back = m_fretbar_timestamps.back();
        return back.fretbar
            + (beats - back.beat).to_fretbar(m_last_fretbar_rate);
    }
    if (pos == m_fretbar_timestamps.cbegin()) {
        return pos->fretbar
            - (pos->beat - beats).to_fretbar(DEFAULT_FRETBAR_RATE);
    }
    const auto prev = pos - 1;
    return prev->fretbar
        + (pos->fretbar - prev->fretbar)
        * ((beats - prev->beat) / (pos->beat - prev->beat));
}

SightRead::Fretbar SightRead::TempoMap::to_fretbars(SightRead::Tick ticks) const
{
    return to_fretbars(to_beats(ticks));
}

SightRead::Measure SightRead::TempoMap::to_measures(SightRead::Beat beats) const
{
    const auto pos = std::ranges::lower_bound(
        m_measure_timestamps, beats,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.beat; });
    if (pos == m_measure_timestamps.cend()) {
        const auto& back = m_measure_timestamps.back();
        return back.measure + (beats - back.beat).to_measure(m_last_beat_rate);
    }
    if (pos == m_measure_timestamps.cbegin()) {
        return pos->measure - (pos->beat - beats).to_measure(DEFAULT_BEAT_RATE);
    }
    const auto prev = pos - 1;
    return prev->measure
        + (pos->measure - prev->measure)
        * ((beats - prev->beat) / (pos->beat - prev->beat));
}

SightRead::Measure
SightRead::TempoMap::to_measures(SightRead::Second seconds) const
{
    return to_measures(to_beats(seconds));
}

SightRead::OdBeat SightRead::TempoMap::to_od_beats(SightRead::Beat beats) const
{
    const auto pos = std::ranges::lower_bound(
        m_od_beat_timestamps, beats,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.beat; });
    if (pos == m_od_beat_timestamps.cend()) {
        const auto& back = m_od_beat_timestamps.back();
        return back.od_beat
            + SightRead::OdBeat(
                   (beats - back.beat).to_measure(m_last_od_beat_rate).value());
    }
    if (pos == m_od_beat_timestamps.cbegin()) {
        return pos->od_beat
            - SightRead::OdBeat(
                   (pos->beat - beats).to_measure(DEFAULT_BEAT_RATE).value());
    }
    const auto prev = pos - 1;
    return prev->od_beat
        + (pos->od_beat - prev->od_beat)
        * ((beats - prev->beat) / (pos->beat - prev->beat));
}

SightRead::Second SightRead::TempoMap::to_seconds(SightRead::Beat beats) const
{
    const auto pos = std::ranges::lower_bound(
        m_beat_timestamps, beats,
        [](const auto& x, const auto& y) { return x < y; },
        [](const auto& x) { return x.beat; });
    if (pos == m_beat_timestamps.cend()) {
        const auto& back = m_beat_timestamps.back();
        return back.time + (beats - back.beat).to_second(m_last_bpm);
    }
    if (pos == m_beat_timestamps.cbegin()) {
        return pos->time
            - (pos->beat - beats).to_second(DEFAULT_MILLIBEATS_PER_MINUTE);
    }
    const auto prev = pos - 1;
    return prev->time
        + (pos->time - prev->time)
        * ((beats - prev->beat) / (pos->beat - prev->beat));
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
