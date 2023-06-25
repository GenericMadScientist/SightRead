#ifndef SIGHTREAD_TESTHELPERS_HPP
#define SIGHTREAD_TESTHELPERS_HPP

#include <ostream>
#include <tuple>

#include "sightread/songparts.hpp"

inline SightRead::Note make_note(int position, int length = 0,
                                 SightRead::FiveFretNotes colour
                                 = SightRead::FIVE_FRET_GREEN)
{
    SightRead::Note note;
    note.position = SightRead::Tick {position};
    note.flags = SightRead::FLAGS_FIVE_FRET_GUITAR;
    note.lengths.at(colour) = SightRead::Tick {length};

    return note;
}

inline SightRead::Note
make_drum_note(int position, SightRead::DrumNotes colour = SightRead::DRUM_RED,
               SightRead::NoteFlags flags = SightRead::FLAGS_NONE)
{
    SightRead::Note note;
    note.position = SightRead::Tick {position};
    note.flags
        = static_cast<SightRead::NoteFlags>(flags | SightRead::FLAGS_DRUMS);
    note.lengths.at(colour) = SightRead::Tick {0};

    return note;
}

namespace SightRead {
inline bool operator!=(const BPM& lhs, const BPM& rhs)
{
    return std::tie(lhs.position, lhs.bpm) != std::tie(rhs.position, rhs.bpm);
}

inline std::ostream& operator<<(std::ostream& stream, const BPM& bpm)
{
    stream << "{Pos " << bpm.position << ", BPM " << bpm.bpm << '}';
    return stream;
}

inline bool operator!=(const DrumFill& lhs, const DrumFill& rhs)
{
    return std::tie(lhs.position, lhs.length)
        != std::tie(rhs.position, rhs.length);
}

inline std::ostream& operator<<(std::ostream& stream, const DrumFill& fill)
{
    stream << "{Pos " << fill.position << ", Length " << fill.length << '}';
    return stream;
}

inline std::ostream& operator<<(std::ostream& stream, Instrument instrument)
{
    stream << static_cast<int>(instrument);
    return stream;
}

inline bool operator!=(const Note& lhs, const Note& rhs)
{
    return std::tie(lhs.position, lhs.lengths, lhs.flags)
        != std::tie(rhs.position, rhs.lengths, rhs.flags);
}

inline std::ostream& operator<<(std::ostream& stream, const Note& note)
{
    stream << "{Pos " << note.position << ", ";
    for (auto i = 0; i < 7; ++i) {
        if (note.lengths.at(i) != SightRead::Tick {-1}) {
            stream << "Colour " << i << " with Length " << note.lengths.at(i)
                   << ", ";
        }
    }
    stream << "Flags " << std::hex << note.flags << std::dec << '}';
    return stream;
}

inline bool operator!=(const Solo& lhs, const Solo& rhs)
{
    return std::tie(lhs.start, lhs.end, lhs.value)
        != std::tie(rhs.start, rhs.end, rhs.value);
}

inline std::ostream& operator<<(std::ostream& stream, const Solo& solo)
{
    stream << "{Start " << solo.start << ", End " << solo.end << ", Value "
           << solo.value << '}';
    return stream;
}

inline bool operator!=(const StarPower& lhs, const StarPower& rhs)
{
    return std::tie(lhs.position, lhs.length)
        != std::tie(rhs.position, rhs.length);
}

inline std::ostream& operator<<(std::ostream& stream, const StarPower& sp)
{
    stream << "{Pos " << sp.position << ", Length " << sp.length << '}';
    return stream;
}

inline bool operator!=(const TimeSignature& lhs, const TimeSignature& rhs)
{
    return std::tie(lhs.position, lhs.numerator, lhs.denominator)
        != std::tie(rhs.position, rhs.numerator, rhs.denominator);
}

inline std::ostream& operator<<(std::ostream& stream, const TimeSignature& ts)
{
    stream << "{Pos " << ts.position << ", " << ts.numerator << '/'
           << ts.denominator << '}';
    return stream;
}
}

#endif
