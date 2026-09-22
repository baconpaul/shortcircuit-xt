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

#include "drop_mapping.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace scxt::engine
{
namespace
{
// widest first, so the top of the travel is the widest zone
constexpr std::array<int16_t, 16> widthLadder{60, 48, 36, 24, 12, 11, 10, 9,
                                              8,  7,  6,  5,  4,  3,  2,  1};

constexpr float fullOverlapBand{0.05f};

int16_t lastKeyOf(int firstKey, int width) { return (int16_t)std::min(firstKey + width - 1, 127); }

// tile 0..127 across the elements, all of them sharing one key range
std::vector<DropRange> velocitySplit(int n, int16_t root, int16_t keyLo, int16_t keyHi, float bend)
{
    std::vector<DropRange> ranges;
    ranges.reserve(n);

    auto gamma = std::pow(2.f, -std::clamp(bend, -1.f, 1.f));
    int start{0};

    for (int i = 0; i < n; ++i)
    {
        int end{127};
        if (i < n - 1)
        {
            auto t = std::pow((float)(i + 1) / n, gamma);
            end = std::clamp((int)std::round(127.f * t), 0, 127);
        }
        // with more samples than velocities the bands run out of room, so stop inverting
        end = std::max(end, start);
        ranges.emplace_back(root, keyLo, keyHi, (int16_t)start, (int16_t)end);
        start = std::min(end + 1, 127);
    }

    return ranges;
}
} // namespace

bool isFullOverlapAt(float fromTop) { return fromTop <= fullOverlapBand; }

int16_t zoneWidthAt(float fromTop)
{
    auto t = std::clamp((fromTop - fullOverlapBand) / (1.f - fullOverlapBand), 0.f, 1.f);
    auto steps = (int)widthLadder.size();
    return widthLadder[std::clamp((int)(t * steps), 0, steps - 1)];
}

std::vector<DropRange> dropRangesFor(const DropGeometry &g)
{
    if (g.isMappedInstrument || g.nElements <= 0)
        return {{60, 0, 127}};

    auto n = g.nElements;
    auto key = (int16_t)std::clamp(g.key, 0.f, 127.f);
    auto width = zoneWidthAt(g.fromTop);

    // the root is the left edge of the zone, and the cursor is the left edge of the spread
    int16_t root{key}, firstLo{key}, firstHi{lastKeyOf(key, width)};
    bool overlapAll{false};

    if (g.overKeyboard)
    {
        // everything lands on the one key under the cursor
        firstLo = key;
        firstHi = key;
        overlapAll = true;
    }
    else if (isFullOverlapAt(g.fromTop))
    {
        firstLo = 0;
        firstHi = 127;
        overlapAll = true;
    }

    if (g.alt)
        return {{root, firstLo, firstHi}};

    // the upper half of the keyboard splits over velocity without needing shift
    if (g.shift || (g.overKeyboard && !g.inLowerKeyboardHalf))
        return velocitySplit(n, root, firstLo, firstHi, g.velocityBend);

    if (overlapAll)
        return std::vector<DropRange>(n, DropRange(root, firstLo, firstHi));

    std::vector<DropRange> ranges;
    ranges.reserve(n);
    for (int i = 0; i < n; ++i)
    {
        auto lo = key + i * width;
        // what runs off the end of the keyboard piles up on the last note
        if (lo > 127)
            ranges.emplace_back(127, 127, 127);
        else
            ranges.emplace_back((int16_t)lo, (int16_t)lo, lastKeyOf(lo, width));
    }
    return ranges;
}

} // namespace scxt::engine
