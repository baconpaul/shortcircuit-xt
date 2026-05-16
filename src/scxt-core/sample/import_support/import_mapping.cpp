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

#include "import_mapping.h"
#include "import_harness.h"

namespace scxt::import_support
{
void importZoneMapping(engine::Zone &zone, ImporterContext &ctx, const MappingArgs &a)
{
    if (!a.rootKey.has_value())
    {
        ctx.software_error("importZoneMapping", "rootKey is required");
        return;
    }

    auto rk = *a.rootKey;
    auto &m = zone.mapping;
    m.rootKey = static_cast<int16_t>(rk);

    m.keyboardRange.keyStart = static_cast<int16_t>(a.keyStart.value_or(rk));
    m.keyboardRange.keyEnd = static_cast<int16_t>(a.keyEnd.value_or(rk + 1));
    m.keyboardRange.fadeStart = static_cast<int16_t>(a.keyFadeStart.value_or(0));
    m.keyboardRange.fadeEnd = static_cast<int16_t>(a.keyFadeEnd.value_or(0));

    m.velocityRange.velStart = static_cast<int16_t>(a.velStart.value_or(0));
    m.velocityRange.velEnd = static_cast<int16_t>(a.velEnd.value_or(127));
    m.velocityRange.fadeStart = static_cast<int16_t>(a.velFadeStart.value_or(0));
    m.velocityRange.fadeEnd = static_cast<int16_t>(a.velFadeEnd.value_or(0));

    m.tracking = a.tracking.value_or(1.0f);
    m.pitchOffset = a.pitchOffsetSemitones.value_or(0.0f);

    m.keyboardRange.normalize();
    m.velocityRange.normalize();
}
} // namespace scxt::import_support
