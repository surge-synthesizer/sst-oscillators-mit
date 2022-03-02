//
// Created by Paul Walker on 3/1/22.
//

#ifndef SST_OSCILLATORS_MIT_APIJUCECOMPONENT_H
#define SST_OSCILLATORS_MIT_APIJUCECOMPONENT_H

// Compiling this requires you be in a juce environment
#include "juce_gui_basics/juce_gui_basics.h"
#include "juce_core/juce_core.h"
#include "juce_audio_devices/juce_audio_devices.h"

#include <memory>
#include <sst/oscillators/API.h>
#include "juce_dsp/juce_dsp.h"

namespace sst
{
namespace oscillators_testclients
{

template <typename osc_t> class OSCComponent : public juce::Component, juce::AudioIODeviceCallback
{
  public:
    std::unique_ptr<sst::oscillators_mit::DummyPitchProvider> tuning;

    struct OCMB : public juce::MenuBarModel
    {
        OSCComponent<osc_t> *parent;
        OCMB(OSCComponent<osc_t> *oc) : parent(oc) {}
        juce::StringArray getMenuBarNames() override
        {
            auto res = juce::StringArray();
            res.add("Play");
            res.add("FFT");
            res.add("Settings");
            return res;
        }

        juce::PopupMenu getMenuForIndex(int topLevelMenuIndex,
                                        const juce::String &menuName) override
        {
            if (topLevelMenuIndex == 0)
            {
                auto r = juce::PopupMenu();
                r.addItem("2 Seconds C 261", [this]() { parent->playNoteForSec(60, 2); });
                r.addItem("2 Seconds A 440", [this]() { parent->playNoteForSec(60, 2); });
                r.addSeparator();
                r.addItem("Chromatic MiddleC Scale", [this]() {
                    auto c = 60.0;
                    while (c <= 72)
                    {
                        parent->playNoteForSec(c, 0.5);
                        c = c + 1;
                    }
                });
                r.addItem("Fifths over 5 octaves", [this]() {
                    auto c = 60.0 - 3 * 12;
                    while (c <= 60 + 2 * 12)
                    {
                        parent->playNoteForSec(c, 0.5);
                        c = c + 7;
                    }
                });
                r.addItem("Frequency Sweep", [this]() { parent->playFrequencySweep(); });
                r.addSeparator();
                r.addItem(parent->deviceName + " at " + std::to_string(parent->sampleRate),
                          []() {});
                return r;
            }
            else if (topLevelMenuIndex == 1)
            {
                auto r = juce::PopupMenu();
                r.addItem("Generate Constant Tone", [this]() { parent->constantToneFFT(); });
                r.addItem("Generate Sweep", [this]() { parent->sweepToneFFT(); });
                return r;
            }
            else
            {
                auto r = juce::PopupMenu();
                r.addSectionHeader(menuName);
                r.addItem("Coming Soon", []() {});
                return r;
            }
        }

        void menuItemSelected(int menuItemID, int topLevelMenuIndex) override {}
    };

    double sampleRate{48000};
    std::string deviceName;
    bool hasAudio{false};
    //==============================================================================
    OSCComponent()
    {
        audioDeviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
        if (auto *device = audioDeviceManager.getCurrentAudioDevice())
        {
            sampleRate = device->getCurrentSampleRate();
            deviceName = device->getName().toStdString();
            hasAudio = true;
        }
        else
        {
            deviceName = "no audio";
        }
        audioDeviceManager.addAudioCallback(this);

        tuning = std::make_unique<sst::oscillators_mit::DummyPitchProvider>();
        menuBar = std::make_unique<OCMB>(this);
        auto osc = std::make_unique<osc_t>(sampleRate, tuning.get());

        for (int i = 0; i < osc->numParams(); ++i)
        {
            auto lb =
                std::make_unique<juce::Label>("lbl" + std::to_string(i), osc->getParamName(i));
            addAndMakeVisible(*lb);
            labels.push_back(std::move(lb));

            if (osc->getParamType(i) == sst::oscillators_mit::FLOAT)
            {
                auto b = std::make_unique<juce::Slider>();
                b->setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
                b->onValueChange = [this]() {
                    fftValid = false;
                    repaint();
                };
                float n, x, d;
                osc->getParamRange(i, n, x, d);
                b->setRange(n, x);
                b->setDoubleClickReturnValue(d, juce::dontSendNotification);
                b->setValue(d, juce::dontSendNotification);
                addAndMakeVisible(*b);
                controls.push_back(std::move(b));
            }
            else if (osc->getParamType(i) == sst::oscillators_mit::DISCRETE)
            {
                auto b = std::make_unique<juce::ComboBox>();
                b->onChange = [this]() {
                    fftValid = false;
                    repaint();
                };
                std::vector<std::string> val;
                int dv;
                osc->getDiscreteValues(i, val, dv);
                int idx = comboZero;
                for (const auto &s : val)
                {
                    b->addItem(s, idx++);
                }
                b->setSelectedId(comboZero + dv, juce::dontSendNotification);
                addAndMakeVisible(*b);
                controls.push_back(std::move(b));
            }
            else
            {
                throw "BAD OSC";
            }
        }

        setSize(1000, 700);

        idleTimer = std::make_unique<IdleTimer>(this);
        idleTimer->startTimer(1000 / 60);
    }

    ~OSCComponent()
    {
        audioDeviceManager.removeAudioCallback(this);
        idleTimer->stopTimer();
    }

    struct PipeSample
    {
        float *L, *R;
        size_t n;
        explicit PipeSample(size_t n) : n(n), L(new float[n]), R(new float[n])
        {
            memset(L, 0, n * sizeof(float));
            memset(R, 0, n * sizeof(float));
        }
        ~PipeSample()
        {
            delete[] L;
            delete[] R;
        }
    };

    struct IdleTimer : juce::Timer
    {
        OSCComponent<osc_t> *parent;
        IdleTimer(OSCComponent<osc_t> *oc) : parent(oc) {}
        ~IdleTimer() = default;
        void timerCallback() override { parent->idle(); }
    };
    void idle()
    {
        while (psDP != psRP)
        {
            delete pipeSamples[psDP];
            pipeSamples[psDP] = nullptr;
            if (psDP == nps - 1)
                psDP = 0;
            else
                psDP++;
        }
    }

    std::unique_ptr<IdleTimer> idleTimer;

    static constexpr int nps = 1000;
    std::array<PipeSample *, nps> pipeSamples;
    std::atomic<size_t> psWP{0}, psRP{0}, psDP{0};

    void playNoteForSec(float n, float sec)
    {
        auto osc = std::make_unique<osc_t>(sampleRate, tuning.get());
        sst::oscillators_mit::ParamData<float> data[7];
        populatePData(osc, data);
        osc->init(data);

        auto samples = (int)(sec * sampleRate);
        auto es = samples % osc_t::blocksize;
        samples -= es;

        auto ps = std::make_unique<PipeSample>(samples);

        float dL[32], dR[32];

        size_t p = 0;
        while (p < samples)
        {
            osc->template process<false>(n, dL, dR, data, nullptr);
            memcpy(&(ps->L[p]), dL, osc_t::blocksize * sizeof(float));
            memcpy(&(ps->R[p]), dR, osc_t::blocksize * sizeof(float));
            p += osc_t::blocksize;
        }

        pipeSamples[psWP] = ps.release();
        if (psWP == nps - 1)
            psWP = 0;
        else
            psWP++;
    }

    void playFrequencySweep()
    {
        float sec = 6.0;
        auto osc = std::make_unique<osc_t>(sampleRate, tuning.get());
        sst::oscillators_mit::ParamData<float> data[7];
        populatePData(osc, data);
        osc->init(data);

        auto samples = (int)(sec * sampleRate);
        auto es = samples % osc_t::blocksize;
        samples -= es;

        auto ps = std::make_unique<PipeSample>(samples);

        auto blocks = 1.0 * samples / osc_t::blocksize / 2;
        auto dPitch = 100 / blocks;
        float pitch = 20;

        float dL[32], dR[32];

        size_t p = 0;
        while (p < samples)
        {
            osc->template process<false>(pitch, dL, dR, data, nullptr);
            pitch += dPitch;
            if (p >= samples / 2 && dPitch > 0)
                dPitch = -dPitch;
            memcpy(&(ps->L[p]), dL, osc_t::blocksize * sizeof(float));
            memcpy(&(ps->R[p]), dR, osc_t::blocksize * sizeof(float));
            p += osc_t::blocksize;
        }

        pipeSamples[psWP] = ps.release();
        if (psWP == nps - 1)
            psWP = 0;
        else
            psWP++;
    }

    std::unique_ptr<juce::Image> spectrogramImage;

    void constantToneFFT()
    {
        auto toneFor = [](int) { return 60; };
        generateFFT(toneFor);
    }

    void sweepToneFFT()
    {
        if (!spectrogramImage)
            return;
        auto w = spectrogramImage->getWidth();
        auto toneFor = [w](int i) {
            if (i > w / 2)
                i = w - i;
            float res = i * 2.0 / w * 100 + 27;
            return res;
        };
        generateFFT(toneFor);
    }

    void generateFFT(const std::function<float(int)> &tf)
    {
        if (!spectrogramImage)
            return;

        auto osc = std::make_unique<osc_t>(sampleRate, tuning.get());
        sst::oscillators_mit::ParamData<float> data[7];
        populatePData(osc, data);
        osc->init(data);

        static constexpr int order = 10, size = 1 << order;
        juce::dsp::FFT fft(order);
        juce::dsp::WindowingFunction<float> window(size, juce::dsp::WindowingFunction<float>::hann);
        float dL[32], dR[32];

        auto w = spectrogramImage->getWidth();
        auto p = 0;
        while (p < w)
        {
            static constexpr int navg = 10;
            float fidata[navg][2 * size];
            for (int a = 0; a < navg; ++a)
            {
                memset(fidata[a], 0, 2 * size * sizeof(float));
                auto o = 0;
                while (o < size)
                {
                    osc->template process<false>(tf(p), dL, dR, data, nullptr);
                    memcpy(&(fidata[a][o]), dL, osc_t::blocksize * sizeof(float));
                    o += osc_t::blocksize;
                }
                window.multiplyWithWindowingTable(fidata[a], size);
                fft.performFrequencyOnlyForwardTransform(fidata[a]);
            }

            for (int i = 0; i < size; ++i)
            {
                float av = 0;
                for (int a = 0; a < navg; ++a)
                    av += fidata[a][i];
                av /= navg;
                fidata[0][i] = av;
            }

            auto imageHeight = spectrogramImage->getHeight();
            for (auto y = 1; y < imageHeight; ++y)
            {
                auto skewedProportionY =
                    1.0f - std::exp(std::log((float)y / (float)imageHeight) * 0.25f);

                auto rawIndex = skewedProportionY * size;
                auto riBase = std::clamp((int)std::floor(rawIndex), 0, size - 2);
                auto riNext = riBase + 1;
                auto frac = std::clamp(rawIndex - riBase, 0.f, 1.f);
                float val = fidata[0][riBase] * (1.0 - frac) + frac * fidata[0][riNext];

                auto off = 5.f;
                auto rlevel = sqrt(sqrt((std::clamp(val, off, 50.f + off) - off) / 50));
                auto glevel = sqrt(std::clamp(val, 0.f, 10.f) / 10);
                auto blevel = sqrt((std::clamp(val, 3.f, 30.f)) / 30);

                spectrogramImage->setPixelAt(
                    p, y, juce::Colour(rlevel * 255.f, 40 + 50 * glevel, 40 + 50 * blevel));
            }
            p++;
        }
        fftValid = true;
        repaint();
    }

  private:
    PipeSample *currentPipeSample{nullptr};
    size_t currentPipeSamplePosition{0};
    void audioDeviceStopped() override {}
    void audioDeviceIOCallback(const float **inputChannelData, int numInputChannels,
                               float **outputChannelData, int numOutputChannels,
                               int numSamples) override
    {
        // This is super inefficient of course
        assert(numOutputChannels == 2);
        memset(outputChannelData[0], 0, numSamples * sizeof(float));
        memset(outputChannelData[1], 0, numSamples * sizeof(float));

        if (!currentPipeSample)
            if (psRP != psWP)
            {
                currentPipeSample = pipeSamples[psRP];
                currentPipeSamplePosition = 0;
            }

        if (!currentPipeSample)
            return;

        for (int i = 0; i < numSamples && currentPipeSample; ++i)
        {
            outputChannelData[0][i] = currentPipeSample->L[currentPipeSamplePosition];
            outputChannelData[1][i] = currentPipeSample->R[currentPipeSamplePosition];
            currentPipeSamplePosition++;
            if (currentPipeSamplePosition >= currentPipeSample->n)
            {
                currentPipeSample = nullptr;
                if (psRP == nps - 1)
                    psRP = 0;
                else
                    psRP++;
            }
        }
    }
    void audioDeviceAboutToStart(juce::AudioIODevice *device) override {}

  public:
    //==============================================================================
    static constexpr int ctrlW = 300;
    static constexpr int comboZero = 1000;
    static constexpr int wfH = 400;
    bool fftValid{false};

    void populatePData(const std::unique_ptr<osc_t> &osc,
                       sst::oscillators_mit::ParamData<float> data[7])
    {
        if (osc->numParams() > 7)
            throw "BAD OSC";
        for (int i = 0; i < osc->numParams(); ++i)
        {
            if (osc->getParamType(i) == sst::oscillators_mit::FLOAT)
            {
                juce::Slider *s = dynamic_cast<juce::Slider *>(controls[i].get());
                if (s)
                {
                    data[i].f = s->getValue();
                }
                else
                {
                    data[i].f = 0;
                }
            }

            if (osc->getParamType(i) == sst::oscillators_mit::DISCRETE)
            {
                juce::ComboBox *s = dynamic_cast<juce::ComboBox *>(controls[i].get());
                if (s)
                {
                    data[i].i = s->getSelectedId() - comboZero;
                }
                else
                {
                    data[i].i = 0;
                }
            }
        }
    }
    void paint(juce::Graphics &g) override
    {
        auto osc = std::make_unique<osc_t>(sampleRate, tuning.get());

        // (Our component is opaque, so we must completely fill the background with a solid colour)
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

        auto r = getLocalBounds().withWidth(ctrlW);
        g.setColour(
            getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.4));
        g.fillRect(r);

        auto w = getLocalBounds().withTrimmedLeft(ctrlW).withHeight(wfH);
        g.setColour(juce::Colours::black);
        g.fillRect(w);
        sst::oscillators_mit::ParamData<float> data[7];
        populatePData(osc, data);
        osc->init(data);

        float dL[32], dR[32];
        float p = w.getX() + 3;
        g.setColour(juce::Colours::white);

        juce::Path pt;
        bool first{true};
        std::vector<std::pair<float, float>> ell;
        while (p < w.getWidth() - 3 + w.getX())
        {
            osc->template process<false>(60, dL, dR, data, nullptr);
            for (int i = 0; i < 32; ++i)
            {
                auto y = ((1.0 - dL[i])) * 0.5;
                y = y * w.getHeight() * 0.9 + w.getHeight() * 0.05;
                ell.emplace_back(p, y);
                if (first)
                    pt.startNewSubPath(p, y);
                else
                    pt.lineTo(p, y);
                first = false;
                p++;
            }
            g.setColour(juce::Colours::grey);
            g.strokePath(pt, juce::PathStrokeType(1.0));
        }
        g.setColour(juce::Colours::white);
        for (auto e : ell)
        {
            g.fillEllipse(e.first - 1, e.second - 1, 3, 3);
        }

        auto ctH = 30;
        auto lb = getLocalBounds().withWidth(ctrlW).withHeight(ctH);
        g.setFont(20);
        g.setColour(juce::Colours::white);
        g.drawText(osc->getName(), lb.reduced(2), juce::Justification::left);

        auto specr = getLocalBounds().withTrimmedLeft(ctrlW).withTrimmedTop(wfH);
        if (spectrogramImage)
        {
            g.setOpacity(1.f);
            g.drawImage(*spectrogramImage, specr.toFloat());
        }
        if (!fftValid)
        {
            g.setColour(juce::Colours::white);
            g.drawText("stale", specr, juce::Justification::topLeft);
        }
    }

    void resized() override
    {
        auto ctH = 30;
        auto r = getLocalBounds().withWidth(ctrlW).withHeight(ctH);
        r = r.translated(0, ctH);
        for (int i = 0; i < labels.size(); ++i)
        {
            labels[i]->setBounds(r.reduced(1));
            r = r.translated(0, ctH);
            controls[i]->setBounds(r.reduced(1));
            r = r.translated(0, ctH * 1.2);
        }

        auto specr = getLocalBounds().withTrimmedLeft(ctrlW).withTrimmedTop(wfH);
        spectrogramImage = std::make_unique<juce::Image>(juce::Image::RGB, specr.getWidth() * 2,
                                                         specr.getHeight() * 2, true);

        auto imw = spectrogramImage->getWidth();
        auto imh = spectrogramImage->getHeight();
        for (int i = 0; i < imw; ++i)
        {
            for (int j = 0; j < imh; ++j)
            {
                spectrogramImage->setPixelAt(i, j,
                                             juce::Colour(i * 255.0 / imw, j * 255.0 / imh, 0));
            }
        }
    }

    std::unique_ptr<juce::MenuBarModel> menuBar;

  private:
    juce::AudioDeviceManager audioDeviceManager;

    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::Label>> labels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCComponent)
};

//==============================================================================
template <typename osc_t> class OSCApplication : public juce::JUCEApplication
{
  public:
    //==============================================================================
    OSCApplication() {}

    // We inject these as compile definitions from the CMakeLists.txt
    // If you've enabled the juce header with `juce_generate_juce_header(<thisTarget>)`
    // you could `#include <JuceHeader.h>` and use `ProjectInfo::projectName` etc. instead.
    const juce::String getApplicationName() override { return "OSC Juce Application"; }
    const juce::String getApplicationVersion() override { return "0.0.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    //==============================================================================
    void initialise(const juce::String &commandLine) override
    {
        // This method is where you should put your application's initialisation code..
        juce::ignoreUnused(commandLine);

        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted(const juce::String &commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
        juce::ignoreUnused(commandLine);
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our OSCComponent class.
    */
    class MainWindow : public juce::DocumentWindow
    {
      public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                                 ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            auto oc = new OSCComponent<osc_t>();

            setContentOwned(oc, true);
            setMenuBar(oc->menuBar.get());
#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
            centreWithSize(1100, 700);
#endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

      private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

  private:
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace oscillators_testclients
} // namespace sst
#endif // SST_OSCILLATORS_MIT_APIJUCECOMPONENT_H
