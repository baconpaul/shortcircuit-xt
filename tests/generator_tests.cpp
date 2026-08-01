/*
 * Shortcircuit XT - a Surge Synth Team product
 *
 * A fully featured creative sampler, available as a standalone
 * and plugin for multiple platforms.
 *
 * Copyright 2019 - 2026, Various authors, as described in the github
 * transaction log.
 *
 * This source file and all other files in the shortcircuit-xt repo outside of
 * `libs/` are licensed under the MIT license, available in the
 * file LICENSE or at https://opensource.org/license/mit.
 *
 * As some dependencies of ShortcircuitXT are released under the GNU General
 * Public License 3, if you distribute a binary of ShortcircuitXT
 * without breaking those dependencies, the combined work must be
 * distributed under GPL3.
 *
 * ShortcircuitXT is inspired by, and shares a small amount of code with,
 * the commercial product Shortcircuit 1 and 2, released by VemberTech
 * in the mid 2000s. The code for Shortcircuit 2 was opensourced in
 * 2020 at the outset of this project.
 *
 * All source for ShortcircuitXT is available at
 * https://github.com/surge-synthesizer/shortcircuit-xt
 */

#include "catch2/catch2.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "configuration.h"
#include "dsp/data_tables.h"
#include "dsp/generator.h"
#include "dsp/resampling.h"

/*
 * Golden tests for the sample generator.
 *
 * The generator is a pure function of (GeneratorState, GeneratorIO), so these drive
 * it directly - no engine, no voice, no zone. See the block comment at the top of
 * dsp/generator.cpp for the memory layout contract these fixtures have to honour.
 *
 * The important property of every capture here is that it SPANS AT LEAST TWO LOOP
 * SEAMS. A golden that sits in the middle of a loop agrees with itself forever while
 * the seam behaviour rots, which is exactly the bug class this file exists to catch.
 * checkSpannedSeams() enforces it rather than trusting the constants to stay put.
 *
 * To regenerate after an intentional change:
 *     SCXT_PRINT_GOLDEN=1 ./build/tests/scxt-test "[generator][golden]"
 * and paste the printed arrays back over the expected{} initializers.
 */

namespace
{

constexpr double goldenSampleRate{48000.0};
constexpr double goldenPi{3.14159265358979323846};

// A short sample with a short loop, so seams arrive every 64 samples rather than
// every few seconds. blockSize is 16, so a 64 sample loop also puts seams at four
// different offsets within the block as the capture proceeds.
constexpr int gWaveSize{2048};
constexpr int gStartSample{0};
constexpr int gEndSample{512};
constexpr int gStartLoop{256};
constexpr int gEndLoop{320};
constexpr int gLoopLen{gEndLoop - gStartLoop}; // 64
constexpr int gFade{32};                       // 2 blocks worth, spans a block boundary

constexpr float goldenTolerance{1e-5f};

constexpr int32_t unityRatio{1 << 24};

bool goldenPrintMode() { return std::getenv("SCXT_PRINT_GOLDEN") != nullptr; }

/*
 * Two tones, switching at startLoop, so the loop seam is a genuine discontinuity and
 * the crossfade has distinguishable material on each side of it. Neither frequency
 * divides the loop length, so the loop is not accidentally seamless.
 */
float testSignal(int n)
{
    auto f = (n < gStartLoop) ? 700.0 : 1900.0;
    return (float)(0.7 * std::sin(2.0 * goldenPi * f * n / goldenSampleRate));
}

struct GenConfig
{
    std::string name;
    bool stereo{false};
    bool isFloat{true};
    bool loopActive{true};
    bool loopForward{true}; // FORWARD_ONLY as opposed to ALTERNATE_DIRECTIONS
    bool loopWhileGated{false};
    bool playReverse{false};
    int32_t loopFade{0};
    int32_t ratio{unityRatio};
    scxt::dsp::InterpolationTypes interp{scxt::dsp::InterpolationTypes::Sinc};
    int warmupBlocks{32};
    int recordBlocks{10};
    int ungateAtBlock{-1}; // relative to the start of the recorded region
    int startPos{-1};      // -1 means the natural start for the direction
};

struct GenHarness
{
    std::vector<float> bufFL, bufFR;
    std::vector<int16_t> bufIL, bufIR;

    scxt::dsp::GeneratorState gs;
    scxt::dsp::GeneratorIO io;
    scxt::dsp::GeneratorFPtr fn{nullptr};

    float outL alignas(16)[scxt::blockSize * 2];
    float outR alignas(16)[scxt::blockSize * 2];

    std::vector<float> recL, recR;

    // seam bookkeeping, filled in by run()
    int seamsInRecord{0};

    explicit GenHarness(const GenConfig &c)
    {
        // The sinc kernels read dsp::sincTable, which is otherwise only initialized by
        // the Engine constructor. Without this the Sinc paths silently produce silence.
        static bool tablesReady = []() {
            scxt::dsp::sincTable.init();
            return true;
        }();
        (void)tablesReady;

        const auto pad = scxt::dsp::FIRoffset;

        bufFL.assign(gWaveSize + scxt::dsp::FIRipol_N, 0.f);
        bufFR.assign(gWaveSize + scxt::dsp::FIRipol_N, 0.f);
        bufIL.assign(gWaveSize + scxt::dsp::FIRipol_N, 0);
        bufIR.assign(gWaveSize + scxt::dsp::FIRipol_N, 0);
        for (int i = 0; i < gWaveSize; ++i)
        {
            auto v = testSignal(i);
            bufFL[i + pad] = v;
            bufFR[i + pad] = -v; // inverted so a channel mixup is visible
            bufIL[i + pad] = (int16_t)std::lround(v * 32767.f);
            bufIR[i + pad] = (int16_t)-std::lround(v * 32767.f);
        }

        if (c.isFloat)
        {
            io.sampleDataL = (void *)(bufFL.data() + pad);
            io.sampleDataR = (void *)(bufFR.data() + pad);
        }
        else
        {
            io.sampleDataL = (void *)(bufIL.data() + pad);
            io.sampleDataR = (void *)(bufIR.data() + pad);
        }
        io.outputL = outL;
        io.outputR = outR;
        io.waveSize = gWaveSize;

        gs = scxt::dsp::GeneratorState{};
        gs.playbackLowerBound = gStartSample;
        gs.playbackUpperBound = gEndSample;
        gs.playbackInvertedBounds = 1.f / std::max(1, gEndSample - gStartSample);
        gs.loopLowerBound = c.loopActive ? gStartLoop : gStartSample;
        gs.loopUpperBound = c.loopActive ? gEndLoop : gEndSample;
        gs.loopInvertedBounds = 1.f / std::max(1, gs.loopUpperBound - gs.loopLowerBound);
        gs.loopFade = c.loopFade;
        gs.ratio = c.ratio;
        gs.blockSize = scxt::blockSize;
        gs.isFinished = false;
        gs.gated = true;
        gs.loopCount = -1;
        gs.interpolationType = c.interp;
        gs.samplePos = c.playReverse ? gs.playbackUpperBound : gs.playbackLowerBound;
        if (c.startPos >= 0)
            gs.samplePos = c.startPos;
        gs.sampleSubPos = 0;
        gs.loopDirection = c.playReverse ? -1 : 1;
        gs.directionAtOutset = gs.loopDirection;
        // the generator derives this per block; seed it so a capture with no warmup
        // does not read a zero and count a spurious turnaround on its first block
        gs.travelDirection = gs.loopDirection * (c.ratio < 0 ? -1 : 1);

        fn = scxt::dsp::GetFPtrGeneratorSample(c.stereo, c.isFloat, c.loopActive, c.loopForward,
                                               c.loopWhileGated);
    }

    void run(const GenConfig &c)
    {
        for (int b = 0; b < c.warmupBlocks; ++b)
            fn(&gs, &io);

        recL.clear();
        recR.clear();
        seamsInRecord = 0;

        // A seam is either a wrap (position jumps by much more than one block of
        // travel) or a turnaround (direction flips). Detecting it from the state
        // rather than from loopCount keeps this honest across all five loop modes.
        auto travelPerBlock = std::abs((double)c.ratio / (double)unityRatio) * gs.blockSize;
        auto prevPos = gs.samplePos;
        auto prevDir = gs.travelDirection;

        for (int b = 0; b < c.recordBlocks; ++b)
        {
            if (b == c.ungateAtBlock)
                gs.gated = false;

            fn(&gs, &io);

            for (int i = 0; i < gs.blockSize; ++i)
            {
                recL.push_back(outL[i]);
                recR.push_back(outR[i]);
            }

            if (std::abs(gs.samplePos - prevPos) > travelPerBlock + 2 ||
                gs.travelDirection != prevDir)
                seamsInRecord++;
            prevPos = gs.samplePos;
            prevDir = gs.travelDirection;
        }
    }
};

/*
 * The whole point: assert the capture actually crossed loop seams. If a later edit
 * to the loop bounds, the ratio or the record length quietly turns one of these into
 * a mid-loop capture, this fires instead of the golden silently going soft.
 */
void checkSpannedSeams(const GenConfig &c, const GenHarness &h)
{
    if (!c.loopActive)
        return;
    INFO("config " << c.name << " crossed " << h.seamsInRecord << " seams in "
                   << c.recordBlocks * scxt::blockSize << " recorded samples");
    REQUIRE(h.seamsInRecord >= 2);
}

void goldenCheckOrPrint(const std::string &label, const std::vector<float> &got,
                        const std::vector<float> &expected)
{
    if (goldenPrintMode())
    {
        std::printf("// %s\n        {", label.c_str());
        for (size_t i = 0; i < got.size(); ++i)
        {
            if (i > 0 && i % 5 == 0)
                std::printf("\n         ");
            std::printf("%.9ff%s", got[i], i + 1 < got.size() ? ", " : "");
        }
        std::printf("};\n");
        return;
    }

    REQUIRE(got.size() == expected.size());
    for (size_t i = 0; i < got.size(); ++i)
    {
        INFO(label << " sample[" << i << "]: got=" << got[i] << " expected=" << expected[i]);
        REQUIRE(got[i] == Approx(expected[i]).margin(goldenTolerance));
    }
}

// Drive a config and hand back the left channel, having asserted the seam coverage.
std::vector<float> capture(const GenConfig &c, GenHarness &h)
{
    h.run(c);
    checkSpannedSeams(c, h);
    return h.recL;
}

} // namespace

TEST_CASE("Generator harness honours the layout contract", "[generator]")
{
    // A no-loop forward pass should reproduce the sample data at unity ratio. This is
    // the sanity check that the FIRoffset padding, the read pointer setup and the
    // output indexing in the fixture all line up; if this is wrong every golden below
    // is measuring the fixture rather than the generator.
    //
    // Note the generator's indexing convention: the read pointer is
    // SampleData + samplePos - FIRoffset and the kernels take their interpolation
    // point at FIRoffset - 1, so samplePos p at zero subposition reads data[p - 1].
    // Output n therefore carries sample n - 1, and output 0 comes from the leading
    // zero pad.
    GenConfig c;
    c.name = "contract";
    c.loopActive = false;
    c.warmupBlocks = 0;
    c.recordBlocks = 8;
    c.interp = scxt::dsp::InterpolationTypes::ZeroOrderHold;

    GenHarness h(c);
    h.run(c);

    REQUIRE(h.recL.size() == 8 * scxt::blockSize);
    REQUIRE(h.recL[0] == Approx(0.f).margin(1e-9));
    for (int i = 1; i < 8 * scxt::blockSize; ++i)
    {
        INFO("sample " << i);
        REQUIRE(h.recL[i] == Approx(testSignal(i - 1)).margin(1e-6));
    }
}

TEST_CASE("Generator no-loop playback terminates at the bounds", "[generator]")
{
    SECTION("forward")
    {
        GenConfig c;
        c.name = "noloop-fwd-terminate";
        c.loopActive = false;
        c.warmupBlocks = 0;
        c.recordBlocks = 40; // 640 samples over a 512 sample playback region

        GenHarness h(c);
        h.run(c);

        REQUIRE(h.gs.isFinished);
        REQUIRE(h.gs.samplePos == gEndSample);
        // everything past the end of the region is silence
        for (int i = gEndSample + 1; i < (int)h.recL.size(); ++i)
        {
            INFO("sample " << i);
            REQUIRE(h.recL[i] == Approx(0.f).margin(1e-9));
        }
    }

    SECTION("reverse")
    {
        GenConfig c;
        c.name = "noloop-rev-terminate";
        c.loopActive = false;
        c.playReverse = true;
        c.warmupBlocks = 0;
        c.recordBlocks = 40;

        GenHarness h(c);
        h.run(c);

        REQUIRE(h.gs.isFinished);
        REQUIRE(h.gs.samplePos == gStartSample);
        for (int i = gEndSample + 1; i < (int)h.recL.size(); ++i)
        {
            INFO("sample " << i);
            REQUIRE(h.recL[i] == Approx(0.f).margin(1e-9));
        }
    }
}

TEST_CASE("Generator terminates under a negative playback ratio", "[generator]")
{
    /*
     * A negative ratio means the playhead travels opposite to loopDirection. The
     * end-of-playback tests have to ask which way we are actually moving; asking
     * loopDirection instead clamps the position at the bound and never finishes, so
     * the voice hangs. Both sections run to a bound loopDirection alone would not
     * predict.
     */
    SECTION("forward loop direction, negative ratio, travelling down")
    {
        GenConfig c;
        c.name = "negratio-down";
        c.loopActive = false;
        c.ratio = -unityRatio;
        c.startPos = 256;
        c.warmupBlocks = 0;
        c.recordBlocks = 24; // 384 samples for a 256 sample descent

        GenHarness h(c);
        h.run(c);

        REQUIRE(h.gs.travelDirection == -1);
        REQUIRE(h.gs.loopDirection == 1); // unchanged: the ratio sign is not the loop's business
        REQUIRE(h.gs.samplePos == gStartSample);
        REQUIRE(h.gs.isFinished);
    }

    SECTION("reverse loop direction, negative ratio, travelling up")
    {
        GenConfig c;
        c.name = "negratio-up";
        c.loopActive = false;
        c.playReverse = true;
        c.ratio = -unityRatio;
        c.startPos = 256;
        c.warmupBlocks = 0;
        c.recordBlocks = 24;

        GenHarness h(c);
        h.run(c);

        REQUIRE(h.gs.travelDirection == 1);
        REQUIRE(h.gs.loopDirection == -1);
        REQUIRE(h.gs.samplePos == gEndSample);
        REQUIRE(h.gs.isFinished);
    }
}

TEST_CASE("Generator stereo tracks the mono path per channel", "[generator]")
{
    // The right buffer is the left negated, so a correct stereo instantiation gives
    // outR == -outL exactly. Cheap way to cover the stereo kernels without doubling
    // every golden array.
    GenConfig c;
    c.name = "stereo-mirror";
    c.stereo = true;
    c.loopFade = gFade;

    GenHarness h(c);
    h.run(c);
    checkSpannedSeams(c, h);

    REQUIRE(h.recL.size() == h.recR.size());
    REQUIRE(h.recL.size() > 0);
    for (size_t i = 0; i < h.recL.size(); ++i)
    {
        INFO("sample " << i);
        REQUIRE(h.recR[i] == Approx(-h.recL[i]).margin(1e-7));
    }
}

/*
 * The goldens.
 *
 * Only configurations believed correct today are captured here. Reverse-with-fade
 * and alternate-with-fade are deliberately absent: they are broken (issue #2149) and
 * capturing them now would enshrine the bug. They get goldens once fixed.
 */

TEST_CASE("Generator golden - no loop", "[generator][golden]")
{
    struct Case
    {
        const char *name;
        bool reverse;
    };
    // 128 warmup samples, then 512 recorded over a 512 sample playback region: the
    // capture brackets the moment the playhead reaches the far bound and the voice
    // finishes, so the trailing silence is part of the golden.
    static const std::array<Case, 2> cases{{{"noloop-fwd", false}, {"noloop-rev", true}}};

    static const std::array<std::vector<float>, 2> expected{{
        // noloop-fwd
        {-0.560877740f, -0.520201385f, -0.475160539f, -0.426133007f, -0.373530179f, -0.317793339f,
         -0.259390235f, -0.198810771f, -0.136563271f, -0.073169954f, -0.009162744f, 0.054921340f,
         0.118544631f,  0.181173295f,  0.242281929f,  0.301357746f,  0.357905120f,  0.411449671f,
         0.461542070f,  0.507762015f,  0.549721837f,  0.587069333f,  0.619491339f,  0.646715581f,
         0.668513954f,  0.684703350f,  0.695147932f,  0.699760199f,  0.698501348f,  0.691381812f,
         0.678461611f,  0.659848988f,  0.635700285f,  0.606217921f,  0.571649134f,  0.532284260f,
         0.488453358f,  0.440524310f,  0.388899177f,  0.334011137f,  0.276320726f,  0.216311932f,
         0.154488221f,  0.091368362f,  0.027481899f,  -0.036635142f, -0.100444816f, -0.163411736f,
         -0.225007623f, -0.284715623f, -0.342034906f, -0.396484345f, -0.447607279f, -0.494974732f,
         -0.538189292f, -0.576888323f, -0.610747159f, -0.639481843f, -0.662851095f, -0.680658937f,
         -0.692755938f, -0.699040592f, -0.699460387f, -0.694011509f, -0.682739556f, -0.665739596f,
         -0.643153846f, -0.615171969f, -0.582028747f, -0.544002175f, -0.501411438f, -0.454613626f,
         -0.404001653f, -0.349999994f, -0.293061823f, -0.233664826f, -0.172307342f, -0.109504148f,
         -0.045782220f, 0.018323835f,  0.082276158f,  0.145538166f,  0.207579091f,  0.267878413f,
         0.325930178f,  0.381247312f,  0.433365762f,  0.481848240f,  0.526287854f,  0.566311896f,
         0.601584435f,  0.631809711f,  0.656733930f,  0.676147997f,  0.689889193f,  0.697842062f,
         0.699940085f,  0.696165323f,  0.686549723f,  0.671173811f,  0.650166750f,  0.623704612f,
         0.592009485f,  0.555347383f,  0.514025807f,  0.468391448f,  0.418827206f,  0.365749002f,
         0.309602082f,  0.250857592f,  0.190008342f,  0.127564877f,  0.064051166f,  0.000000026f,
         -0.064051107f, -0.127564833f, -0.190008298f, -0.250857562f, -0.309602052f, -0.365748972f,
         -0.418827176f, -0.468391478f, -0.514004171f, -0.555640042f, -0.590347648f, -0.629353344f,
         -0.637001932f, -0.693634152f, -0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,
         0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,
         0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f,
         -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f,
         -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,
         0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,
         0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f,
         -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f,
         -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,
         0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,
         0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f,
         -0.501411378f, -0.606217802f, -0.673718631f, -0.699760139f, -0.682739615f, -0.623704612f,
         -0.526287854f, -0.396484405f, -0.242281988f, -0.073169962f, 0.100444809f,  0.267878383f,
         0.418827176f,  0.544002175f,  0.635700226f,  0.688278437f,  0.698501229f,  0.665739536f,
         0.592009544f,  0.481848240f,  0.342034936f,  0.181173369f,  0.009162753f,  -0.163411722f,
         -0.325930148f, -0.468391359f, -0.582028687f, -0.659849048f, -0.697063446f, -0.691381872f,
         -0.643153787f, -0.555347383f, -0.433365792f, -0.284715652f, -0.118544683f, 0.054921336f,
         0.225007594f,  0.381247282f,  0.514025748f,  0.615171969f,  0.678461671f,  0.699999988f,
         0.678461671f,  0.615172029f,  0.514025807f,  0.381247342f,  0.225007668f,  0.054921407f,
         -0.118544623f, -0.284715623f, -0.433365732f, -0.555347383f, -0.643153787f, -0.691381872f,
         -0.697063506f, -0.659849048f, -0.582028747f, -0.468391418f, -0.325930238f, -0.163411796f,
         0.009162681f,  0.181173295f,  0.342034847f,  0.481848180f,  0.592009485f,  0.665739536f,
         0.698501229f,  0.688278437f,  0.635700166f,  0.544002235f,  0.418827236f,  0.267878413f,
         0.100444868f,  -0.073169887f, -0.242281914f, -0.396484345f, -0.526287854f, -0.623704553f,
         -0.682739615f, -0.699760139f, -0.673718691f, -0.606217861f, -0.501411438f, -0.365749061f,
         -0.207579151f, -0.036635205f, 0.136563197f,  0.301357746f,  0.447607249f,  0.566311955f,
         0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,  0.571649075f,  0.454613656f,
         0.309602112f,  0.145538211f,  -0.027481835f, -0.198810726f, -0.357905120f, -0.494974703f,
         -0.601584435f, -0.671173751f, -0.699460328f, -0.684703350f, -0.627810955f, -0.532284260f,
         -0.404001683f, -0.250857592f, -0.082276218f, 0.091368288f,  0.259390175f,  0.411449641f,
         0.538189232f,  0.631809711f,  0.686549723f,  0.699040651f,  0.668513954f,  0.596848190f,
         0.488453329f,  0.350000024f,  0.190008357f,  0.018323898f,  -0.154488161f, -0.317793280f,
         -0.461542070f, -0.576888323f, -0.656733930f, -0.696165383f, -0.692755938f, -0.646715701f,
         -0.560877740f, -0.440524340f, -0.293061882f, -0.127564892f, 0.045782156f,  0.216311887f,
         0.373530120f,  0.507762015f,  0.610747218f,  0.676148117f,  0.699940026f,  0.680658937f,
         0.619491398f,  0.520201445f,  0.388899207f,  0.233664840f,  0.064051174f,  -0.109504096f,
         -0.276320696f, -0.426132947f, -0.549721837f, -0.639481902f, -0.689889252f, -0.697842062f,
         -0.662851095f, -0.587069392f, -0.475160539f, -0.334011167f, -0.172307342f, -0.000000036f,
         0.172307268f,  0.334011108f,  0.475160480f,  0.587069333f,  0.662851095f,  0.697842062f,
         0.689889312f,  0.639481902f,  0.549721897f,  0.426133037f,  0.276320755f,  0.109504163f,
         -0.064051099f, -0.233664766f, -0.388899148f, -0.520201445f, -0.619491339f, -0.680658937f,
         -0.699940026f, -0.676148057f, -0.610747218f, -0.507762074f, -0.373530179f, -0.216311961f,
         -0.045782227f, 0.127564833f,  0.293061823f,  0.440524280f,  0.560877681f,  0.646715701f,
         0.692755938f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f},
        // noloop-rev
        {0.592009485f,  0.481848180f,  0.342034847f,  0.181173295f,  0.009162681f,  -0.163411796f,
         -0.325930238f, -0.468391418f, -0.582028747f, -0.659849048f, -0.697063506f, -0.691381872f,
         -0.643153787f, -0.555347383f, -0.433365732f, -0.284715623f, -0.118544623f, 0.054921407f,
         0.225007668f,  0.381247342f,  0.514025807f,  0.615172029f,  0.678461671f,  0.699999988f,
         0.678461671f,  0.615171969f,  0.514025748f,  0.381247282f,  0.225007594f,  0.054921336f,
         -0.118544683f, -0.284715652f, -0.433365792f, -0.555347383f, -0.643153787f, -0.691381872f,
         -0.697063446f, -0.659849048f, -0.582028687f, -0.468391359f, -0.325930148f, -0.163411722f,
         0.009162753f,  0.181173369f,  0.342034936f,  0.481848240f,  0.592009544f,  0.665739536f,
         0.698501229f,  0.688278437f,  0.635700226f,  0.544002175f,  0.418827176f,  0.267878383f,
         0.100444809f,  -0.073169962f, -0.242281988f, -0.396484405f, -0.526287854f, -0.623704612f,
         -0.682739615f, -0.699760139f, -0.673718631f, -0.606217802f, -0.501411378f, -0.365748972f,
         -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,  0.447607338f,  0.566311955f,
         0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,  0.571649075f,  0.454613626f,
         0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f, -0.357905179f, -0.494974732f,
         -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f, -0.627810895f, -0.532284200f,
         -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,  0.259390265f,  0.411449730f,
         0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,  0.668513954f,  0.596848071f,
         0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,  -0.154488236f, -0.317793339f,
         -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f, -0.692755938f, -0.646715701f,
         -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f, 0.045782227f,  0.216311961f,
         0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,  0.705242038f,  0.668064654f,
         0.641370535f,  0.491449922f,  -0.657547593f, -0.693634152f, -0.637001932f, -0.629353344f,
         -0.590347648f, -0.555640042f, -0.514004171f, -0.468391478f, -0.418827176f, -0.365748972f,
         -0.309602052f, -0.250857562f, -0.190008298f, -0.127564833f, -0.064051107f, 0.000000026f,
         0.064051166f,  0.127564877f,  0.190008342f,  0.250857592f,  0.309602082f,  0.365749002f,
         0.418827206f,  0.468391448f,  0.514025807f,  0.555347383f,  0.592009485f,  0.623704612f,
         0.650166750f,  0.671173811f,  0.686549723f,  0.696165323f,  0.699940085f,  0.697842062f,
         0.689889193f,  0.676147997f,  0.656733930f,  0.631809711f,  0.601584435f,  0.566311896f,
         0.526287854f,  0.481848240f,  0.433365762f,  0.381247312f,  0.325930178f,  0.267878413f,
         0.207579091f,  0.145538166f,  0.082276158f,  0.018323835f,  -0.045782220f, -0.109504148f,
         -0.172307342f, -0.233664826f, -0.293061823f, -0.349999994f, -0.404001653f, -0.454613626f,
         -0.501411438f, -0.544002175f, -0.582028747f, -0.615171969f, -0.643153846f, -0.665739596f,
         -0.682739556f, -0.694011509f, -0.699460387f, -0.699040592f, -0.692755938f, -0.680658937f,
         -0.662851095f, -0.639481843f, -0.610747159f, -0.576888323f, -0.538189292f, -0.494974732f,
         -0.447607279f, -0.396484345f, -0.342034906f, -0.284715623f, -0.225007623f, -0.163411736f,
         -0.100444816f, -0.036635142f, 0.027481899f,  0.091368362f,  0.154488221f,  0.216311932f,
         0.276320726f,  0.334011137f,  0.388899177f,  0.440524310f,  0.488453358f,  0.532284260f,
         0.571649134f,  0.606217921f,  0.635700285f,  0.659848988f,  0.678461611f,  0.691381812f,
         0.698501348f,  0.699760199f,  0.695147932f,  0.684703350f,  0.668513954f,  0.646715581f,
         0.619491339f,  0.587069333f,  0.549721837f,  0.507762015f,  0.461542070f,  0.411449671f,
         0.357905120f,  0.301357746f,  0.242281929f,  0.181173295f,  0.118544631f,  0.054921340f,
         -0.009162744f, -0.073169954f, -0.136563271f, -0.198810771f, -0.259390235f, -0.317793339f,
         -0.373530179f, -0.426133007f, -0.475160539f, -0.520201385f, -0.560877740f, -0.596848130f,
         -0.627810955f, -0.653506339f, -0.673718750f, -0.688278496f, -0.697063565f, -0.700000048f,
         -0.697063446f, -0.688278377f, -0.673718750f, -0.653506339f, -0.627810895f, -0.596848071f,
         -0.560877681f, -0.520201385f, -0.475160509f, -0.426132977f, -0.373530149f, -0.317793339f,
         -0.259390175f, -0.198810726f, -0.136563212f, -0.073169902f, -0.009162690f, 0.054921392f,
         0.118544683f,  0.181173354f,  0.242281988f,  0.301357776f,  0.357905179f,  0.411449671f,
         0.461542100f,  0.507762134f,  0.549721897f,  0.587069392f,  0.619491339f,  0.646715701f,
         0.668514013f,  0.684703410f,  0.695148051f,  0.699760258f,  0.698501289f,  0.691381752f,
         0.678461611f,  0.659848988f,  0.635700226f,  0.606217861f,  0.571649075f,  0.532284200f,
         0.488453329f,  0.440524280f,  0.388899177f,  0.334011108f,  0.276320696f,  0.216311887f,
         0.154488176f,  0.091368303f,  0.027481845f,  -0.036635194f, -0.100444868f, -0.163411781f,
         -0.225007653f, -0.284715652f, -0.342034936f, -0.396484375f, -0.447607309f, -0.494974762f,
         -0.538189352f, -0.576888382f, -0.610747278f, -0.639481902f, -0.662851155f, -0.680659056f,
         -0.692755997f, -0.699040651f, -0.699460328f, -0.694011450f, -0.682739556f, -0.665739477f,
         -0.643153787f, -0.615171909f, -0.582028687f, -0.544002116f, -0.501411378f, -0.454613596f,
         -0.404001653f, -0.349999964f, -0.293061793f, -0.233664796f, -0.172307283f, -0.109504104f,
         -0.045782164f, 0.018323889f,  0.082276210f,  0.145538211f,  0.207579136f,  0.267878413f,
         0.325930208f,  0.381247342f,  0.433365792f,  0.481848240f,  0.526287854f,  0.566311955f,
         0.601584494f,  0.631809771f,  0.656733990f,  0.676148057f,  0.689889252f,  0.697842181f,
         0.699940026f,  0.696165323f,  0.686549664f,  0.671173811f,  0.650166750f,  0.623704553f,
         0.592009425f,  0.555347264f,  0.514025748f,  0.468391418f,  0.418827176f,  0.365750134f,
         0.309587687f,  0.250932038f,  0.189778060f,  0.128050908f,  0.063304774f,  0.000858521f,
         -0.000746359f, 0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,  0.000000000f,
         0.000000000f,  0.000000000f},
    }};

    for (size_t ci = 0; ci < cases.size(); ++ci)
    {
        GenConfig c;
        c.name = cases[ci].name;
        c.loopActive = false;
        c.playReverse = cases[ci].reverse;
        c.warmupBlocks = 8;
        c.recordBlocks = 32;

        GenHarness h(c);
        goldenCheckOrPrint(c.name, capture(c, h), expected[ci]);
    }
}

TEST_CASE("Generator golden - looping without fade", "[generator][golden]")
{
    struct Case
    {
        const char *name;
        bool loopForward;
        bool reverse;
        int recordBlocks;
    };
    static const std::array<Case, 4> cases{{
        {"loop-fwd-nofade", true, false, 10},
        {"loop-rev-nofade", true, true, 10},
        {"loop-alt-nofade", false, false, 20},
        {"loop-alt-rev-nofade", false, true, 20},
    }};

    static const std::array<std::vector<float>, 4> expected{{
        // loop-fwd-nofade
        {-0.501411378f, 0.491449922f,  0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,
         0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f,
         -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f,
         -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,
         0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,
         0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,
         -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f,
         -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f,
         -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,
         0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,
         0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f, -0.501411378f, 0.491449922f,
         0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,
         0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f,
         -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f,
         -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,
         0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,
         0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f,
         -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f,
         -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,
         0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,
         0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f,
         -0.207579061f, -0.365748972f, -0.501411378f, 0.491449922f,  0.641370535f,  0.668064654f,
         0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,
         0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f,
         -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f,
         -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,
         0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f},
        // loop-rev-nofade
        {-0.657547593f, -0.365748972f, -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,
         0.447607338f,  0.566311955f,  0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,
         0.571649075f,  0.454613626f,  0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f,
         -0.357905179f, -0.494974732f, -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f,
         -0.627810895f, -0.532284200f, -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,
         0.259390265f,  0.411449730f,  0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,
         0.668513954f,  0.596848071f,  0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,
         -0.154488236f, -0.317793339f, -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f,
         -0.692755938f, -0.646715701f, -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f,
         0.045782227f,  0.216311961f,  0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,
         0.705242038f,  0.668064654f,  0.641370535f,  0.491449922f,  -0.657547593f, -0.365748972f,
         -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,  0.447607338f,  0.566311955f,
         0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,  0.571649075f,  0.454613626f,
         0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f, -0.357905179f, -0.494974732f,
         -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f, -0.627810895f, -0.532284200f,
         -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,  0.259390265f,  0.411449730f,
         0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,  0.668513954f,  0.596848071f,
         0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,  -0.154488236f, -0.317793339f,
         -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f, -0.692755938f, -0.646715701f,
         -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f, 0.045782227f,  0.216311961f,
         0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,  0.705242038f,  0.668064654f,
         0.641370535f,  0.491449922f,  -0.657547593f, -0.365748972f, -0.207579061f, -0.036635134f,
         0.136563271f,  0.301357806f,  0.447607338f,  0.566311955f,  0.650166690f,  0.694011331f,
         0.695147991f,  0.653506219f,  0.571649075f,  0.454613626f,  0.309602082f,  0.145538136f,
         -0.027481908f, -0.198810786f, -0.357905179f, -0.494974732f, -0.601584554f, -0.671173811f,
         -0.699460328f, -0.684703350f, -0.627810895f, -0.532284200f, -0.404001623f, -0.250857532f,
         -0.082276143f, 0.091368362f,  0.259390265f,  0.411449730f},
        // loop-alt-nofade
        {-0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,
         0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f,
         -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f,
         -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,
         0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,
         0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,
         -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f,
         -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f,
         -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,
         0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,
         0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f, -0.501411378f, -0.365748972f,
         -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,  0.447607338f,  0.566311955f,
         0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,  0.571649075f,  0.454613626f,
         0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f, -0.357905179f, -0.494974732f,
         -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f, -0.627810895f, -0.532284200f,
         -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,  0.259390265f,  0.411449730f,
         0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,  0.668513954f,  0.596848071f,
         0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,  -0.154488236f, -0.317793339f,
         -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f, -0.692755938f, -0.646715701f,
         -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f, 0.045782227f,  0.216311961f,
         0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,  0.705242038f,  0.668064654f,
         0.641370535f,  0.491449922f,  -0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,
         0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,
         0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f,
         -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f,
         -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,
         0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,
         0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f,
         -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f,
         -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,
         0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,
         0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f,
         -0.501411378f, -0.365748972f, -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,
         0.447607338f,  0.566311955f,  0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,
         0.571649075f,  0.454613626f,  0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f,
         -0.357905179f, -0.494974732f, -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f,
         -0.627810895f, -0.532284200f, -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,
         0.259390265f,  0.411449730f,  0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,
         0.668513954f,  0.596848071f,  0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,
         -0.154488236f, -0.317793339f, -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f,
         -0.692755938f, -0.646715701f, -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f,
         0.045782227f,  0.216311961f,  0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,
         0.705242038f,  0.668064654f,  0.641370535f,  0.491449922f,  -0.657547593f, 0.491449922f,
         0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,
         0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f,
         -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f,
         -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,
         0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,
         0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f,
         -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f,
         -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,
         0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,
         0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f,
         -0.207579061f, -0.365748972f},
        // loop-alt-rev-nofade
        {-0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,
         0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f,
         -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f,
         -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,
         0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,
         0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,
         -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f,
         -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f,
         -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,
         0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,
         0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f, -0.501411378f, -0.365748972f,
         -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,  0.447607338f,  0.566311955f,
         0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,  0.571649075f,  0.454613626f,
         0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f, -0.357905179f, -0.494974732f,
         -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f, -0.627810895f, -0.532284200f,
         -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,  0.259390265f,  0.411449730f,
         0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,  0.668513954f,  0.596848071f,
         0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,  -0.154488236f, -0.317793339f,
         -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f, -0.692755938f, -0.646715701f,
         -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f, 0.045782227f,  0.216311961f,
         0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,  0.705242038f,  0.668064654f,
         0.641370535f,  0.491449922f,  -0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,
         0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,
         0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f,
         -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f,
         -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,
         0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,
         0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f,
         -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f,
         -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,
         0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,
         0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f,
         -0.501411378f, -0.365748972f, -0.207579061f, -0.036635134f, 0.136563271f,  0.301357806f,
         0.447607338f,  0.566311955f,  0.650166690f,  0.694011331f,  0.695147991f,  0.653506219f,
         0.571649075f,  0.454613626f,  0.309602082f,  0.145538136f,  -0.027481908f, -0.198810786f,
         -0.357905179f, -0.494974732f, -0.601584554f, -0.671173811f, -0.699460328f, -0.684703350f,
         -0.627810895f, -0.532284200f, -0.404001623f, -0.250857532f, -0.082276143f, 0.091368362f,
         0.259390265f,  0.411449730f,  0.538189352f,  0.631809771f,  0.686549723f,  0.699040651f,
         0.668513954f,  0.596848071f,  0.488453329f,  0.349999964f,  0.190008298f,  0.018323826f,
         -0.154488236f, -0.317793339f, -0.461542130f, -0.576888382f, -0.656733930f, -0.696165383f,
         -0.692755938f, -0.646715701f, -0.560877681f, -0.440524280f, -0.293061823f, -0.127564833f,
         0.045782227f,  0.216311961f,  0.373530179f,  0.507742941f,  0.611010611f,  0.674619675f,
         0.705242038f,  0.668064654f,  0.641370535f,  0.491449922f,  -0.657547593f, 0.491449922f,
         0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,
         0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f,
         -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f,
         -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,
         0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,
         0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f,
         -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f,
         -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,
         0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,
         0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f,
         -0.207579061f, -0.365748972f},
    }};

    for (size_t ci = 0; ci < cases.size(); ++ci)
    {
        GenConfig c;
        c.name = cases[ci].name;
        c.loopForward = cases[ci].loopForward;
        c.playReverse = cases[ci].reverse;
        c.recordBlocks = cases[ci].recordBlocks;

        GenHarness h(c);
        goldenCheckOrPrint(c.name, capture(c, h), expected[ci]);
    }
}

TEST_CASE("Generator golden - gated loop and release", "[generator][golden]")
{
    struct Case
    {
        const char *name;
        int ungateAtBlock;
    };
    static const std::array<Case, 2> cases{{{"gated-loop-held", -1}, {"gated-loop-release", 4}}};

    static const std::array<std::vector<float>, 2> expected{{
        // gated-loop-held
        {-0.501411378f, 0.491449922f,  0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,
         0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f,
         -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f,
         -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,
         0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,
         0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,
         -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f,
         -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f,
         -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,
         0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,
         0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f, -0.501411378f, 0.491449922f,
         0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,
         0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f,
         -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f,
         -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,
         0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,
         0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f,
         -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f,
         -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,
         0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,
         0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f,
         -0.207579061f, -0.365748972f, -0.501411378f, 0.491449922f,  0.641370535f,  0.668064654f,
         0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,
         0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f,
         -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f,
         -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,
         0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,
         0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f,
         -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f,
         -0.357905179f, -0.198810786f, -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,
         0.571649075f,  0.653506219f,  0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,
         0.447607338f,  0.301357806f,  0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f},
        // gated-loop-release
        {-0.501411378f, 0.491449922f,  0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,
         0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f,
         -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f,
         -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,
         0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,
         0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,
         -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f,
         -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f,
         -0.027481908f, 0.145538136f,  0.309602082f,  0.454613626f,  0.571649075f,  0.653506219f,
         0.695147991f,  0.694011331f,  0.650166690f,  0.566311955f,  0.447607338f,  0.301357806f,
         0.136563271f,  -0.036635134f, -0.207579061f, -0.365748972f, -0.501411378f, -0.606217802f,
         -0.673718631f, -0.699760139f, -0.682739615f, -0.623704612f, -0.526287854f, -0.396484405f,
         -0.242281988f, -0.073169962f, 0.100444809f,  0.267878383f,  0.418827176f,  0.544002175f,
         0.635700226f,  0.688278437f,  0.698501229f,  0.665739536f,  0.592009544f,  0.481848240f,
         0.342034936f,  0.181173369f,  0.009162753f,  -0.163411722f, -0.325930148f, -0.468391359f,
         -0.582028687f, -0.659849048f, -0.697063446f, -0.691381872f, -0.643153787f, -0.555347383f,
         -0.433365792f, -0.284715652f, -0.118544683f, 0.054921336f,  0.225007594f,  0.381247282f,
         0.514025748f,  0.615171969f,  0.678461671f,  0.699999988f,  0.678461671f,  0.615172029f,
         0.514025807f,  0.381247342f,  0.225007668f,  0.054921407f,  -0.118544623f, -0.284715623f,
         -0.433365732f, -0.555347383f, -0.643153787f, -0.691381872f, -0.697063506f, -0.659849048f,
         -0.582028747f, -0.468391418f, -0.325930238f, -0.163411796f, 0.009162681f,  0.181173295f,
         0.342034847f,  0.481848180f,  0.592009485f,  0.665739536f,  0.698501229f,  0.688278437f,
         0.635700166f,  0.544002235f,  0.418827236f,  0.267878413f,  0.100444868f,  -0.073169887f,
         -0.242281914f, -0.396484345f, -0.526287854f, -0.623704553f, -0.682739615f, -0.699760139f,
         -0.673718691f, -0.606217861f, -0.501411438f, -0.365749061f, -0.207579151f, -0.036635205f,
         0.136563197f,  0.301357746f,  0.447607249f,  0.566311955f,  0.650166690f,  0.694011331f,
         0.695147991f,  0.653506219f,  0.571649075f,  0.454613656f,  0.309602112f,  0.145538211f,
         -0.027481835f, -0.198810726f, -0.357905120f, -0.494974703f, -0.601584435f, -0.671173751f,
         -0.699460328f, -0.684703350f, -0.627810955f, -0.532284260f, -0.404001683f, -0.250857592f,
         -0.082276218f, 0.091368288f,  0.259390175f,  0.411449641f,  0.538189232f,  0.631809711f,
         0.686549723f,  0.699040651f,  0.668513954f,  0.596848190f,  0.488453329f,  0.350000024f,
         0.190008357f,  0.018323898f,  -0.154488161f, -0.317793280f, -0.461542070f, -0.576888323f},
    }};

    for (size_t ci = 0; ci < cases.size(); ++ci)
    {
        GenConfig c;
        c.name = cases[ci].name;
        c.loopWhileGated = true;
        c.ungateAtBlock = cases[ci].ungateAtBlock;
        c.recordBlocks = 12;

        GenHarness h(c);
        h.run(c);
        // the release case leaves the loop, so it only has to span seams while gated
        if (cases[ci].ungateAtBlock < 0)
            checkSpannedSeams(c, h);
        goldenCheckOrPrint(c.name, h.recL, expected[ci]);
    }
}

TEST_CASE("Generator golden - forward loop with fade", "[generator][golden]")
{
    // The one fade path believed correct today. Captured at three ratios so the seam
    // lands at different sub-positions and different offsets within the 16 sample
    // block - at unity with a block aligned seam, engagement bugs can hide.
    struct Case
    {
        const char *name;
        int32_t ratio;
        int recordBlocks;
    };
    static const std::array<Case, 3> cases{{
        {"loop-fwd-fade-r10", unityRatio, 10},
        {"loop-fwd-fade-r13", (int32_t)(unityRatio * 1.3), 10},
        {"loop-fwd-fade-r05", (int32_t)(unityRatio * 0.5), 24},
    }};

    static const std::array<std::vector<float>, 3> expected{{
        // loop-fwd-fade-r10
        {-0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,
         0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f,
         -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f,
         -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,
         0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,
         0.686549723f,  0.631809771f,  0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,
         -0.082276143f, -0.250857532f, -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f,
         -0.699460328f, -0.671173811f, -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f,
         0.024379505f,  0.092896707f,  0.142336637f,  0.167617336f,  0.165648341f,  0.135559231f,
         0.078754492f,  -0.001211202f, -0.098928034f, -0.207578257f, -0.319481641f, -0.427103847f,
         -0.520643711f, -0.604681909f, -0.640874565f, -0.704790652f, -0.657547593f, 0.491449922f,
         0.641370535f,  0.668064654f,  0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,
         0.373530179f,  0.216311961f,  0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f,
         -0.560877681f, -0.646715701f, -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f,
         -0.461542130f, -0.317793339f, -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,
         0.488453329f,  0.596848071f,  0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f,
         0.538189352f,  0.411449730f,  0.259390265f,  0.091368362f,  -0.082276143f, -0.250857532f,
         -0.404001623f, -0.532284200f, -0.627810895f, -0.684703350f, -0.699460328f, -0.671173811f,
         -0.601584554f, -0.494974732f, -0.357905179f, -0.198810786f, 0.024379505f,  0.092896707f,
         0.142336637f,  0.167617336f,  0.165648341f,  0.135559231f,  0.078754492f,  -0.001211202f,
         -0.098928034f, -0.207578257f, -0.319481641f, -0.427103847f, -0.520643711f, -0.604681909f,
         -0.640874565f, -0.704790652f, -0.657547593f, 0.491449922f,  0.641370535f,  0.668064654f,
         0.705242038f,  0.674619675f,  0.611010611f,  0.507742941f,  0.373530179f,  0.216311961f,
         0.045782227f,  -0.127564833f, -0.293061823f, -0.440524280f, -0.560877681f, -0.646715701f,
         -0.692755938f, -0.696165383f, -0.656733930f, -0.576888382f, -0.461542130f, -0.317793339f,
         -0.154488236f, 0.018323826f,  0.190008298f,  0.349999964f,  0.488453329f,  0.596848071f,
         0.668513954f,  0.699040651f,  0.686549723f,  0.631809771f},
        // loop-fwd-fade-r13
        {0.436234772f,  0.587565541f,  0.678007126f,  0.698186994f,  0.646013856f,  0.526894391f,
         0.353172988f,  0.142852455f,  -0.082271926f, -0.298870385f, -0.484497070f, -0.619915307f,
         -0.691092074f, -0.690651059f, -0.618637979f, -0.482515574f, -0.112847686f, -0.002099511f,
         0.097023658f,  0.156520516f,  0.182383850f,  0.164592043f,  0.070962340f,  -0.031078832f,
         -0.158810377f, -0.327623427f, -0.458796561f, -0.568416655f, -0.641259968f, -0.793626308f,
         0.039094359f,  0.659351349f,  0.691757858f,  0.689712524f,  0.627503276f,  0.495614618f,
         0.312889069f,  0.097728111f,  -0.127560407f, -0.339629799f, -0.516503513f, -0.639852047f,
         -0.696892858f, -0.681714952f, -0.595890880f, -0.448314905f, -0.254280180f, -0.033894390f,
         0.190003857f,  0.394212008f,  0.557568073f,  0.663143575f,  0.699997604f,  0.664311051f,
         0.559782028f,  0.397242963f,  0.193537816f,  -0.030223720f, -0.250853181f, -0.445486695f,
         -0.593954563f, -0.680871189f, -0.697229207f, -0.641333520f, -0.255871326f, -0.134947717f,
         -0.025744790f, 0.075553209f,  0.153173178f,  0.175751507f,  0.166688398f,  0.113725021f,
         -0.012677008f, -0.135444745f, -0.273092031f, -0.435048312f, -0.546031177f, -0.647931814f,
         -0.714280009f, -0.351909518f, 0.702969074f,  0.654219747f,  0.698658824f,  0.649059057f,
         0.531054735f,  0.358696550f,  0.149126112f,  -0.075898252f, -0.293057352f, -0.479846865f,
         -0.616909981f, -0.690042973f, -0.691666901f, -0.621613622f, -0.487142563f, -0.302189082f,
         -0.085919835f, 0.139253303f,  0.349995613f,  0.524467945f,  0.644589782f,  0.697912931f,
         0.678911686f,  0.589554846f,  0.439102590f,  0.243146196f,  0.021992665f,  -0.201439992f,
         -0.403997451f, -0.564688742f, -0.666861415f, -0.699927509f, -0.341882497f, -0.272023976f,
         -0.171736464f, -0.048932917f, 0.053724039f,  0.136062101f,  0.184873581f,  0.167018488f,
         0.122768871f,  0.037913859f,  -0.113155872f, -0.247934178f, -0.385910660f, -0.526867092f,
         -0.637100399f, -0.645465016f, -0.677521706f, 0.668715596f,  0.616297483f,  0.708281755f,
         0.666036367f,  0.563559175f,  0.402508467f,  0.199694291f,  -0.023813762f, -0.244853988f,
         -0.440520138f, -0.590535164f, -0.679353118f, -0.697770000f, -0.643877208f, -0.523259461f,
         -0.348416418f, -0.137467161f, 0.087727830f,  0.303831607f,  0.488449425f,  0.622449279f,
         0.691944838f,  0.689734340f,  0.616046906f,  0.478518605f,  0.291401565f,  0.074086644f,
         -0.150905848f, -0.360260040f, -0.532280624f, -0.649140954f},
        // loop-fwd-fade-r05
        {-0.657547593f, -0.093648151f, 0.491449922f,  0.704202414f,  0.641370535f,  0.608718872f,
         0.668064654f,  0.710621476f,  0.705242038f,  0.689711809f,  0.674619675f,  0.649057209f,
         0.611010611f,  0.563555956f,  0.507742941f,  0.444076419f,  0.373530179f,  0.297216147f,
         0.216311961f,  0.132066876f,  0.045782227f,  -0.041209560f, -0.127564833f, -0.211950049f,
         -0.293061823f, -0.369647563f, -0.440524280f, -0.504597545f, -0.560877681f, -0.608495593f,
         -0.646715701f, -0.674947917f, -0.692755938f, -0.699865103f, -0.696165383f, -0.681713939f,
         -0.656733930f, -0.621611297f, -0.576888382f, -0.523255825f, -0.461542130f, -0.392700166f,
         -0.317793339f, -0.237978473f, -0.154488236f, -0.068611994f, 0.018323826f,  0.104976736f,
         0.190008298f,  0.272105396f,  0.349999964f,  0.422489166f,  0.488453329f,  0.546873748f,
         0.596848071f,  0.637604713f,  0.668513954f,  0.689098597f,  0.699040651f,  0.698186755f,
         0.686549723f,  0.664309621f,  0.631809771f,  0.589552104f,  0.538189352f,  0.478514612f,
         0.411449730f,  0.338030279f,  0.259390265f,  0.176744133f,  0.091368362f,  0.004581451f,
         -0.082276143f, -0.167863131f, -0.250857532f, -0.329977721f, -0.404001623f, -0.471786141f,
         -0.532284200f, -0.584561586f, -0.332513273f, -0.370193809f, -0.367025703f, -0.388376802f,
         -0.370489120f, -0.375752032f, -0.344746172f, -0.335279256f, -0.293883473f, -0.272022545f,
         -0.223864749f, -0.192717195f, -0.142034486f, -0.105223201f, -0.056525953f, -0.017902393f,
         0.024379505f,  0.061031155f,  0.092896707f,  0.124142811f,  0.142336637f,  0.165343225f,
         0.167617336f,  0.180347279f,  0.165648341f,  0.166991621f,  0.135559231f,  0.125387281f,
         0.078754492f,  0.057894163f,  -0.001211202f, -0.031080410f, -0.098928034f, -0.135446861f,
         -0.207578257f, -0.247936696f, -0.319481641f, -0.360648423f, -0.427103847f, -0.466584921f,
         -0.520643711f, -0.552988589f, -0.604681909f, -0.642611206f, -0.640874565f, -0.626298189f,
         -0.704790652f, -0.830736935f, -0.657547593f, -0.093648151f, 0.491449922f,  0.704202414f,
         0.641370535f,  0.608718872f,  0.668064654f,  0.710621476f,  0.705242038f,  0.689711809f,
         0.674619675f,  0.649057209f,  0.611010611f,  0.563555956f,  0.507742941f,  0.444076419f,
         0.373530179f,  0.297216147f,  0.216311961f,  0.132066876f,  0.045782227f,  -0.041209560f,
         -0.127564833f, -0.211950049f, -0.293061823f, -0.369647563f, -0.440524280f, -0.504597545f,
         -0.560877681f, -0.608495593f, -0.646715701f, -0.674947917f, -0.692755938f, -0.699865103f,
         -0.696165383f, -0.681713939f, -0.656733930f, -0.621611297f, -0.576888382f, -0.523255825f,
         -0.461542130f, -0.392700166f, -0.317793339f, -0.237978473f, -0.154488236f, -0.068611994f,
         0.018323826f,  0.104976736f,  0.190008298f,  0.272105396f,  0.349999964f,  0.422489166f,
         0.488453329f,  0.546873748f,  0.596848071f,  0.637604713f,  0.668513954f,  0.689098597f,
         0.699040651f,  0.698186755f,  0.686549723f,  0.664309621f,  0.631809771f,  0.589552104f,
         0.538189352f,  0.478514612f,  0.411449730f,  0.338030279f,  0.259390265f,  0.176744133f,
         0.091368362f,  0.004581451f,  -0.082276143f, -0.167863131f, -0.250857532f, -0.329977721f,
         -0.404001623f, -0.471786141f, -0.532284200f, -0.584561586f, -0.332513273f, -0.370193809f,
         -0.367025703f, -0.388376802f, -0.370489120f, -0.375752032f, -0.344746172f, -0.335279256f,
         -0.293883473f, -0.272022545f, -0.223864749f, -0.192717195f, -0.142034486f, -0.105223201f,
         -0.056525953f, -0.017902393f, 0.024379505f,  0.061031155f,  0.092896707f,  0.124142811f,
         0.142336637f,  0.165343225f,  0.167617336f,  0.180347279f,  0.165648341f,  0.166991621f,
         0.135559231f,  0.125387281f,  0.078754492f,  0.057894163f,  -0.001211202f, -0.031080410f,
         -0.098928034f, -0.135446861f, -0.207578257f, -0.247936696f, -0.319481641f, -0.360648423f,
         -0.427103847f, -0.466584921f, -0.520643711f, -0.552988589f, -0.604681909f, -0.642611206f,
         -0.640874565f, -0.626298189f, -0.704790652f, -0.830736935f, -0.657547593f, -0.093648151f,
         0.491449922f,  0.704202414f,  0.641370535f,  0.608718872f,  0.668064654f,  0.710621476f,
         0.705242038f,  0.689711809f,  0.674619675f,  0.649057209f,  0.611010611f,  0.563555956f,
         0.507742941f,  0.444076419f,  0.373530179f,  0.297216147f,  0.216311961f,  0.132066876f,
         0.045782227f,  -0.041209560f, -0.127564833f, -0.211950049f, -0.293061823f, -0.369647563f,
         -0.440524280f, -0.504597545f, -0.560877681f, -0.608495593f, -0.646715701f, -0.674947917f,
         -0.692755938f, -0.699865103f, -0.696165383f, -0.681713939f, -0.656733930f, -0.621611297f,
         -0.576888382f, -0.523255825f, -0.461542130f, -0.392700166f, -0.317793339f, -0.237978473f,
         -0.154488236f, -0.068611994f, 0.018323826f,  0.104976736f,  0.190008298f,  0.272105396f,
         0.349999964f,  0.422489166f,  0.488453329f,  0.546873748f,  0.596848071f,  0.637604713f,
         0.668513954f,  0.689098597f,  0.699040651f,  0.698186755f,  0.686549723f,  0.664309621f,
         0.631809771f,  0.589552104f,  0.538189352f,  0.478514612f,  0.411449730f,  0.338030279f,
         0.259390265f,  0.176744133f,  0.091368362f,  0.004581451f,  -0.082276143f, -0.167863131f,
         -0.250857532f, -0.329977721f, -0.404001623f, -0.471786141f, -0.532284200f, -0.584561586f,
         -0.332513273f, -0.370193809f, -0.367025703f, -0.388376802f, -0.370489120f, -0.375752032f,
         -0.344746172f, -0.335279256f, -0.293883473f, -0.272022545f, -0.223864749f, -0.192717195f,
         -0.142034486f, -0.105223201f, -0.056525953f, -0.017902393f, 0.024379505f,  0.061031155f,
         0.092896707f,  0.124142811f,  0.142336637f,  0.165343225f,  0.167617336f,  0.180347279f,
         0.165648341f,  0.166991621f,  0.135559231f,  0.125387281f,  0.078754492f,  0.057894163f,
         -0.001211202f, -0.031080410f, -0.098928034f, -0.135446861f, -0.207578257f, -0.247936696f,
         -0.319481641f, -0.360648423f, -0.427103847f, -0.466584921f, -0.520643711f, -0.552988589f,
         -0.604681909f, -0.642611206f, -0.640874565f, -0.626298189f, -0.704790652f, -0.830736935f},
    }};

    for (size_t ci = 0; ci < cases.size(); ++ci)
    {
        GenConfig c;
        c.name = cases[ci].name;
        c.loopFade = gFade;
        c.ratio = cases[ci].ratio;
        c.recordBlocks = cases[ci].recordBlocks;

        GenHarness h(c);
        goldenCheckOrPrint(c.name, capture(c, h), expected[ci]);
    }
}

TEST_CASE("Generator golden - interpolation types", "[generator][golden]")
{
    struct Case
    {
        const char *name;
        scxt::dsp::InterpolationTypes interp;
    };
    static const std::array<Case, 3> cases{{
        {"loop-fwd-fade-linear", scxt::dsp::InterpolationTypes::Linear},
        {"loop-fwd-fade-zoh", scxt::dsp::InterpolationTypes::ZeroOrderHold},
        {"loop-fwd-fade-zohaa", scxt::dsp::InterpolationTypes::ZOHAA},
    }};

    static const std::array<std::vector<float>, 3> expected{{
        // loop-fwd-fade-linear
        {0.433068603f,  0.586005986f,  0.674618542f,  0.692795515f,  0.642759085f,  0.525518477f,
         0.350629658f,  0.141779050f,  -0.082271874f, -0.296796978f, -0.480967969f, -0.618255794f,
         -0.687654376f, -0.685317755f, -0.615504146f, -0.481271207f, -0.111420862f, -0.001689501f,
         0.097023606f,  0.155096933f,  0.180273145f,  0.163706452f,  0.069497518f,  -0.033030376f,
         -0.159746557f, -0.327821940f, -0.458071649f, -0.569842458f, -0.653774858f, -0.689692318f,
         0.037469752f,  0.609559774f,  0.684514642f,  0.688044667f,  0.623829067f,  0.494342387f,
         0.310646951f,  0.096945569f,  -0.127560303f, -0.337296665f, -0.512733161f, -0.638129652f,
         -0.693437755f, -0.676450670f, -0.592859566f, -0.447171003f, -0.252475619f, -0.033524364f,
         0.190003738f,  0.391532302f,  0.553487301f,  0.661345482f,  0.696542799f,  0.659181178f,
         0.556915879f,  0.396247834f,  0.192186013f,  -0.030178117f, -0.250852972f, -0.442482919f,
         -0.589597583f, -0.679012597f, -0.693803787f, -0.636381090f, -0.253912747f, -0.134382725f,
         -0.025191860f, 0.075229660f,  0.153173059f,  0.173944935f,  0.164353773f,  0.112837456f,
         -0.013937757f, -0.136938706f, -0.273689508f, -0.434643209f, -0.549421072f, -0.637435138f,
         -0.692551255f, -0.324558407f, 0.579772532f,  0.674540460f,  0.695182323f,  0.643449485f,
         0.528362036f,  0.357812792f,  0.148104876f,  -0.075555794f, -0.293057084f, -0.476626873f,
         -0.612378061f, -0.688150644f, -0.688280165f, -0.616813421f, -0.484614640f, -0.301467568f,
         -0.085368387f, 0.138497993f,  0.349995315f,  0.520968616f,  0.639845550f,  0.695987105f,
         0.675603330f,  0.585002244f,  0.436801314f,  0.242592961f,  0.021915644f,  -0.200278163f,
         -0.403997093f, -0.560939372f, -0.661944687f, -0.697984219f, -0.339462996f, -0.268902421f,
         -0.170288429f, -0.048694212f, 0.053435709f,  0.135089517f,  0.184873343f,  0.165001079f,
         0.120411679f,  0.037094336f,  -0.114130892f, -0.248896748f, -0.386083275f, -0.528999746f,
         -0.621095419f, -0.680869877f, -0.706039906f, 0.549985349f,  0.656190038f,  0.698011279f,
         0.663069904f,  0.559257805f,  0.400380671f,  0.199264199f,  -0.023551276f, -0.243407622f,
         -0.440519720f, -0.586626410f, -0.674338460f, -0.695824325f, -0.640767276f, -0.519218802f,
         -0.346547574f, -0.137212411f, 0.086992256f,  0.301997453f,  0.488448977f,  0.618345618f,
         0.686829031f,  0.687799215f,  0.613088608f,  0.474823505f,  0.289806932f,  0.074009404f,
         -0.149703354f, -0.358053505f, -0.532280147f, -0.644876838f},
        // loop-fwd-fade-zoh
        {0.349999994f,  0.488453329f,  0.668513954f,  0.699040651f,  0.686549723f,  0.538189292f,
         0.411449671f,  0.259390205f,  0.091368333f,  -0.250857562f, -0.404001623f, -0.532284200f,
         -0.684703350f, -0.699460328f, -0.671173811f, -0.494974732f, -0.142034486f, -0.056525931f,
         0.024379510f,  0.142336607f,  0.167617306f,  0.165648326f,  0.078754432f,  -0.001211205f,
         -0.098928064f, -0.319501013f, -0.426835924f, -0.522194743f, -0.599310875f, -0.682686806f,
         -0.686549723f, 0.520201385f,  0.680658937f,  0.699940026f,  0.676148057f,  0.507762074f,
         0.373530149f,  0.216311902f,  0.045782190f,  -0.293061823f, -0.440524280f, -0.560877681f,
         -0.692755997f, -0.696165323f, -0.656733930f, -0.461542070f, -0.317793339f, -0.154488206f,
         0.018323863f,  0.349999994f,  0.488453329f,  0.596848130f,  0.699040651f,  0.686549723f,
         0.631809711f,  0.411449671f,  0.259390205f,  0.091368333f,  -0.082276180f, -0.404001623f,
         -0.532284200f, -0.627810895f, -0.699460328f, -0.671173811f, -0.293883443f, -0.142034486f,
         -0.056525931f, 0.024379510f,  0.092896715f,  0.167617306f,  0.165648326f,  0.135559261f,
         -0.001211205f, -0.098928064f, -0.207578227f, -0.426835924f, -0.522194743f, -0.599310875f,
         -0.653614700f, -0.686549723f, 0.520201385f,  0.619491339f,  0.699940026f,  0.676148057f,
         0.610747218f,  0.373530149f,  0.216311902f,  0.045782190f,  -0.127564862f, -0.440524280f,
         -0.560877681f, -0.646715701f, -0.696165323f, -0.656733930f, -0.576888323f, -0.317793339f,
         -0.154488206f, 0.018323863f,  0.190008312f,  0.488453329f,  0.596848130f,  0.668513954f,
         0.686549723f,  0.631809711f,  0.538189292f,  0.259390205f,  0.091368333f,  -0.082276180f,
         -0.250857562f, -0.532284200f, -0.627810895f, -0.684703350f, -0.344746172f, -0.293883443f,
         -0.223864764f, -0.056525931f, 0.024379510f,  0.092896715f,  0.142336607f,  0.165648326f,
         0.135559261f,  0.078754432f,  -0.098928064f, -0.207578227f, -0.319501013f, -0.522194743f,
         -0.599310875f, -0.653614700f, -0.682686806f, 0.520201385f,  0.619491339f,  0.680658937f,
         0.676148057f,  0.610747218f,  0.507762074f,  0.216311902f,  0.045782190f,  -0.127564862f,
         -0.293061823f, -0.560877681f, -0.646715701f, -0.692755997f, -0.656733930f, -0.576888323f,
         -0.461542070f, -0.154488206f, 0.018323863f,  0.190008312f,  0.349999994f,  0.596848130f,
         0.668513954f,  0.699040651f,  0.631809711f,  0.538189292f,  0.411449671f,  0.091368333f,
         -0.082276180f, -0.250857562f, -0.404001623f, -0.627810895f},
        // loop-fwd-fade-zohaa
        {0.434826374f,  0.587319970f,  0.677469611f,  0.698270798f,  0.646334648f,  0.527846694f,
         0.355102986f,  0.144162625f,  -0.082271807f, -0.296781361f, -0.483190149f, -0.619704008f,
         -0.690738678f, -0.690924346f, -0.619034350f, -0.483564705f, -0.076519705f, 0.037601341f,
         0.139724374f,  0.201930031f,  0.229157820f,  0.211650103f,  0.119026318f,  0.015700679f,
         -0.114998274f, -0.286274403f, -0.420460343f, -0.538220406f, -0.628614008f, -0.670481920f,
         -0.680237293f, 0.616116405f,  0.687534869f,  0.693618536f,  0.627375305f,  0.496647775f,
         0.314893216f,  0.099056490f,  -0.127560273f, -0.337606460f, -0.515275836f, -0.639666259f,
         -0.696672916f, -0.682122231f, -0.596339285f, -0.449427843f, -0.256373644f, -0.035238650f,
         0.190003738f,  0.392295092f,  0.556460142f,  0.662994802f,  0.699965656f,  0.664902806f,
         0.560299754f,  0.398437202f,  0.195703074f,  -0.028874850f, -0.250853062f, -0.443692327f,
         -0.592975676f, -0.680760980f, -0.697385728f, -0.642104805f, -0.224472687f, -0.098477826f,
         0.013281032f,  0.117599115f,  0.197616845f,  0.222560123f,  0.214204729f,  0.160652190f,
         0.034646329f,  -0.090138562f, -0.231586412f, -0.396419525f, -0.516036689f, -0.610445261f,
         -0.672221601f, -0.675597906f, 0.636461079f,  0.676262081f,  0.698721826f,  0.649177492f,
         0.531660140f,  0.359942675f,  0.151331589f,  -0.074553020f, -0.293057144f, -0.478149265f,
         -0.616028368f, -0.689960778f, -0.691957414f, -0.622509241f, -0.487768859f, -0.303498894f,
         -0.088165656f, 0.137922809f,  0.349995464f,  0.522917986f,  0.643850505f,  0.697870493f,
         0.679387391f,  0.590618014f,  0.439785242f,  0.244508728f,  0.024260009f,  -0.200135440f,
         -0.403997302f, -0.563299358f, -0.666270792f, -0.699925184f, -0.314756662f, -0.241427392f,
         -0.137243137f, -0.009701056f, 0.095202409f,  0.180100620f,  0.230603486f,  0.214755297f,
         0.170497328f,  0.084156223f,  -0.067183822f, -0.204719365f, -0.347199470f, -0.494619220f,
         -0.592335641f, -0.658821523f, -0.690907896f, 0.630723178f,  0.660428047f,  0.699937522f,
         0.666910887f,  0.564787567f,  0.403227597f,  0.201087490f,  -0.021542680f, -0.243574679f,
         -0.440520078f, -0.589267790f, -0.678871810f, -0.697796345f, -0.644660294f, -0.524588346f,
         -0.349182308f, -0.138893262f, 0.085467868f,  0.302596807f,  0.488449335f,  0.621361613f,
         0.691619754f,  0.689800560f,  0.617000937f,  0.479987860f,  0.292207211f,  0.075533696f,
         -0.148675978f, -0.359080106f, -0.532280505f, -0.648242176f},
    }};

    for (size_t ci = 0; ci < cases.size(); ++ci)
    {
        GenConfig c;
        c.name = cases[ci].name;
        c.loopFade = gFade;
        c.ratio = (int32_t)(unityRatio * 1.3); // non integer so the kernels actually interpolate
        c.interp = cases[ci].interp;

        GenHarness h(c);
        goldenCheckOrPrint(c.name, capture(c, h), expected[ci]);
    }
}

TEST_CASE("Generator golden - int16 sample data", "[generator][golden]")
{
    // loop-fwd-fade-i16
    static const std::vector<float> expected{
        0.435652643f,  0.587378204f,  0.677839398f,  0.698073566f,  0.646129489f,  0.527297616f,
        0.353061944f,  0.142848566f,  -0.081589922f, -0.298173547f, -0.483955830f, -0.619699836f,
        -0.690939069f, -0.690603852f, -0.618803918f, -0.482966334f, -0.112807594f, -0.002104514f,
        0.096730731f,  0.156299606f,  0.182288736f,  0.164540663f,  0.070972323f,  -0.030792756f,
        -0.158401906f, -0.327297240f, -0.458679616f, -0.568296015f, -0.641328454f, -0.792478919f,
        0.033942498f,  0.659195662f,  0.691595256f,  0.689705193f,  0.627663791f,  0.496061981f,
        0.312783211f,  0.097737372f,  -0.126874819f, -0.338947028f, -0.515992045f, -0.639645994f,
        -0.696740866f, -0.681714594f, -0.596113801f, -0.448785186f, -0.254190385f, -0.033929735f,
        0.189311415f,  0.393520445f,  0.557101369f,  0.662925959f,  0.699819446f,  0.664394438f,
        0.560074925f,  0.397764415f,  0.193457767f,  -0.030179538f, -0.250169754f, -0.444830388f,
        -0.593513131f, -0.680661380f, -0.697057605f, -0.641465425f, -0.256055146f, -0.135202646f,
        -0.025718275f, 0.075525068f,  0.152922735f,  0.175605193f,  0.166686401f,  0.113697179f,
        -0.012628522f, -0.135106817f, -0.272613436f, -0.434712321f, -0.545904219f, -0.647855103f,
        -0.712977290f, -0.356648535f, 0.703160405f,  0.654058158f,  0.698542476f,  0.649154365f,
        0.531430185f,  0.359234214f,  0.149065927f,  -0.075849541f, -0.292382330f, -0.479214579f,
        -0.616514564f, -0.689837158f, -0.691495776f, -0.621800721f, -0.487526119f, -0.302769363f,
        -0.085874669f, 0.139181092f,  0.349313080f,  0.523860753f,  0.644250214f,  0.697676420f,
        0.678754151f,  0.589813769f,  0.439554214f,  0.243749291f,  0.021961626f,  -0.201361418f,
        -0.403348625f, -0.564134717f, -0.666579247f, -0.699701071f, -0.341815472f, -0.272169948f,
        -0.171997339f, -0.049212933f, 0.053727459f,  0.136018410f,  0.184687451f,  0.166979790f,
        0.122859195f,  0.037937257f,  -0.113105536f, -0.247528866f, -0.385389924f, -0.526582599f,
        -0.636931062f, -0.645466626f, -0.680385411f, 0.667366624f,  0.615920961f,  0.708076835f,
        0.665930033f,  0.563901126f,  0.402985662f,  0.200330243f,  -0.023837084f, -0.244772553f,
        -0.439895600f, -0.590018094f, -0.679122686f, -0.697542965f, -0.643725574f, -0.523603916f,
        -0.348950177f, -0.138119966f, 0.087719470f,  0.303727180f,  0.487837285f,  0.621988595f,
        0.691745996f,  0.689508617f,  0.615918934f,  0.478925586f,  0.291976154f,  0.074760035f,
        -0.150891602f, -0.360159159f, -0.531695127f, -0.648722589f};

    GenConfig c;
    c.name = "loop-fwd-fade-i16";
    c.isFloat = false;
    c.loopFade = gFade;
    c.ratio = (int32_t)(unityRatio * 1.3);

    GenHarness h(c);
    goldenCheckOrPrint(c.name, capture(c, h), expected);
}
