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

#ifndef SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_FILTER_H
#define SCXT_SRC_SCXT_CORE_SAMPLE_IMPORT_SUPPORT_IMPORT_FILTER_H

#include <optional>
#include "engine/zone.h"
#include "dsp/processor/processor.h"

namespace scxt::import_support
{
class ImporterContext;

// Handle returned by importZoneFilter; carries the slot index and processor
// type so the modulation resolver can map FilterParam::Cutoff to the right
// floatParams index.
struct FilterHandle
{
    int procSlot{-1};
    dsp::processor::ProcessorType type{dsp::processor::ProcessorType::proct_none};
};

// type is required; the helper logs + asserts + returns if it is unset.
// cutoff and resonance are left at the processor's current value when unset.
// Values are in the processor's native units (e.g. for CytomicSVF, cutoff is
// semitones offset from MIDI 69, resonance is 0..1).
struct FilterArgs
{
    std::optional<dsp::processor::ProcessorType> type;
    std::optional<float> cutoff;
    std::optional<float> resonance;
};

FilterHandle importZoneFilter(engine::Zone &, ImporterContext &, int procSlot, const FilterArgs &);
} // namespace scxt::import_support

#endif
