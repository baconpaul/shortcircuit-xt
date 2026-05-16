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

#ifndef SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_ENVELOPE_H
#define SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_ENVELOPE_H

#include <optional>
#include "engine/zone.h"

namespace scxt::import_support
{
class ImporterContext;

struct EGHandle
{
    int egSlot{-1};
};

// Times are in seconds; the helper performs the seconds→normalized conversion.
// Sustain is a level (0..1); the importer converts from dB or % before calling.
// Any unset optional is left at the zone's current value (supports partial updates,
// e.g. SFZ writing only the opcodes that were present).
struct EnvelopeArgs
{
    std::optional<float> delaySeconds;
    std::optional<float> attackSeconds;
    std::optional<float> holdSeconds;
    std::optional<float> decaySeconds;
    std::optional<float> sustainLevel;
    std::optional<float> releaseSeconds;

    std::optional<float> attackShape;
    std::optional<float> decayShape;
    std::optional<float> releaseShape;
};

EGHandle importZoneEnvelope(engine::Zone &, ImporterContext &, int egIdx, const EnvelopeArgs &);
} // namespace scxt::import_support

#endif
