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
    std::vector<SightRead::StarPower> all_phrases;
    for (const auto instrument : instruments()) {
        const auto diffs = difficulties(instrument);
        const auto difficulty = *std::ranges::max_element(diffs);
        const auto& track = m_tracks.at({instrument, difficulty});
        for (const auto& phrase : track.sp_phrases()) {
            all_phrases.push_back(phrase);
        }
    }
    std::ranges::sort(all_phrases, [](const auto& x, const auto& y) {
        return std::tie(x.position, x.length) < std::tie(y.position, y.length);
    });

    std::vector<SightRead::StarPower> phrases;
    auto unison_start_phrase = all_phrases.cbegin();
    const SightRead::Tick tolerance {m_global_data->resolution() / 4};
    for (auto it = all_phrases.cbegin(); it < all_phrases.cend();) {
        unison_start_phrase
            = std::find_if(unison_start_phrase, it, [=](const auto& phrase) {
                  return phrase.position + tolerance > it->position;
              });
        if (std::next(unison_start_phrase) == all_phrases.cend()) {
            break;
        }
        if (it == unison_start_phrase
            && it->position + tolerance <= std::next(it)->position) {
            ++it;
            continue;
        }
        if (it->length < unison_start_phrase->length + tolerance) {
            phrases.push_back(*it);
        }
        it = std::find_if_not(it, all_phrases.cend(), [=](const auto& phrase) {
            return std::tie(phrase.position, phrase.length)
                == std::tie(it->position, it->length);
        });
    }

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
