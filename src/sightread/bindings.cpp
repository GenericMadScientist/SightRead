#include <emscripten/bind.h>

#include "sightread/midiparser.hpp"
#include "sightread/songparts.hpp"
#include "sightread/chartparser.hpp"
#include "sightread/metadata.hpp"
#include "sightread/detail/chart.hpp"
#include "sightread/detail/chartconverter.hpp"

using namespace emscripten;

EMSCRIPTEN_BINDINGS(Sightread) {
    class_<SightRead::MidiParser>("MidiParser")
        .constructor<SightRead::Metadata>()
        .function("hopo_threshold", &SightRead::MidiParser::hopo_threshold)
        .function("permit_instruments", &SightRead::MidiParser::permit_instruments)
        .function("parse_solos", &SightRead::MidiParser::parse_solos)
        .function("parse", &SightRead::MidiParser::parse)
        ;

    class_<std::set<SightRead::Instrument>>("std::set<SightRead::Instrument>")
		.constructor<>();

	value_object<SightRead::Metadata>("Metadata")
        .field("name", &SightRead::Metadata::name)
        .field("artist", &SightRead::Metadata::artist)
        .field("charter", &SightRead::Metadata::charter)
        ;

	enum_<SightRead::Instrument>("Instrument")
		.value("Guitar", SightRead::Instrument::Guitar)
		.value("GuitarCoop", SightRead::Instrument::GuitarCoop)
		.value("Bass", SightRead::Instrument::Bass)
		.value("Rhythm", SightRead::Instrument::Rhythm)
		.value("Keys", SightRead::Instrument::Keys)
		.value("GHLGuitar", SightRead::Instrument::GHLGuitar)
		.value("GHLBass", SightRead::Instrument::GHLBass)
		.value("GHLRhythm", SightRead::Instrument::GHLRhythm)
		.value("GHLGuitarCoop", SightRead::Instrument::GHLGuitarCoop)
		.value("Drums", SightRead::Instrument::Drums)
		;
}
