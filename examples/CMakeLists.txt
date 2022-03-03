message(STATUS "Configuring sst-filters examples...")

CPMAddPackage("gh:juce-framework/JUCE#6.1.5")

juce_add_gui_app(noodler
        BUNDLE_ID org.surge-synth-team.sst-oscillators-mit.noodler
        PRODUCT_NAME "Noodler"
        )
target_sources(noodler PRIVATE noodler.cpp)
target_compile_definitions(noodler
        PUBLIC

        JUCE_ALLOW_STATIC_NULL_VARIABLES=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1

        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CAMERA=disabled

        # These are purposefully not overridden.
        # JUCE_DISPLAY_SPLASH_SCREEN=0
        # JUCE_REPORT_APP_USAGE=0

        JUCE_COREGRAPHICS_DRAW_ASYNC=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1

        JUCE_CATC_UNHANDLED_EXCEPTIONS=0
        )
target_link_libraries(noodler PUBLIC
        juce::juce_core
        juce::juce_gui_basics
        juce::juce_dsp
        juce::juce_audio_devices
        simde
        sst-oscillators-mit-testclients)