#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <unordered_map>

#include "sightread/detail/qbmidiconverter.hpp"

namespace {
constexpr int RESOLUTION = 1920;

constexpr std::array<std::uint32_t, 256> crc_table {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

std::uint32_t crc32(std::string_view key, std::uint32_t initial_crc = ~0U)
{
    std::uint32_t crc = initial_crc;

    for (const auto character : key) {
        const auto table_key
            = (crc ^ static_cast<std::uint32_t>(character)) & 0xFF;
        crc = crc_table.at(table_key) ^ (crc >> CHAR_BIT);
    }

    return crc;
}

SightRead::Detail::QbItem
find_item_by_id(const std::vector<SightRead::Detail::QbItem>& items,
                std::string_view suffix, std::uint32_t prefix_crc)
{
    const auto crc = crc32(suffix, prefix_crc);

    for (const auto& item : items) {
        if (item.props.id == crc) {
            return item;
        }
    }

    throw SightRead::ParseError("Unable to find item by id");
}

std::vector<std::uint32_t> fretbars_ms(const SightRead::Detail::QbMidi& midi,
                                       std::uint32_t short_name_crc)
{
    const auto fretbars_item
        = find_item_by_id(midi.items, "_fretbars", short_name_crc);
    const auto raw_fretbars
        = std::any_cast<std::vector<std::any>>(fretbars_item.data);

    std::vector<std::uint32_t> values;
    values.reserve(raw_fretbars.size());
    for (const auto& value : raw_fretbars) {
        values.push_back(std::any_cast<std::uint32_t>(value));
    }

    return values;
}

struct QbNoteEvent {
    std::uint32_t position;
    std::uint32_t length;
    std::uint32_t flags;
};

std::vector<QbNoteEvent> note_events(const SightRead::Detail::QbMidi& midi,
                                     std::uint32_t short_name_crc,
                                     SightRead::Difficulty difficulty)
{
    const std::unordered_map<SightRead::Difficulty, std::string> diff_names {
        {SightRead::Difficulty::Easy, "easy"},
        {SightRead::Difficulty::Medium, "medium"},
        {SightRead::Difficulty::Hard, "hard"},
        {SightRead::Difficulty::Expert, "expert"}};

    const auto suffix = std::string("_song_") + diff_names.at(difficulty);
    const auto notes_item = find_item_by_id(midi.items, suffix, short_name_crc);
    const auto raw_notes
        = std::any_cast<std::vector<std::any>>(notes_item.data);
    assert((raw_notes.size() % 3) == 0);

    std::vector<QbNoteEvent> values;
    values.reserve(raw_notes.size() / 3);
    for (auto i = 0U; i < raw_notes.size(); i += 3U) {
        values.push_back({std::any_cast<std::uint32_t>(raw_notes[i]),
                          std::any_cast<std::uint32_t>(raw_notes[i + 1]),
                          std::any_cast<std::uint32_t>(raw_notes[i + 2])});
    }

    return values;
}

struct QbTimeSignature {
    std::uint32_t time_ms;
    std::uint32_t numerator;
    std::uint32_t denominator;
};

std::vector<QbTimeSignature> qb_timesigs(const SightRead::Detail::QbMidi& midi,
                                         std::uint32_t short_name_crc)
{
    const auto timesigs_item
        = find_item_by_id(midi.items, "_timesig", short_name_crc);
    const auto raw_timesigs
        = std::any_cast<std::vector<std::any>>(timesigs_item.data);

    std::vector<QbTimeSignature> values;
    values.reserve(raw_timesigs.size());
    for (const auto& value : raw_timesigs) {
        const auto array = std::any_cast<std::vector<std::any>>(value);
        assert(array.size() == 3);
        values.push_back({std::any_cast<std::uint32_t>(array[0]),
                          std::any_cast<std::uint32_t>(array[1]),
                          std::any_cast<std::uint32_t>(array[2])});
    }

    return values;
}

struct QbSpEvent {
    std::uint32_t position;
    std::uint32_t length;
    std::uint32_t note_count;
};

std::vector<QbSpEvent> sp_events(const SightRead::Detail::QbMidi& midi,
                                 std::uint32_t short_name_crc,
                                 SightRead::Difficulty difficulty)
{
    const std::unordered_map<SightRead::Difficulty, std::string> diff_names {
        {SightRead::Difficulty::Easy, "easy"},
        {SightRead::Difficulty::Medium, "medium"},
        {SightRead::Difficulty::Hard, "hard"},
        {SightRead::Difficulty::Expert, "expert"}};

    const auto suffix = std::string("_") + diff_names.at(difficulty) + "_star";
    const auto sps_item = find_item_by_id(midi.items, suffix, short_name_crc);
    const auto raw_phrases
        = std::any_cast<std::vector<std::any>>(sps_item.data);

    std::vector<QbSpEvent> values;
    values.reserve(raw_phrases.size());
    for (const auto& value : raw_phrases) {
        const auto array = std::any_cast<std::vector<std::any>>(value);
        assert(array.size() == 3);
        values.push_back({std::any_cast<std::uint32_t>(array[0]),
                          std::any_cast<std::uint32_t>(array[1]),
                          std::any_cast<std::uint32_t>(array[2])});
    }

    return values;
}

struct QbTimeData {
private:
    std::vector<double> m_fretbars_beats;
    std::vector<std::uint32_t> m_fretbars_ms;
    std::vector<QbTimeSignature> m_timesigs;

    [[nodiscard]] double ms_to_beats(std::uint32_t ms) const
    {
        const auto it = std::upper_bound(m_fretbars_ms.cbegin(),
                                         m_fretbars_ms.cend(), ms);
        assert(it != m_fretbars_ms.cbegin());
        assert(it != m_fretbars_ms.cend());
        const auto beat_after = *it;
        const auto beat_before = *std::prev(it);
        const auto after_index = std::distance(m_fretbars_ms.cbegin(), it);
        const auto after_beat
            = m_fretbars_beats.at(static_cast<std::size_t>(after_index));
        return after_beat
            - static_cast<double>(beat_after - ms)
            * (after_beat
               - m_fretbars_beats.at(static_cast<std::size_t>(after_index - 1)))
            / static_cast<double>(beat_after - beat_before);
    }

public:
    QbTimeData(std::vector<std::uint32_t> fretbars,
               std::vector<QbTimeSignature> timesigs)
        : m_fretbars_ms {std::move(fretbars)}
        , m_timesigs {std::move(timesigs)}
    {
        constexpr auto DEFAULT_TIME_SIG_DENOMINATOR = 4.0;

        assert(!m_fretbars_ms.empty());
        m_fretbars_beats.reserve(m_fretbars_ms.size());
        m_fretbars_beats.emplace_back(0.0);

        auto beat_position = 0.0;
        auto timesig_denominator = 4U;
        auto timesig_iter = m_timesigs.cbegin();
        auto fretbar_iter = m_fretbars_ms.cbegin();

        while (fretbar_iter < std::prev(m_fretbars_ms.cend())) {
            if (timesig_iter != m_timesigs.cend()
                && timesig_iter->time_ms <= *fretbar_iter) {
                timesig_denominator = timesig_iter->denominator;
                ++timesig_iter;
                continue;
            }

            beat_position += DEFAULT_TIME_SIG_DENOMINATOR / timesig_denominator;
            m_fretbars_beats.emplace_back(beat_position);
            ++fretbar_iter;
        }
    }

    [[nodiscard]] std::vector<SightRead::BPM> bpms() const
    {
        constexpr auto MICROS_IN_MINUTE = 60 * 1000 * 1000;

        std::vector<SightRead::BPM> bpms;
        bpms.reserve(m_fretbars_ms.size() - 1);
        for (auto i = 0U; i + 1 < m_fretbars_ms.size(); ++i) {
            const auto time_diff = m_fretbars_ms[i + 1] - m_fretbars_ms[i];
            const auto tick_pos
                = static_cast<int>(RESOLUTION * m_fretbars_beats[i]);
            const auto beat_diff
                = m_fretbars_beats[i + 1] - m_fretbars_beats[i];
            bpms.push_back(
                {SightRead::Tick {tick_pos},
                 static_cast<int>(MICROS_IN_MINUTE * beat_diff / time_diff)});
        }

        return bpms;
    }

    [[nodiscard]] SightRead::Tick ms_to_ticks(std::uint32_t ms) const
    {
        return SightRead::Tick {static_cast<int>(RESOLUTION * ms_to_beats(ms))};
    }

    [[nodiscard]] std::uint32_t sustain_threshold() const
    {
        return m_fretbars_ms.at(1) / 2;
    }

    [[nodiscard]] std::vector<SightRead::TimeSignature> time_sigs() const
    {
        std::vector<SightRead::TimeSignature> time_sigs;
        time_sigs.reserve(m_timesigs.size());
        for (const auto& time_sig : m_timesigs) {
            const auto position = ms_to_ticks(time_sig.time_ms);
            time_sigs.push_back({position, static_cast<int>(time_sig.numerator),
                                 static_cast<int>(time_sig.denominator)});
        }

        return time_sigs;
    }
};

QbTimeData time_data(const SightRead::Detail::QbMidi& midi,
                     std::uint32_t short_name_crc)
{
    return {fretbars_ms(midi, short_name_crc),
            qb_timesigs(midi, short_name_crc)};
}

SightRead::TempoMap tempo_map(const QbTimeData& time_data)
{
    return {time_data.time_sigs(), time_data.bpms(), {}, RESOLUTION};
}

std::optional<SightRead::NoteTrack>
note_track(const SightRead::Detail::QbMidi& midi, std::uint32_t short_name_crc,
           SightRead::Difficulty difficulty,
           std::shared_ptr<SightRead::SongGlobalData> global_data,
           const QbTimeData& timedata)
{
    constexpr auto NUMBER_OF_FRETS = 5U;

    const auto events = note_events(midi, short_name_crc, difficulty);
    if (events.empty()) {
        return {};
    }

    const auto phrase_events = sp_events(midi, short_name_crc, difficulty);

    const auto sustain_threshold = timedata.sustain_threshold();
    std::vector<SightRead::Note> notes;
    notes.reserve(events.size());
    for (const auto& event : events) {
        SightRead::Note note;
        auto ms_length = event.length;
        if (ms_length <= sustain_threshold) {
            ms_length = 0;
        }
        note.position = timedata.ms_to_ticks(event.position);
        const auto end_position
            = timedata.ms_to_ticks(event.position + ms_length);
        const auto length = end_position - note.position;

        for (auto i = 0U; i < NUMBER_OF_FRETS; ++i) {
            if ((event.flags & (1 << i)) != 0) {
                note.lengths.at(i) = length;
            }
        }
        if ((event.flags & (1 << NUMBER_OF_FRETS)) != 0) {
            note.flags = SightRead::NoteFlags::FLAGS_FORCE_FLIP;
        }

        notes.push_back(note);
    }

    std::vector<SightRead::StarPower> sp_phrases;
    sp_phrases.reserve(phrase_events.size());
    for (const auto& event : phrase_events) {
        const auto position = timedata.ms_to_ticks(event.position);
        const auto end_position
            = timedata.ms_to_ticks(event.position + event.length);
        sp_phrases.push_back({position, end_position - position});
    }

    return {{std::move(notes), sp_phrases, SightRead::TrackType::FiveFret,
             std::move(global_data)}};
}
}

SightRead::Detail::QbMidiConverter::QbMidiConverter(std::string_view short_name)
    : m_short_name_crc {crc32(short_name)}
{
}

SightRead::Song SightRead::Detail::QbMidiConverter::convert(
    const SightRead::Detail::QbMidi& midi) const
{
    constexpr std::array<SightRead::Difficulty, 4> DIFFICULTIES {
        SightRead::Difficulty::Easy, SightRead::Difficulty::Medium,
        SightRead::Difficulty::Hard, SightRead::Difficulty::Expert};

    const auto timedata = time_data(midi, m_short_name_crc);

    SightRead::Song song;
    song.global_data().resolution(RESOLUTION);
    song.global_data().tempo_map(tempo_map(timedata));

    for (const auto diff : DIFFICULTIES) {
        auto track = note_track(midi, m_short_name_crc, diff,
                                song.global_data_ptr(), timedata);
        if (track.has_value()) {
            song.add_note_track(SightRead::Instrument::Guitar, diff,
                                std::move(*track));
        }
    }

    return song;
}
