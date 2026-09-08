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
 * The 'Alternate' voice modulation source - a flip flop which hands each note on 0, then 1,
 * then 0 again. It flips per note on rather than per voice, so zones layered on one key all
 * sound with the same value. The flag lives on the engine, so the alternation is global
 * rather than per zone or per key, and a legato retrigger keeps what its voice was born with.
 * GH #2647.
 */

#include "catch2/catch2.hpp"

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

TEST_CASE("Alternate flips on every note on", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &zone = oneZoneOnKey(*eng, 60);

    std::vector<float> alts;
    for (int i = 0; i < 8; ++i)
    {
        eng->processNoteOnEvent(0, 0, 60, -1, 1.f, 0.f);
        const auto *v = newestVoice(zone);
        REQUIRE(v);
        alts.push_back(v->currentAlternate);
        eng->processNoteOffEvent(0, 0, 60, -1, 0.f);
    }

    for (int i = 0; i < (int)alts.size(); ++i)
        REQUIRE(alts[i] == ((i % 2) ? 1.f : 0.f));
}

TEST_CASE("Alternate does not restart per key", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &part = *eng->getPatch()->getPart(0);
    part.addGroup();
    addBlankZoneToGroup(part, 0, 48, 72);
    auto &zone = *part.getGroup(0)->getZone(0);

    std::vector<float> alts;
    for (auto key : {60, 62, 64, 60, 67})
    {
        eng->processNoteOnEvent(0, 0, key, -1, 1.f, 0.f);
        const auto *v = newestVoice(zone);
        REQUIRE(v);
        alts.push_back(v->currentAlternate);
        eng->processNoteOffEvent(0, 0, key, -1, 0.f);
    }

    REQUIRE(alts == std::vector<float>{0.f, 1.f, 0.f, 1.f, 0.f});
}

TEST_CASE("Alternate is shared by zones layered on one key", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &part = *eng->getPatch()->getPart(0);
    part.addGroup();
    addBlankZoneToGroup(part, 0, 60, 60);
    addBlankZoneToGroup(part, 0, 60, 60);
    auto &lower = *part.getGroup(0)->getZone(0);
    auto &upper = *part.getGroup(0)->getZone(1);

    // both zones on a press get the same value, and it advances press to press: 00 11 00 11
    for (int i = 0; i < 4; ++i)
    {
        eng->processNoteOnEvent(0, 0, 60, -1, 1.f, 0.f);
        const auto *lv = newestVoice(lower);
        const auto *uv = newestVoice(upper);
        REQUIRE(lv);
        REQUIRE(uv);
        REQUIRE(lv->currentAlternate == ((i % 2) ? 1.f : 0.f));
        REQUIRE(uv->currentAlternate == lv->currentAlternate);
        eng->processNoteOffEvent(0, 0, 60, -1, 0.f);
    }
}

TEST_CASE("Alternate reaches the voice matrix", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &zone = oneZoneOnKey(*eng, 60);

    // an empty zone's voice ends on its first block - no generator, no procs - so give it
    // an oscillator to sustain through the block that runs the matrix
    {
        auto bypass = eng->getMessageController()->threadingChecker.bypassChecksInScope();
        zone.setProcessorType(0, scxt::dsp::processor::proct_osc_sineplus);
    }

    const auto panT = vm::MatrixConfig::TargetIdentifier{'zout', 'pan ', 0};

    auto &row = zone.routingTable.routes[0];
    row.active = true;
    row.source = vm::sourcesForScanning().voiceSources.alternate;
    row.target = panT;
    row.depth = 1.f;

    auto panAfterPress = [&]() {
        eng->processNoteOnEvent(0, 0, 60, -1, 1.f, 0.f);
        eng->processAudio();
        const auto *v = newestVoice(zone);
        REQUIRE(v);
        auto p = v->modMatrix->getTargetValue(panT);
        eng->processNoteOffEvent(0, 0, 60, -1, 0.f);
        return p;
    };

    auto pOff = panAfterPress();
    auto pOn = panAfterPress();

    REQUIRE(pOff == Approx(0.f));
    REQUIRE(pOn > 0.1f);
}

TEST_CASE("Alternate is offered as a voice source", "[modulation]")
{
    std::unique_ptr<scxt::engine::Engine> eng(makeEngine());
    auto &zone = oneZoneOnKey(*eng, 60);

    auto md = vm::getVoiceMatrixMetadata(zone);
    const auto &sources = std::get<1>(md);

    auto found = std::find_if(sources.begin(), sources.end(), [](const auto &s) {
        return s.second.first == "Voice" && s.second.second == "Alternate";
    });
    REQUIRE(found != sources.end());
    REQUIRE(found->first == vm::sourcesForScanning().voiceSources.alternate);
}
