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

#ifndef SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_MAPPING_H
#define SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_MAPPING_H

#include <optional>
#include "engine/zone.h"

namespace scxt::import_support
{
class ImporterContext;

// rootKey is required; the helper logs + asserts + returns if it is unset.
// Other fields default to neutral values when unset:
//   keyStart=rootKey, keyEnd=rootKey+1, velStart=0, velEnd=127,
//   all fades=0, tracking=1.0, pitchOffsetSemitones=0.0.
struct MappingArgs
{
    std::optional<int> rootKey;

    std::optional<int> keyStart, keyEnd;
    std::optional<int> keyFadeStart, keyFadeEnd;

    std::optional<int> velStart, velEnd;
    std::optional<int> velFadeStart, velFadeEnd;

    std::optional<float> tracking;
    std::optional<float> pitchOffsetSemitones;
};

void importZoneMapping(engine::Zone &, ImporterContext &, const MappingArgs &);
} // namespace scxt::import_support

#endif
