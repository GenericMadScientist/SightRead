#ifndef SIGHTREAD_TEMPOMAP_HPP
#define SIGHTREAD_TEMPOMAP_HPP

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "sightread/time.hpp"

namespace SightRead {
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const char* what)
        : std::runtime_error {what}
    {
    }

    explicit ParseError(const std::string& what)
        : std::runtime_error {what}
    {
    }
};

struct TimeSignature {
    SightRead::Tick position;
    int numerator;
    int denominator;
};

struct BPM {
    SightRead::Tick position;
    // Larger int type is needed to handle speedups.
    std::int64_t bpm;
};

// Invariants:
// bpms() are sorted by position.
// bpms() never has two BPMs with the same position.
// bpms() is never empty.
// time_sigs() are sorted by position.
// time_sigs() never has two TimeSignatures with the same position.
// time_sigs() is never empty.
class TempoMap {
private:
    struct BeatTimestamp {
        SightRead::Beat beat;
        SightRead::Second time;
    };

    struct FretbarTimestamp {
        SightRead::Fretbar fretbar;
        SightRead::Beat beat;
    };

    struct MeasureTimestamp {
        SightRead::Measure measure;
        SightRead::Beat beat;
    };

    struct OdBeatTimestamp {
        SightRead::OdBeat od_beat;
        SightRead::Beat beat;
    };

    static constexpr double DEFAULT_BEAT_RATE = 4.0;
    static constexpr std::int64_t DEFAULT_BPM = 120000;
    static constexpr double DEFAULT_FRETBAR_RATE = 1.0;
    static constexpr int DEFAULT_RESOLUTION = 192;

    std::vector<TimeSignature> m_time_sigs;
    std::vector<BPM> m_bpms;
    std::vector<SightRead::Tick> m_od_beats;
    int m_resolution;

    std::vector<BeatTimestamp> m_beat_timestamps;
    std::int64_t m_last_bpm;

    std::vector<FretbarTimestamp> m_fretbar_timestamps;
    double m_last_fretbar_rate;

    std::vector<MeasureTimestamp> m_measure_timestamps;
    double m_last_beat_rate;

    std::vector<OdBeatTimestamp> m_od_beat_timestamps;
    double m_last_od_beat_rate;

public:
    TempoMap()
        : TempoMap({}, {}, {}, DEFAULT_RESOLUTION)
    {
    }
    TempoMap(std::vector<TimeSignature> time_sigs, std::vector<BPM> bpms,
             std::vector<SightRead::Tick> od_beats, int resolution);
    [[nodiscard]] const std::vector<TimeSignature>& time_sigs() const
    {
        return m_time_sigs;
    }
    [[nodiscard]] const std::vector<BPM>& bpms() const { return m_bpms; }

    // Return the TempoMap for a speedup of speed% (normal speed is 100).
    [[nodiscard]] TempoMap speedup(int speed) const;

    [[nodiscard]] SightRead::Beat to_beats(SightRead::Fretbar fretbars) const;
    [[nodiscard]] SightRead::Beat to_beats(SightRead::Measure measures) const;
    [[nodiscard]] SightRead::Beat to_beats(SightRead::OdBeat od_beats) const;
    [[nodiscard]] SightRead::Beat to_beats(SightRead::Second seconds) const;
    [[nodiscard]] SightRead::Beat to_beats(SightRead::Tick ticks) const
    {
        return SightRead::Beat {ticks.value()
                                / static_cast<double>(m_resolution)};
    }

    [[nodiscard]] SightRead::Fretbar to_fretbars(SightRead::Beat beats) const;
    [[nodiscard]] SightRead::Fretbar to_fretbars(SightRead::Tick ticks) const;

    [[nodiscard]] SightRead::Measure to_measures(SightRead::Beat beats) const;
    [[nodiscard]] SightRead::Measure
    to_measures(SightRead::Second seconds) const;

    [[nodiscard]] SightRead::OdBeat to_od_beats(SightRead::Beat beats) const;

    [[nodiscard]] SightRead::Second to_seconds(SightRead::Beat beats) const;
    [[nodiscard]] SightRead::Second
    to_seconds(SightRead::Measure measures) const;
    [[nodiscard]] SightRead::Second to_seconds(SightRead::Tick ticks) const;

    [[nodiscard]] SightRead::Tick to_ticks(SightRead::Beat beats) const
    {
        return SightRead::Tick {static_cast<int>(beats.value() * m_resolution)};
    }
    [[nodiscard]] SightRead::Tick to_ticks(SightRead::Second seconds) const;
};
}

#endif
