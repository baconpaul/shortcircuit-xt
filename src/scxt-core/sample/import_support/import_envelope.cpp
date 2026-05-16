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

#include "import_envelope.h"
#include "import_harness.h"
#include "modulation/modulator_storage.h"

namespace scxt::import_support
{
EGHandle importZoneEnvelope(engine::Zone &zone, ImporterContext &ctx, int egIdx,
                            const EnvelopeArgs &a)
{
    (void)ctx; // Reserved for future required-field checks; envelope is partial-write today.
    auto &eg = zone.egStorage[egIdx];
    const auto s2a = [](float s) { return (float)modulation::secondsToNormalizedEnvTime(s); };

    if (a.delaySeconds)
        eg.dly = s2a(*a.delaySeconds);
    if (a.attackSeconds)
        eg.a = s2a(*a.attackSeconds);
    if (a.holdSeconds)
        eg.h = s2a(*a.holdSeconds);
    if (a.decaySeconds)
        eg.d = s2a(*a.decaySeconds);
    if (a.sustainLevel)
        eg.s = *a.sustainLevel;
    if (a.releaseSeconds)
        eg.r = s2a(*a.releaseSeconds);

    if (a.attackShape)
        eg.aShape = *a.attackShape;
    if (a.decayShape)
        eg.dShape = *a.decayShape;
    if (a.releaseShape)
        eg.rShape = *a.releaseShape;

    return {egIdx};
}
} // namespace scxt::import_support
