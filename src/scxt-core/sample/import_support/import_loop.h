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

#ifndef SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_LOOP_H
#define SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_LOOP_H

#include <optional>
#include "engine/zone.h"

namespace scxt::import_support
{
class ImporterContext;

// Partial-write: any unset optional is left at the variant's current value.
// (Contrast with MappingArgs which requires rootKey and defaults the rest —
// loop fields are more often partially updated, e.g. SFZ's loop_mode without
// explicit bounds, where the helper should not clobber meta-loaded values.)
struct LoopArgs
{
    std::optional<engine::Zone::LoopMode> mode;
    std::optional<engine::Zone::LoopDirection> direction;
    std::optional<int64_t> startSamples;
    std::optional<int64_t> endSamples;
    std::optional<int64_t> fadeSamples;
    std::optional<bool> active;
};

void importZoneLoop(engine::Zone &, ImporterContext &, int variantIdx, const LoopArgs &);
} // namespace scxt::import_support

#endif
