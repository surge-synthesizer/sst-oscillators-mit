#include "juce_core/juce_core.h"

#include "sst/oscillators/SimpleExample.h"
#include "sst/oscillators/APFPD.h"

#include "sst/oscillators/APIJuceComponent.h"

using namespace sst::oscillators_mit;
using namespace sst::oscillators_testclients;

//==============================================================================
// This macro generates the main() routine that launches the app.

START_JUCE_APPLICATION(OSCApplication<APFPD<>>)
// START_JUCE_APPLICATION(OSCApplication<SimpleExample<>>)