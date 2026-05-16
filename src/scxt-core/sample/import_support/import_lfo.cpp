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

#include "import_lfo.h"
#include "import_harness.h"

namespace scxt::import_support
{
LFOHandle importZoneLFO(engine::Zone &zone, ImporterContext &ctx, int lfoSlot, const LFOArgs &a)
{
    if (!a.shape.has_value())
    {
        ctx.software_error("importZoneLFO", "shape is required");
        return {};
    }

    auto &ms = zone.modulatorStorage[lfoSlot];
    ms.modulatorShape = *a.shape;
    if (a.rate)
        ms.rate = *a.rate;
    return {lfoSlot};
}
} // namespace scxt::import_support
