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

#include "import_modulation.h"
#include "import_harness.h"
#include "modulation/voice_matrix.h"

namespace scxt::import_support
{
using SR = voice::modulation::MatrixConfig::SourceIdentifier;
using TG = voice::modulation::MatrixConfig::TargetIdentifier;

static int filterParamIdx(dsp::processor::ProcessorType type, FilterParam p)
{
    switch (type)
    {
    case dsp::processor::ProcessorType::proct_CytomicSVF:
        if (p == FilterParam::Cutoff)
            return 0;
        if (p == FilterParam::Resonance)
            return 2;
        return 0;
    default:
        return 0;
    }
}

ImportedTarget ImportedTarget::filter(FilterHandle h, FilterParam p)
{
    return {ImportedTargetKind::FilterParam, h.procSlot, filterParamIdx(h.type, p)};
}

int addImportedModRoute(engine::Zone &zone, ImporterContext &ctx, const ImportedModRoute &r)
{
    int rowIdx = -1;
    auto &routes = zone.routingTable.routes;
    for (int i = 0; i < (int)routes.size(); ++i)
    {
        if (routes[i].hasDefaultValues())
        {
            rowIdx = i;
            break;
        }
    }
    if (rowIdx < 0)
    {
        ctx.software_error("addImportedModRoute", "no free routing row available");
        return -1;
    }

    auto &row = routes[rowIdx];

    SR src{};
    switch (r.source.kind)
    {
    case ImportedSourceKind::LFO:
        src = voice::modulation::MatrixEndpoints::Sources::lfoSource(r.source.index);
        break;
    case ImportedSourceKind::EG:
        src = voice::modulation::MatrixEndpoints::Sources::egSource(r.source.index);
        break;
    }
    row.source = src;

    TG tgt{};
    switch (r.target.kind)
    {
    case ImportedTargetKind::Pitch:
        tgt = voice::modulation::MatrixEndpoints::MappingTarget::pitchOffsetA;
        break;
    case ImportedTargetKind::FilterParam:
        tgt = voice::modulation::MatrixEndpoints::ProcessorTarget::floatParam(r.target.procSlot,
                                                                              r.target.paramIdx);
        break;
    }
    row.target = tgt;

    row.depth = r.depth;
    row.active = r.active;

    zone.onRoutingChanged();
    return rowIdx;
}
} // namespace scxt::import_support
