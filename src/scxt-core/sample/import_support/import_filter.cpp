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

#include "import_filter.h"
#include "import_harness.h"
#include "engine/engine.h"
#include "messaging/messaging.h"

namespace scxt::import_support
{
FilterHandle importZoneFilter(engine::Zone &zone, ImporterContext &ctx, int procSlot,
                              const FilterArgs &a)
{
    if (!a.type.has_value())
    {
        ctx.software_error("importZoneFilter", "type is required");
        return {};
    }

    auto type = *a.type;
    {
        auto guard = ctx.getEngine().getMessageController()->threadingChecker.bypassChecksInScope();
        zone.setProcessorType(procSlot, type);
    }

    auto &ps = zone.processorStorage[procSlot];
    switch (type)
    {
    case dsp::processor::ProcessorType::proct_CytomicSVF:
        if (a.cutoff)
            ps.floatParams[0] = *a.cutoff;
        if (a.resonance)
            ps.floatParams[2] = *a.resonance;
        break;
    default:
        // Future processor types add their own index mapping here.
        break;
    }

    return {procSlot, type};
}
} // namespace scxt::import_support
