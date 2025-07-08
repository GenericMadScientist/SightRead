# SightRead

SightRead is a library for reading .chart and .mid files for GH/RB-style games.

## Requirements

The main library requires Boost.Locale and a C++20 compiler. Boost.Test is
needed if you want to build the tests.

## Crash Course

This is a quick summary of what I expect people are most likely to want. Proper
documentation will come when the interface settles down.

Your two parsers are `SightRead::ChartParser` and `SightRead::MidiParser`. Find
them in `sightread/chartparser.hpp` and `sightread/midiparser.hpp` respectively.
Both take in their constructor a single `const SightRead::Metadata&` parameter,
which is a simple struct with song name, artist, and charter.

There are some methods to customise their behaviour, but for Clone Hero the only
one you care about is `.hopo_threshold`. This takes a struct representing the
song.ini options that determine the cutoff for how close notes need to be to be
HOPOs. The default behaviour is as if these tags are absent.

Then to use the parsers, both have a `.parse` method. `ChartParser` accepts a
`std::string_view`, `MidiParser` accepts a `std::span<const std::uint8_t>`.
These are meant to be the contents of the .chart/.midi files. Of note is that
`ChartParser::parse` expects UTF-8. Unfortunately UTF-16 .chart files do exist
in the wild, and the conversion is your job.

Both parsers return a `SightRead::Song`. Here the primary methods are `.track`
to get a `SightRead::NoteTrack` for a particular instrument and difficulty, and
`.global_data()` which returns a class that crucially contains a
`SightRead::TempoMap`. The tempo map is there to convert between time that is
measured in various units: `Beat`, `Measure`, `OdBeat` (for Rock Band),
`Second`, and `Tick`. Not all the conversion methods currently exist, but you
can convert between any two with suitable chaining. I'll clean that up at some
point.

Worth noting, right now `SightRead::NoteTrack` pretty much contains just what is
needed for CHOpt. In particular, section names are currently absent. However,
HOPO/tap status is present on notes. This has not been thoroughly tested though
so for the time being, caveat emptor!

Lastly, no writing. Serialisation is out of scope for SightRead.

## Integration

The intended means of consumption is as a git submodule. This gives you a CMake
library called `sightread`, which can then be used in a way similar to the
following:

```cmake
add_subdirectory("extern/sightread")
target_link_libraries(<YOUR-TARGET> PRIVATE sightread)
```

You should use a tagged version, for now `main` is a development branch and
right now the interface should be treated as very unstable.

## Acknowledgements

* TheNathannator for [making my life easier](https://github.com/TheNathannator/GuitarGame_ChartFormats).
* FireFox2000000, for Moonscraper was useful to understand CH's parsing
behaviour back when I first made CHOpt.
* Matref for the name.
