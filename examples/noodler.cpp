#include "juce_core/juce_core.h"

#include "sst/oscillators/SimpleExample.h"
#include "sst/oscillators/APIJuceComponent.h"

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(
    sst::oscillators_testclients::OSCApplication<sst::oscillators_mit::SimpleExample<>>)
