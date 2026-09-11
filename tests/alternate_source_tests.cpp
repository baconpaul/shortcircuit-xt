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

/*
 * The three Alternate voice modulation sources - a 0/1 flip flop, its bipolar twin, and a
 * -1/0/1 rotation. All three come off one engine counter which steps on each note on that
 * starts voices, so they are global rather than per zone or per key, they step per note on
 * rather than per voice (zones layered on one key sound with the same value), and a legato
 * retrigger keeps what its voice was born with. GH #2647.
 */

#include "catch2/catch2.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include "configuration.h"
#include "dsp/processor/processor.h"
#include "engine/engine.h"
#include "engine/group.h"
#include "engine/part.h"
#include "engine/zone.h"
#include "messaging/messaging.h"
#include "modulation/voice_matrix.h"
#include "voice/voice.h"

#include "test_utils.h"

namespace
{
namespace vm = scxt::voice::modulation;

// what each source should read on the nth note on of a fresh engine
float expectedAlternate(int n) { return (n % 2) * 1.f; }
float expectedBipolar(int n) { return (n % 2) * 2.f - 1.f; }
float expectedRotation(int n) { return (n % 3) * 1.f - 1.f; }

// The voice the most recent note on made in this zone. Released voices linger, so "newest"
// has to come off the creation id rather than the first assigned slot.
const scxt::voice::Voice *newestVoice(const scxt::engine::Zone &z)
{
    const scxt::voice::Voice *best{nullptr};
    for (int i = 0; i < (int)scxt::maxVoices; ++i)
    {
        const auto *v = z.voiceWeakPointers[i];
        if (v && v->isVoiceAssigned && (!best || v->voiceCreationId > best->voiceCreationId))
            best = v;
    }
    return best;
}

scxt::engine::Zone &oneZoneOnKey(scxt::engine::Engine &eng, int key)
{
    auto &part = *eng.getPatch()->getPart(0);
    part.addGroup();
    addBlankZoneToGroup(part, 0, key, key);
    return *part.getGroup(0)->getZone(0);
}
} // namespace

TEST_CASE("Alternates step on every note on", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &zone = oneZoneOnKey(*eng, 60);

    // twelve presses so the 6 state counter wraps twice and the 2 and 3 cycles have to come
    // back into phase across the wrap
    for (int i = 0; i < 12; ++i)
    {
        eng->processNoteOnEvent(0, 0, 60, -1, 1.f, 0.f);
        const auto *v = newestVoice(zone);
        REQUIRE(v);
        REQUIRE(v->currentAlternate == expectedAlternate(i));
        REQUIRE(v->currentAlternateBipolar == expectedBipolar(i));
        REQUIRE(v->currentAlternateRotation == expectedRotation(i));
        eng->processNoteOffEvent(0, 0, 60, -1, 0.f);
    }
}

TEST_CASE("Alternates do not restart per key", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &part = *eng->getPatch()->getPart(0);
    part.addGroup();
    addBlankZoneToGroup(part, 0, 48, 72);
    auto &zone = *part.getGroup(0)->getZone(0);

    int n{0};
    for (auto key : {60, 62, 64, 60, 67, 55, 60})
    {
        eng->processNoteOnEvent(0, 0, key, -1, 1.f, 0.f);
        const auto *v = newestVoice(zone);
        REQUIRE(v);
        REQUIRE(v->currentAlternate == expectedAlternate(n));
        REQUIRE(v->currentAlternateRotation == expectedRotation(n));
        eng->processNoteOffEvent(0, 0, key, -1, 0.f);
        n++;
    }
}

TEST_CASE("Alternates are shared by zones layered on one key", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &part = *eng->getPatch()->getPart(0);
    part.addGroup();
    addBlankZoneToGroup(part, 0, 60, 60);
    addBlankZoneToGroup(part, 0, 60, 60);
    auto &lower = *part.getGroup(0)->getZone(0);
    auto &upper = *part.getGroup(0)->getZone(1);

    // both zones on a press get the same value, and it advances press to press: 00 11 00 11
    for (int i = 0; i < 6; ++i)
    {
        eng->processNoteOnEvent(0, 0, 60, -1, 1.f, 0.f);
        const auto *lv = newestVoice(lower);
        const auto *uv = newestVoice(upper);
        REQUIRE(lv);
        REQUIRE(uv);
        REQUIRE(lv->currentAlternate == expectedAlternate(i));
        REQUIRE(lv->currentAlternateRotation == expectedRotation(i));
        REQUIRE(uv->currentAlternate == lv->currentAlternate);
        REQUIRE(uv->currentAlternateRotation == lv->currentAlternateRotation);
        eng->processNoteOffEvent(0, 0, 60, -1, 0.f);
    }
}

TEST_CASE("Alternates reach the voice matrix", "[modulation]")
{
    const auto panT = vm::MatrixConfig::TargetIdentifier{'zout', 'pan ', 0};

    // route one source at full depth to zone pan and report the modulated pan on each of the
    // first two presses. Pan is clamped to its own range so only the sign is asserted below.
    auto firstTwoPans = [&panT](const vm::MatrixConfig::SourceIdentifier &src) {
        std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
        auto &zone = oneZoneOnKey(*eng, 60);

        // an empty zone's voice ends on its first block - no generator, no procs - so give it
        // an oscillator to sustain through the block that runs the matrix
        {
            auto bypass = eng->getMessageController()->threadingChecker.bypassChecksInScope();
            zone.setProcessorType(0, scxt::dsp::processor::proct_osc_sineplus);
        }

        auto &row = zone.routingTable.routes[0];
        row.active = true;
        row.source = src;
        row.target = panT;
        row.depth = 1.f;

        std::vector<float> pans;
        for (int i = 0; i < 2; ++i)
        {
            eng->processNoteOnEvent(0, 0, 60, -1, 1.f, 0.f);
            eng->processAudio();
            const auto *v = newestVoice(zone);
            REQUIRE(v);
            pans.push_back(v->modMatrix->getTargetValue(panT));
            eng->processNoteOffEvent(0, 0, 60, -1, 0.f);
        }
        return pans;
    };

    const auto &srcs = vm::sourcesForScanning().voiceSources;

    SECTION("0/1 alternate goes 0 then positive")
    {
        auto p = firstTwoPans(srcs.alternate);
        REQUIRE(p[0] == Approx(0.f));
        REQUIRE(p[1] > 0.1f);
    }

    SECTION("+/-1 alternate goes negative then positive")
    {
        auto p = firstTwoPans(srcs.alternateBipolar);
        REQUIRE(p[0] < -0.1f);
        REQUIRE(p[1] > 0.1f);
    }

    SECTION("-1/0/1 rotation goes negative then 0")
    {
        auto p = firstTwoPans(srcs.alternateRotation);
        REQUIRE(p[0] < -0.1f);
        REQUIRE(p[1] == Approx(0.f));
    }
}

TEST_CASE("Alternates are offered in their own submenu", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &zone = oneZoneOnKey(*eng, 60);

    auto md = vm::getVoiceMatrixMetadata(zone);
    const auto &sources = std::get<1>(md);

    std::vector<std::string> inAlternates;
    for (const auto &[si, sn] : sources)
        if (sn.first == "Voice/Alternates")
            inAlternates.push_back(sn.second);

    // the menu order is pinned rather than alphabetical, narrowest range first
    REQUIRE(inAlternates ==
            std::vector<std::string>{"0/1 Alternate", "+/-1 Alternate", "-1/0/1 Rotation"});

    auto idFor = [&sources](const std::string &name) {
        auto it = std::find_if(sources.begin(), sources.end(), [&name](const auto &s) {
            return s.second.first == "Voice/Alternates" && s.second.second == name;
        });
        REQUIRE(it != sources.end());
        return it->first;
    };

    const auto &srcs = vm::sourcesForScanning().voiceSources;
    REQUIRE(idFor("0/1 Alternate") == srcs.alternate);
    REQUIRE(idFor("+/-1 Alternate") == srcs.alternateBipolar);
    REQUIRE(idFor("-1/0/1 Rotation") == srcs.alternateRotation);

    // nothing left behind at the old flat path
    auto stale = std::find_if(sources.begin(), sources.end(),
                              [](const auto &s) { return s.second.second == "Alternate"; });
    REQUIRE(stale == sources.end());
}
