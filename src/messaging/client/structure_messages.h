/*
 * Shortcircuit XT - a Surge Synth Team product
 *
 * A fully featured creative sampler, available as a standalone
 * and plugin for multiple platforms.
 *
 * Copyright 2019 - 2024, Various authors, as described in the github
 * transaction log.
 *
 * ShortcircuitXT is released under the Gnu General Public Licence
 * V3 or later (GPL-3.0-or-later). The license is found in the file
 * "LICENSE" in the root of this repository or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Individual sections of code which comprises ShortcircuitXT in this
 * repository may also be used under an MIT license. Please see the
 * section  "Licensing" in "README.md" for details.
 *
 * ShortcircuitXT is inspired by, and shares code with, the
 * commercial product Shortcircuit 1 and 2, released by VemberTech
 * in the mid 2000s. The code for Shortcircuit 2 was opensourced in
 * 2020 at the outset of this project.
 *
 * All source for ShortcircuitXT is available at
 * https://github.com/surge-synthesizer/shortcircuit-xt
 */

#ifndef SCXT_SRC_MESSAGING_CLIENT_STRUCTURE_MESSAGES_H
#define SCXT_SRC_MESSAGING_CLIENT_STRUCTURE_MESSAGES_H

#include "messaging/client/detail/client_json_details.h"
#include "json/selection_traits.h"
#include "selection/selection_manager.h"
#include "engine/engine.h"
#include "client_macros.h"

namespace scxt::messaging::client
{
/*
 * Send the full structure of all groups and zones or for a single part
 * in a single message
 */
inline void pgzSerialSide(const int &partNum, const engine::Engine &engine, MessageController &cont)
{
    serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(), cont);
}

CLIENT_SERIAL_REQUEST_RESPONSE(PartGroupZoneStructure, c2s_request_pgz_structure, int32_t,
                               s2c_send_pgz_structure, engine::Engine::pgzStructure_t,
                               pgzSerialSide(payload, engine, cont), onStructureUpdated);

SERIAL_TO_CLIENT(SendAllProcessorDescriptions, s2c_send_all_processor_descriptions,
                 std::vector<dsp::processor::ProcessorDescription>, onAllProcessorDescriptions);

/*
 * A message the client auto-sends when it registers just so we can respond
 */

inline void doRegisterClient(engine::Engine &engine, MessageController &cont)
{
    assert(cont.threadingChecker.isSerialThread());

    engine.sendFullRefreshToClient();
}
CLIENT_TO_SERIAL(RegisterClient, c2s_register_client, bool, doRegisterClient(engine, cont));

/*
 * A message the client auto-sends when it registers just so we can respond
 */
inline void addSample(const std::string &payload, engine::Engine &engine, MessageController &cont)
{
    assert(cont.threadingChecker.isSerialThread());
    auto p = fs::path(fs::u8path(payload));
    engine.loadSampleIntoSelectedPartAndGroup(p);
}
CLIENT_TO_SERIAL(AddSample, c2s_add_sample, std::string, addSample(payload, engine, cont);)

// sample, root, midi start end, vel start end
using addSampleWithRange_t = std::tuple<std::string, int, int, int, int, int>;
inline void addSampleWithRange(const addSampleWithRange_t &payload, engine::Engine &engine,
                               MessageController &cont)
{
    assert(cont.threadingChecker.isSerialThread());
    auto p = fs::path(fs::u8path(std::get<0>(payload)));
    auto rk = std::get<1>(payload);
    auto kr = engine::KeyboardRange(std::get<2>(payload), std::get<3>(payload));
    auto vr = engine::VelocityRange(std::get<4>(payload), std::get<5>(payload));
    engine.loadSampleIntoSelectedPartAndGroup(p, rk, kr, vr);
}
CLIENT_TO_SERIAL(AddSampleWithRange, c2s_add_sample_with_range, addSampleWithRange_t,
                 addSampleWithRange(payload, engine, cont);)

using addCompoundElementWithRange_t =
    std::tuple<sample::compound::CompoundElement, int, int, int, int, int>;
inline void addCompoundElementWithRange(const addCompoundElementWithRange_t &payload,
                                        engine::Engine &engine, MessageController &cont)
{
    assert(cont.threadingChecker.isSerialThread());
    auto el = std::get<0>(payload);
    auto rk = std::get<1>(payload);
    auto kr = engine::KeyboardRange(std::get<2>(payload), std::get<3>(payload));
    auto vr = engine::VelocityRange(std::get<4>(payload), std::get<5>(payload));
    engine.loadCompoundElementIntoSelectedPartAndGroup(el, rk, kr, vr);
}
CLIENT_TO_SERIAL(AddCompoundElementWithRange, c2s_add_compound_element_with_range,
                 addCompoundElementWithRange_t, addCompoundElementWithRange(payload, engine, cont);)

// sample, part, group, wone, sampleid
using addSampleInZone_t = std::tuple<std::string, int, int, int, int>;
inline void addSampleInZone(const addSampleInZone_t &payload, engine::Engine &engine,
                            MessageController &cont)
{
    assert(cont.threadingChecker.isSerialThread());
    auto path = fs::path(fs::u8path(std::get<0>(payload)));
    auto part{std::get<1>(payload)};
    auto group{std::get<2>(payload)};
    auto zone{std::get<3>(payload)};
    auto sampleID{std::get<4>(payload)};

    engine.loadSampleIntoZone(path, part, group, zone, sampleID);
}
CLIENT_TO_SERIAL(AddSampleInZone, c2s_add_sample_in_zone, addSampleInZone_t,
                 addSampleInZone(payload, engine, cont);)

// sample, part, group, wone, sampleid
using addCompoundElementInZone_t =
    std::tuple<sample::compound::CompoundElement, int, int, int, int>;
inline void addCompoundElementInZone(const addCompoundElementInZone_t &payload,
                                     engine::Engine &engine, MessageController &cont)
{
    assert(cont.threadingChecker.isSerialThread());
    auto el = std::get<0>(payload);
    auto part{std::get<1>(payload)};
    auto group{std::get<2>(payload)};
    auto zone{std::get<3>(payload)};
    auto sampleID{std::get<4>(payload)};

    engine.loadCompoundElementIntoZone(el, part, group, zone, sampleID);
}
CLIENT_TO_SERIAL(AddCompoundElementInZone, c2s_add_compound_element_in_zone,
                 addCompoundElementInZone_t, addCompoundElementInZone(payload, engine, cont);)

inline void createGroupIn(int partNumber, engine::Engine &engine, MessageController &cont)
{
    if (partNumber < 0 || partNumber > numParts)
        partNumber = 0;
    SCLOG_IF(groupZoneMutation, "Creating group in part " << partNumber);
    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [p = partNumber](auto &e) { e.getPatch()->getPart(p)->addGroup(); },
        [p = partNumber](auto &engine) {
            SCLOG_IF(groupZoneMutation, "Responding with part group zone structure in " << p);
            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));

            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(p)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
            if (engine.getSelectionManager()->currentlySelectedZones().empty())
            {
                SCLOG_IF(selection, "Empty selection when creating group in " << p);
                // ooof what to do
                int32_t g = engine.getPatch()->getPart(p)->getGroups().size() - 1;
                engine.getSelectionManager()->selectAction(
                    selection::SelectionManager::SelectActionContents(p, g, -1, true, true, true));
            }
        });
}
CLIENT_TO_SERIAL(CreateGroup, c2s_create_group, int, createGroupIn(payload, engine, cont));

inline void removeZone(const selection::SelectionManager::ZoneAddress &a, engine::Engine &engine,
                       MessageController &cont)
{
    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [s = a](auto &e) {
            auto &zoneO = e.getPatch()->getPart(s.part)->getGroup(s.group)->getZone(s.zone);
            e.terminateVoicesForZone(*zoneO);
            auto zid = zoneO->id;
            auto z = e.getPatch()->getPart(s.part)->getGroup(s.group)->removeZone(zid);
            auto zoneToFree = z.release();

            messaging::audio::AudioToSerialization a2s;
            a2s.id = audio::a2s_delete_this_pointer;
            a2s.payloadType = audio::AudioToSerialization::TO_BE_DELETED;
            a2s.payload.delThis.ptr = zoneToFree;
            a2s.payload.delThis.type = audio::AudioToSerialization::ToBeDeleted::engine_Zone;
            e.getMessageController()->sendAudioToSerialization(a2s);
        },
        [t = a](auto &engine) {
            engine.getSampleManager()->purgeUnreferencedSamples();
            engine.getSelectionManager()->guaranteeConsistencyAfterDeletes(engine, true, t);
            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));
            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(t.part)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
        });
}
CLIENT_TO_SERIAL(DeleteZone, c2s_delete_zone, selection::SelectionManager::ZoneAddress,
                 removeZone(payload, engine, cont));

CLIENT_TO_SERIAL(DuplicateZone, c2s_duplicate_zone, selection::SelectionManager::ZoneAddress,
                 engine.duplicateZone(payload));
CLIENT_TO_SERIAL(CopyZone, c2s_copy_zone, selection::SelectionManager::ZoneAddress,
                 engine.copyZone(payload));
CLIENT_TO_SERIAL(PasteZone, c2s_paste_zone, selection::SelectionManager::ZoneAddress,
                 engine.pasteZone(payload));

inline void removeSelectedZones(const bool &, engine::Engine &engine, MessageController &cont)
{
    auto part = engine.getSelectionManager()->selectedPart;

    auto zs = engine.getSelectionManager()->allSelectedZones[part];

    if (zs.empty())
        return;

    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [sz = zs](auto &e) {
            // I know this allocates and deallocates on audio thread
            std::vector<std::tuple<int, int, ZoneID>> ids;
            for (auto s : sz)
            {
                auto &zoneO = e.getPatch()->getPart(s.part)->getGroup(s.group)->getZone(s.zone);
                e.terminateVoicesForZone(*zoneO);

                ids.push_back(
                    {s.part, s.group,
                     e.getPatch()->getPart(s.part)->getGroup(s.group)->getZone(s.zone)->id});
            }
            for (auto [p, g, zid] : ids)
            {
                auto z = e.getPatch()->getPart(p)->getGroup(g)->removeZone(zid);
                auto zoneToFree = z.release();

                messaging::audio::AudioToSerialization a2s;
                a2s.id = audio::a2s_delete_this_pointer;
                a2s.payloadType = audio::AudioToSerialization::TO_BE_DELETED;
                a2s.payload.delThis.ptr = zoneToFree;
                a2s.payload.delThis.type = audio::AudioToSerialization::ToBeDeleted::engine_Zone;
                e.getMessageController()->sendAudioToSerialization(a2s);
            }
        },
        [t = part](auto &engine) {
            engine.getSampleManager()->purgeUnreferencedSamples();
            engine.getSelectionManager()->guaranteeConsistencyAfterDeletes(engine, true,
                                                                           {t, -1, -1});

            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));
            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(t)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
        });
}
CLIENT_TO_SERIAL(DeleteAllSelectedZones, c2s_delete_selected_zones, bool,
                 removeSelectedZones(payload, engine, cont));

inline void removeGroup(const selection::SelectionManager::ZoneAddress &a, engine::Engine &engine,
                        MessageController &cont)
{
    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [s = a](auto &e) {
            if (e.getPatch()->getPart(s.part)->getGroups().size() < s.group)
                return;

            auto &groupO = e.getPatch()->getPart(s.part)->getGroup(s.group);
            e.terminateVoicesForGroup(*groupO);

            auto gid = e.getPatch()->getPart(s.part)->getGroup(s.group)->id;
            auto groupToFree = e.getPatch()->getPart(s.part)->removeGroup(gid).release();

            messaging::audio::AudioToSerialization a2s;
            a2s.id = audio::a2s_delete_this_pointer;
            a2s.payloadType = audio::AudioToSerialization::TO_BE_DELETED;
            a2s.payload.delThis.ptr = groupToFree;
            a2s.payload.delThis.type = audio::AudioToSerialization::ToBeDeleted::engine_Group;
            e.getMessageController()->sendAudioToSerialization(a2s);
        },
        [t = a](auto &engine) {
            engine.getSampleManager()->purgeUnreferencedSamples();
            engine.getSelectionManager()->guaranteeConsistencyAfterDeletes(engine, false, t);

            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));
            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(t.part)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
        });
}
CLIENT_TO_SERIAL(DeleteGroup, c2s_delete_group, selection::SelectionManager::ZoneAddress,
                 removeGroup(payload, engine, cont));

inline void clearPart(const int p, engine::Engine &engine, MessageController &cont)
{
    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [pt = p](auto &e) {
            auto &part = e.getPatch()->getPart(pt);
            while (!part->getGroups().empty())
            {
                auto &groupO = part->getGroup(0);
                e.terminateVoicesForGroup(*groupO);
                auto gid = groupO->id;
                auto g = part->removeGroup(gid);

                auto groupToFree = g.release();

                messaging::audio::AudioToSerialization a2s;
                a2s.id = audio::a2s_delete_this_pointer;
                a2s.payloadType = audio::AudioToSerialization::TO_BE_DELETED;
                a2s.payload.delThis.ptr = groupToFree;
                a2s.payload.delThis.type = audio::AudioToSerialization::ToBeDeleted::engine_Group;
                e.getMessageController()->sendAudioToSerialization(a2s);
            }
        },
        [pt = p](auto &engine) {
            engine.getSampleManager()->purgeUnreferencedSamples();
            engine.getSelectionManager()->guaranteeConsistencyAfterDeletes(engine, false,
                                                                           {pt, -1, -1});

            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));
            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(pt)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
        });
}
CLIENT_TO_SERIAL(ClearPart, c2s_clear_part, int, clearPart(payload, engine, cont));

using zoneAddressFromTo_t = std::pair<std::set<selection::SelectionManager::ZoneAddress>,
                                      selection::SelectionManager::ZoneAddress>;
inline void moveZonesFromTo(const zoneAddressFromTo_t &payload, engine::Engine &engine,
                            MessageController &cont)
{
    auto src = payload.first;
    auto &tgt = payload.second;

    if (src.empty())
    {
        auto sz = engine.getSelectionManager()->currentlySelectedZones();
        src = std::set<selection::SelectionManager::ZoneAddress>(sz.begin(), sz.end());
        SCLOG("Empty src so we populated it with " << src.size() << " zones");
    }
    assert(src.begin()->part == tgt.part);

    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [ss = src, t = tgt](auto &e) {
            // This is a little hairy byt I know the zones will not be deleted during this function
            std::vector<engine::Zone *> sourceZones;
            std::vector<engine::Group *> sourceGroups;

            auto gid = t.group;
            if (gid < 0)
            {
                gid = e.getPatch()->getPart(t.part)->addGroup() - 1;
            }

            const std::unique_ptr<engine::Group> &targetGroup =
                e.getPatch()->getPart(t.part)->getGroup(gid);

            for (const auto &addr : ss)
            {
                sourceZones.push_back(e.getPatch()
                                          ->getPart(addr.part)
                                          ->getGroup(addr.group)
                                          ->getZone(addr.zone)
                                          .get());
                sourceGroups.push_back(
                    e.getPatch()->getPart(addr.part)->getGroup(addr.group).get());
            }

            for (size_t i = 0; i < sourceZones.size(); i++)
            {
                if (sourceGroups.at(i) == targetGroup.get())
                {
                    if (t.zone >= 0)
                    {
                        e.terminateVoicesForZone(*sourceZones.at(i));
                        e.terminateVoicesForZone(*targetGroup->getZone(t.zone));
                        targetGroup->swapZonesByIndex(
                            targetGroup->getZoneIndex(sourceZones.at(i)->id), t.zone);
                    }
                }
                else
                {
                    e.terminateVoicesForZone(*sourceZones.at(i));

                    auto zid = sourceZones.at(i)->id;
                    auto z = sourceZones.at(i)->parentGroup->removeZone(zid);
                    if (z)
                        targetGroup->addZone(z);
                }
            }
        },
        [ss = src, tt = tgt](auto &engine) {
            bool needsReselect = false;
            auto t = tt;
            if (t.group < 0)
                t.group = engine.getPatch()->getPart(t.part)->getGroups().size() - 1;

            for (const auto &s : ss)
            {
                if (s.group != t.group)
                {
                    needsReselect = true;
                }
            }
            if (needsReselect)
            {
                // they've all been added at the end
                auto zc = engine.getPatch()->getPart(t.part)->getGroup(t.group)->getZones().size() -
                          -ss.size();
                for (int i = 0; i < ss.size(); i++)
                {
                    // retain the selection set
                    auto tc = t;
                    tc.zone = zc + i;
                    auto act =
                        selection::SelectionManager::SelectActionContents(tc, true, i == 0, i == 0);
                    engine.getSelectionManager()->selectAction(act);
                }
            }

            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));
            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(t.part)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
        });
}
CLIENT_TO_SERIAL(MoveZonesFromTo, c2s_move_zone, zoneAddressFromTo_t,
                 moveZonesFromTo(payload, engine, cont));

using moveGroupAddress_t =
    std::pair<selection::SelectionManager::ZoneAddress, selection::SelectionManager::ZoneAddress>;
inline void moveGroupTo(const moveGroupAddress_t &payload, engine::Engine &engine,
                        MessageController &cont)
{
    auto &src = payload.first;
    auto &tgt = payload.second;

    assert(src.part == tgt.part);
    if (src.part < 0 || src.part >= scxt::numParts)
        return;
    if (src.group < 0 || tgt.group < 0)
        return;
    if (src.part != tgt.part)
        return;
    if (src.group == tgt.group)
        return;

    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [ss = src, tt = tgt](auto &e) {
            // This is a little hairy byt I know the zones will not be deleted during this function
            auto &pt = e.getPatch()->getPart(ss.part);

            e.voiceManager.allSoundsOff();
            pt->moveGroupToAfter(ss.group, tt.group);
        },
        [ss = src, tt = tgt](auto &engine) {
            serializationSendToClient(s2c_send_pgz_structure, engine.getPartGroupZoneStructure(),
                                      *(engine.getMessageController()));
            serializationSendToClient(s2c_send_selected_group_zone_mapping_summary,
                                      engine.getPatch()->getPart(tt.part)->getZoneMappingSummary(),
                                      *(engine.getMessageController()));
        });
}
CLIENT_TO_SERIAL(MoveGroupToAfter, c2s_move_group, moveGroupAddress_t,
                 moveGroupTo(payload, engine, cont));

inline void doActivateNextPart(messaging::MessageController &cont)
{
    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [](auto &e) {
            // We should really do this on audio thread but test it here quickly
            for (auto &pt : *(e.getPatch()))
            {
                if (!pt->configuration.active)
                {
                    pt->clearGroups();
                    pt->configuration.active = true;
                    break;
                }
            }
        },
        [](const auto &e) { e.sendFullRefreshToClient(); });
}
CLIENT_TO_SERIAL(ActivateNextPart, c2s_activate_next_part, bool, doActivateNextPart(cont));

inline void doDeactivatePart(int part, messaging::MessageController &cont)
{
    cont.scheduleAudioThreadCallbackUnderStructureLock(
        [part](auto &e) {
            e.getPatch()->getPart(part)->configuration.active = false;
            bool anySel{false};
            for (auto &pt : *(e.getPatch()))
            {
                if (pt->configuration.active)
                    anySel = true;
            }
            if (!anySel)
            {
                e.getPatch()->getPart(0)->clearGroups();
                e.getPatch()->getPart(0)->configuration.active = true;
            }
        },
        [part](const auto &e) {
            if (e.getSelectionManager()->selectedPart == part)
            {
                int tpt{0}, spt{-1};
                for (auto &pt : *(e.getPatch()))
                {
                    if (pt->configuration.active && spt < 0)
                        spt = tpt;
                    tpt++;
                }
                if (spt >= 0)
                {
                    e.getSelectionManager()->selectPart(spt);
                }
            }
            e.sendFullRefreshToClient();
        });
}
CLIENT_TO_SERIAL(DeactivatePart, c2s_deactivate_part, int32_t, doDeactivatePart(payload, cont));

inline void doRequestZoneDataRefresh(const engine::Engine &eng, messaging::MessageController &cont)
{
    auto lz = eng.getSelectionManager()->currentLeadZone(eng);
    if (lz.has_value())
    {
        eng.getSelectionManager()->sendDisplayDataForZonesBasedOnLead(lz->part, lz->group,
                                                                      lz->zone);
    }
}

CLIENT_TO_SERIAL(RequestZoneDataRefresh, c2s_request_zone_data_refresh, bool,
                 doRequestZoneDataRefresh(engine, cont));

} // namespace scxt::messaging::client

#endif // SHORTCIRCUIT_STRUCTURE_MESSAGES_H
