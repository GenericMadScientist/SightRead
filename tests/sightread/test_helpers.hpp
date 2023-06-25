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
