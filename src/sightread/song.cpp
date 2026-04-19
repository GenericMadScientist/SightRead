#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

#include "sightread/song.hpp"

namespace {
struct UnisonLookup {
    std::vector<SightRead::StarPower> phrases;
    std::vector<SightRead::StarPower> other_phrases;
};

std::vector<SightRead::StarPower> lookup_phrases(const UnisonLookup& lookup,
                                                 int resolution)
{
    const SightRead::Tick tolerance {resolution / 4};

    std::vector<SightRead::StarPower> unison_phrases;
    for (const auto& phrase : lookup.phrases) {
        std::vector<SightRead::StarPower> candidates {phrase};
        for (const auto& candidate : lookup.other_phrases) {
            if (candidate.position + tolerance > phrase.position
                && phrase.position + tolerance > candidate.position) {
                candidates.push_back(candidate);
            }
        }

        if (candidates.size() < 2) {
            continue;
        }

        auto benchmark = candidates.at(0);
        for (auto i = 1U; i < candidates.size(); ++i) {
            if (candidates.at(i).position < benchmark.position) {
                benchmark = candidates.at(i);
            }
        }

        auto nearby_phrases = 0;
        const auto benchmark_end = benchmark.position + benchmark.length;
        for (const auto& candidate : candidates) {
            const auto candidate_end = candidate.position + candidate.length;
            if (candidate_end + tolerance > benchmark_end
                && benchmark_end + tolerance > candidate_end) {
                ++nearby_phrases;
            }
        }

        if (nearby_phrases >= 2) {
            unison_phrases.push_back(phrase);
        }
    }

    return unison_phrases;
}

void sort_and_uniqify_phrases(std::vector<SightRead::StarPower>& phrases)
{
    std::ranges::sort(phrases, [](const auto& x, const auto& y) {
        return std::tie(x.position, x.length) < std::tie(y.position, y.length);
    });
    const auto [first, last]
        = std::ranges::unique(phrases, [](const auto& x, const auto& y) {
              return std::tie(x.position, x.length)
                  == std::tie(y.position, y.length);
          });
    phrases.erase(first, last);
}
}

void SightRead::Song::add_note_track(SightRead::Instrument instrument,
                                     SightRead::Difficulty difficulty,
                                     SightRead::NoteTrack note_track)
{
    if (!note_track.notes().empty()) {
        m_tracks.emplace(std::tuple {instrument, difficulty},
                         std::move(note_track));
    }
}

std::vector<SightRead::Instrument> SightRead::Song::instruments() const
{
    std::set<SightRead::Instrument> instrument_set;
    for (const auto& [key, val] : m_tracks) {
        instrument_set.insert(std::get<0>(key));
    }

    std::vector<SightRead::Instrument> instruments {instrument_set.cbegin(),
                                                    instrument_set.cend()};
    std::ranges::sort(instruments);
    return instruments;
}

std::vector<SightRead::Difficulty>
SightRead::Song::difficulties(SightRead::Instrument instrument) const
{
    std::vector<SightRead::Difficulty> difficulties;
    for (const auto& [key, val] : m_tracks) {
        if (std::get<0>(key) == instrument) {
            difficulties.push_back(std::get<1>(key));
        }
    }
    std::ranges::sort(difficulties);
    return difficulties;
}

const SightRead::NoteTrack&
SightRead::Song::track(SightRead::Instrument instrument,
                       SightRead::Difficulty difficulty) const
{
    const auto insts = instruments();
    if (std::ranges::find(insts, instrument) == std::ranges::end(insts)) {
        throw std::invalid_argument("Chosen instrument not present in song");
    }
    const auto diffs = difficulties(instrument);
    if (std::ranges::find(diffs, difficulty) == std::ranges::end(diffs)) {
        throw std::invalid_argument(
            "Difficulty not available for chosen instrument");
    }
    return m_tracks.at({instrument, difficulty});
}

std::vector<SightRead::StarPower> SightRead::Song::unison_phrases() const
{
    const std::array<std::vector<SightRead::Instrument>, 4> INSTRUMENT_GROUPS {
        {{SightRead::Instrument::Guitar, SightRead::Instrument::GHLGuitar,
          SightRead::Instrument::GuitarCoop, SightRead::Instrument::Rhythm},
         {SightRead::Instrument::Bass, SightRead::Instrument::GHLBass},
         {SightRead::Instrument::Drums},
         {SightRead::Instrument::Keys}}};

    std::array<UnisonLookup, 4> unison_lookups;
    for (auto i = 0U; i < INSTRUMENT_GROUPS.size(); ++i) {
        for (const auto instrument : INSTRUMENT_GROUPS.at(i)) {
            const auto diffs = difficulties(instrument);
            if (diffs.empty()) {
                continue;
            }
            const auto difficulty = *std::ranges::max_element(diffs);
            const auto& track = m_tracks.at({instrument, difficulty});
            for (const auto& phrase : track.sp_phrases()) {
                for (auto j = 0U; j < INSTRUMENT_GROUPS.size(); ++j) {
                    if (i == j) {
                        unison_lookups.at(j).phrases.push_back(phrase);
                    } else {
                        unison_lookups.at(j).other_phrases.push_back(phrase);
                    }
                }
            }
        }
    }

    std::vector<SightRead::StarPower> phrases;
    for (const auto& lookup : unison_lookups) {
        for (const auto& phrase :
             lookup_phrases(lookup, m_global_data->resolution())) {
            phrases.push_back(phrase);
        }
    }

    sort_and_uniqify_phrases(phrases);
    return phrases;
}

void SightRead::Song::speedup(int speed)
{
    constexpr int DEFAULT_SPEED = 100;

    if (speed == DEFAULT_SPEED) {
        return;
    }
    if (speed <= 0) {
        throw std::invalid_argument("Speed must be positive");
    }

    m_global_data->name(m_global_data->name() + " (" + std::to_string(speed)
                        + "%)");
    m_global_data->tempo_map(m_global_data->tempo_map().speedup(speed));
}
