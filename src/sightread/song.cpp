#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

#include "sightread/detail/parserutil.hpp"
#include "sightread/song.hpp"

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
    std::sort(instruments.begin(), instruments.end());
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
    std::sort(difficulties.begin(), difficulties.end());
    return difficulties;
}

const SightRead::NoteTrack&
SightRead::Song::track(SightRead::Instrument instrument,
                       SightRead::Difficulty difficulty) const
{
    const auto insts = instruments();
    if (std::find(insts.cbegin(), insts.cend(), instrument) == insts.cend()) {
        throw std::invalid_argument("Chosen instrument not present in song");
    }
    const auto diffs = difficulties(instrument);
    if (std::find(diffs.cbegin(), diffs.cend(), difficulty) == diffs.cend()) {
        throw std::invalid_argument(
            "Difficulty not available for chosen instrument");
    }
    return m_tracks.at({instrument, difficulty});
}

std::vector<SightRead::Tick> SightRead::Song::unison_phrase_positions() const
{
    std::map<SightRead::Tick, std::set<SightRead::Instrument>>
        phrase_by_instrument;
    for (const auto& [key, value] : m_tracks) {
        const auto instrument = std::get<0>(key);
        if (SightRead::Detail::is_six_fret_instrument(instrument)) {
            continue;
        }
        for (const auto& phrase : value.sp_phrases()) {
            phrase_by_instrument[phrase.position].insert(instrument);
        }
    }

    std::vector<SightRead::Tick> unison_starts;
    for (const auto& [key, value] : phrase_by_instrument) {
        if (value.size() > 1) {
            unison_starts.push_back(key);
        }
    }
    return unison_starts;
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
