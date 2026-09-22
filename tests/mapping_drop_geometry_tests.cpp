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

#include "catch2/catch2.hpp"

#include <set>

#include "engine/drop_mapping.h"

namespace
{
scxt::engine::DropGeometry at(int n, float key, float fromTop)
{
    scxt::engine::DropGeometry g;
    g.nElements = n;
    g.key = key;
    g.fromTop = fromTop;
    return g;
}

// every range has to be a usable zone on a 0..127 keyboard
void requireWellFormed(const std::vector<scxt::engine::DropRange> &rs)
{
    REQUIRE(!rs.empty());
    for (const auto &r : rs)
    {
        INFO("root " << r.root << " key " << r.keyLo << ".." << r.keyHi << " vel " << r.velLo
                     << ".." << r.velHi);
        REQUIRE(r.keyLo >= 0);
        REQUIRE(r.keyHi <= 127);
        REQUIRE(r.keyLo <= r.keyHi);
        REQUIRE(r.root >= 0);
        REQUIRE(r.root <= 127);
        REQUIRE(r.velLo >= 0);
        REQUIRE(r.velHi <= 127);
        REQUIRE(r.velLo <= r.velHi);
    }
}
} // namespace

TEST_CASE("Drop Geometry Is Always Well Formed")
{
    SECTION("across the whole gesture surface")
    {
        for (int n : {1, 2, 3, 4, 5, 7, 12, 16, 64, 126, 127, 128, 200})
        {
            for (float key = 0.f; key <= 127.f; key += 7.f)
            {
                for (float top = 0.f; top <= 1.f; top += 0.05f)
                {
                    // plain, shift, alt, over the keyboard, over its lower half
                    for (auto variant : {0, 1, 2, 3, 4})
                    {
                        auto g = at(n, key, top);
                        g.shift = (variant == 1);
                        g.alt = (variant == 2);
                        g.overKeyboard = (variant >= 3);
                        g.inLowerKeyboardHalf = (variant == 4);
                        INFO("n " << n << " key " << key << " fromTop " << top << " variant "
                                  << variant);
                        requireWellFormed(scxt::engine::dropRangesFor(g));
                    }
                }
            }
        }
    }

    SECTION("degenerate element counts do not produce an empty result")
    {
        requireWellFormed(scxt::engine::dropRangesFor(at(0, 60, 0.5)));
        requireWellFormed(scxt::engine::dropRangesFor(at(-1, 60, 0.5)));
    }
}

TEST_CASE("Drop Geometry Element Counts")
{
    SECTION("one range per element when spreading over the keyboard")
    {
        for (int n : {2, 3, 5, 16})
        {
            auto r = scxt::engine::dropRangesFor(at(n, 60, 0.5));
            REQUIRE((int)r.size() == n);
        }
    }

    SECTION("one range per element when spreading over velocity")
    {
        for (int n : {2, 3, 5, 16})
        {
            auto g = at(n, 60, 0.5);
            g.shift = true;
            auto r = scxt::engine::dropRangesFor(g);
            REQUIRE((int)r.size() == n);
        }
    }

    SECTION("alt collapses to a single shared range")
    {
        auto g = at(7, 60, 0.5);
        g.alt = true;
        REQUIRE(scxt::engine::dropRangesFor(g).size() == 1);
    }

    SECTION("a mapped instrument ignores the gesture entirely")
    {
        auto g = at(7, 20, 0.1);
        g.isMappedInstrument = true;
        auto r = scxt::engine::dropRangesFor(g);
        REQUIRE(r.size() == 1);
        REQUIRE(r[0].keyLo == 0);
        REQUIRE(r[0].keyHi == 127);
    }
}

TEST_CASE("Drop Geometry Keyboard Spread")
{
    SECTION("two elements at the top of the travel stay on the keyboard")
    {
        // the pair is wider than the keyboard here, which used to underflow to 1.8e19
        auto r = scxt::engine::dropRangesFor(at(2, 60, 0.f));
        REQUIRE(r.size() == 2);
        requireWellFormed(r);
        REQUIRE(r[0].keyLo == 0);
    }

    SECTION("zones run left to right and do not leave gaps")
    {
        auto r = scxt::engine::dropRangesFor(at(4, 60, 0.5));
        REQUIRE(r.size() == 4);
        for (size_t i = 1; i < r.size(); ++i)
        {
            INFO("zone " << i);
            REQUIRE(r[i].keyLo >= r[i - 1].keyLo);
            REQUIRE(r[i].keyLo == r[i - 1].keyHi + 1);
        }
    }

    SECTION("each root sits inside its own zone")
    {
        for (int n : {2, 3, 5, 9})
        {
            auto r = scxt::engine::dropRangesFor(at(n, 64, 0.4));
            for (const auto &e : r)
            {
                INFO("n " << n << " root " << e.root);
                REQUIRE(e.root >= e.keyLo);
                REQUIRE(e.root <= e.keyHi);
            }
        }
    }

    SECTION("dropping at the bottom of the travel gives single key zones")
    {
        auto r = scxt::engine::dropRangesFor(at(3, 60, 1.f));
        REQUIRE(r.size() == 3);
        for (const auto &e : r)
            REQUIRE(e.keyLo == e.keyHi);
    }
}

TEST_CASE("Drop Geometry Velocity Spread")
{
    SECTION("velocity covers 0 to 127 with no gaps and no overlap")
    {
        for (int n : {2, 3, 4, 8})
        {
            auto g = at(n, 60, 0.5);
            g.shift = true;
            auto r = scxt::engine::dropRangesFor(g);
            REQUIRE((int)r.size() == n);
            REQUIRE(r.front().velLo == 0);
            REQUIRE(r.back().velHi == 127);
            for (size_t i = 1; i < r.size(); ++i)
            {
                INFO("n " << n << " band " << i);
                REQUIRE(r[i].velLo == r[i - 1].velHi + 1);
            }
        }
    }

    SECTION("every band shares one key range")
    {
        auto g = at(5, 60, 0.3);
        g.shift = true;
        auto r = scxt::engine::dropRangesFor(g);
        for (const auto &e : r)
        {
            REQUIRE(e.keyLo == r[0].keyLo);
            REQUIRE(e.keyHi == r[0].keyHi);
            REQUIRE(e.root == r[0].root);
        }
    }
}

TEST_CASE("Drop Geometry Width Ladder")
{
    SECTION("the travel runs widest at the top to one key at the bottom")
    {
        REQUIRE(scxt::engine::zoneWidthAt(0.f) == 60);
        REQUIRE(scxt::engine::zoneWidthAt(1.f) == 1);
    }

    SECTION("width never widens as the cursor moves down")
    {
        auto prev = scxt::engine::zoneWidthAt(0.f);
        for (float t = 0.f; t <= 1.f; t += 0.01f)
        {
            auto w = scxt::engine::zoneWidthAt(t);
            INFO("fromTop " << t << " width " << w << " previous " << prev);
            REQUIRE(w <= prev);
            prev = w;
        }
    }

    SECTION("every rung is reachable and no other width is")
    {
        std::set<int> seen;
        for (float t = 0.f; t <= 1.f; t += 0.001f)
            seen.insert(scxt::engine::zoneWidthAt(t));

        std::set<int> expected{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 24, 36, 48, 60};
        REQUIRE(seen == expected);
    }

    SECTION("only the top of the travel is the full overlap band")
    {
        REQUIRE(scxt::engine::isFullOverlapAt(0.f));
        REQUIRE(!scxt::engine::isFullOverlapAt(0.5f));
        REQUIRE(!scxt::engine::isFullOverlapAt(1.f));
    }
}

TEST_CASE("Drop Geometry Anchors Left")
{
    SECTION("the cursor is the left edge of the spread, which extends right")
    {
        auto g = at(4, 40, 0.6);
        auto r = scxt::engine::dropRangesFor(g);
        auto w = scxt::engine::zoneWidthAt(0.6);

        REQUIRE(r.size() == 4);
        REQUIRE(r[0].keyLo == 40);
        for (size_t i = 0; i < r.size(); ++i)
        {
            INFO("zone " << i << " width " << w);
            REQUIRE(r[i].keyLo == 40 + (int)i * w);
            REQUIRE(r[i].keyHi == r[i].keyLo + w - 1);
        }
    }

    SECTION("the root of every zone is its own left edge")
    {
        for (float top : {0.2f, 0.5f, 0.8f})
        {
            auto r = scxt::engine::dropRangesFor(at(5, 30, top));
            for (const auto &e : r)
            {
                INFO("fromTop " << top);
                REQUIRE(e.root == e.keyLo);
            }
        }
    }

    SECTION("a single sample follows the same rule")
    {
        auto r = scxt::engine::dropRangesFor(at(1, 48, 0.5));
        REQUIRE(r.size() == 1);
        REQUIRE(r[0].keyLo == 48);
        REQUIRE(r[0].root == 48);
        REQUIRE(r[0].keyHi == 48 + scxt::engine::zoneWidthAt(0.5) - 1);
    }
}

TEST_CASE("Drop Geometry Scrunches The Overflow")
{
    SECTION("what does not fit piles onto the last note")
    {
        // wide zones from high up the keyboard cannot all fit
        auto r = scxt::engine::dropRangesFor(at(8, 120, 0.3));
        REQUIRE(r.size() == 8);
        requireWellFormed(r);
        REQUIRE(r.back().keyLo == 127);
        REQUIRE(r.back().keyHi == 127);
    }

    SECTION("a zone straddling the end is truncated, not moved")
    {
        auto w = scxt::engine::zoneWidthAt(0.5);
        auto r = scxt::engine::dropRangesFor(at(1, (float)(127 - w / 2), 0.5));
        REQUIRE(r[0].keyHi == 127);
        REQUIRE(r[0].keyLo == 127 - w / 2);
    }

    SECTION("dropping on the last key still gives one usable zone each")
    {
        auto r = scxt::engine::dropRangesFor(at(6, 127, 0.5));
        REQUIRE(r.size() == 6);
        requireWellFormed(r);
        for (const auto &e : r)
        {
            REQUIRE(e.keyLo == 127);
            REQUIRE(e.keyHi == 127);
        }
    }
}

TEST_CASE("Drop Geometry Full Overlap Band")
{
    SECTION("the top of the travel overlaps everything across the keyboard")
    {
        auto r = scxt::engine::dropRangesFor(at(5, 60, 0.f));
        REQUIRE(r.size() == 5);
        for (const auto &e : r)
        {
            REQUIRE(e.keyLo == 0);
            REQUIRE(e.keyHi == 127);
            REQUIRE(e.velLo == 0);
            REQUIRE(e.velHi == 127);
        }
    }

    SECTION("overlapped zones stay separate rather than collapsing to variants")
    {
        // one range is the signal for a variant stack, so overlap must not send it
        auto r = scxt::engine::dropRangesFor(at(5, 60, 0.f));
        REQUIRE(r.size() == 5);
    }

    SECTION("the root stays under the cursor rather than snapping to zero")
    {
        auto r = scxt::engine::dropRangesFor(at(3, 72, 0.f));
        for (const auto &e : r)
            REQUIRE(e.root == 72);
    }
}

TEST_CASE("Drop Geometry Over The Keyboard")
{
    SECTION("the upper half distributes over velocity on one key")
    {
        auto g = at(4, 55, 1.f);
        g.overKeyboard = true;
        auto r = scxt::engine::dropRangesFor(g);

        REQUIRE(r.size() == 4);
        REQUIRE(r.front().velLo == 0);
        REQUIRE(r.back().velHi == 127);
        for (size_t i = 0; i < r.size(); ++i)
        {
            INFO("band " << i);
            REQUIRE(r[i].keyLo == 55);
            REQUIRE(r[i].keyHi == 55);
            if (i > 0)
                REQUIRE(r[i].velLo == r[i - 1].velHi + 1);
        }
    }

    SECTION("the lower half overlaps on one key with no velocity split")
    {
        auto g = at(4, 55, 1.f);
        g.overKeyboard = true;
        g.inLowerKeyboardHalf = true;
        auto r = scxt::engine::dropRangesFor(g);

        REQUIRE(r.size() == 4);
        for (const auto &e : r)
        {
            REQUIRE(e.keyLo == 55);
            REQUIRE(e.keyHi == 55);
            REQUIRE(e.velLo == 0);
            REQUIRE(e.velHi == 127);
        }
    }

    SECTION("the keyboard overrides the width the travel would have picked")
    {
        auto g = at(2, 55, 0.f); // would be the full overlap band in the mapping area
        g.overKeyboard = true;
        g.inLowerKeyboardHalf = true;
        auto r = scxt::engine::dropRangesFor(g);
        REQUIRE(r[0].keyLo == 55);
        REQUIRE(r[0].keyHi == 55);
    }
}
