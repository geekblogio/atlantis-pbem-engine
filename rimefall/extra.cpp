#include "game.h"
#include "gamedata.h"
#include "quests.h"
#include "rimefall.h"
#include <cmath>
#include <string>
#include <iterator>
#include <memory>
#include <fstream>

//
// ---------------------------------------------------------------------------------------------
// The start slot registry
// ---------------------------------------------------------------------------------------------
//
// There is no registry stored anywhere, and there deliberately is not one. rulesetSpecificData is
// not persisted (0011), and ARegion has no field saying "this hex is a start slot" — adding one
// would change the game.in format, a published interface. So the registry is READ BACK OUT OF THE
// WORLD every time it is needed: the gateway objects sitting in the nexus are the slots, their
// `inner` names the destination hex, and the destination's own latitude gives the band.
//
// Occupancy is derived the same way: a slot is taken while a player faction holds it. That is what
// makes a slot return to the pool when its holder dies or walks away, which is the right behaviour
// in a long game and falls out for free rather than needing to be maintained.
//

// Is this region held by a player faction? Guardsmen and monsters do not count as holding it —
// they are furniture, and the engine's own arrival cascade already prefers hexes without them.
//
// Tested through Faction::is_npc rather than against Game::guardfaction and Game::monfaction,
// because those are Game members and this has to be callable from ARegion code too.
static bool rimefall_slot_taken(ARegion *reg)
{
    for (const auto o : reg->objects) {
        for (const auto u : o->units) {
            if (u->faction && !u->faction->is_npc) return true;
        }
    }
    return false;
}

// Which player faction holds this region, if any. The first one found: a start slot with two
// player factions standing in it is already contested, and for the starting alliance the one that
// took it is the one that matters.
static Faction *rimefall_slot_holder(ARegion *reg)
{
    for (const auto o : reg->objects) {
        for (const auto u : o->units) {
            if (u->faction && !u->faction->is_npc) return u->faction;
        }
    }
    return nullptr;
}

// Every gateway object in the nexus, paired with the region it leads to.
static std::vector<std::pair<Object *, ARegion *>> rimefall_slots(ARegionList& regions)
{
    std::vector<std::pair<Object *, ARegion *>> slots;
    ARegionArray *nexus = regions.GetRegionArray(ARegionArray::LEVEL_NEXUS);
    if (!nexus) return slots;

    for (int x = 0; x < nexus->x; x++) {
        for (int y = 0; y < nexus->y; y++) {
            ARegion *reg = nexus->GetRegion(x, y);
            if (!reg) continue;
            for (const auto o : reg->objects) {
                if (o->type != O_GATEWAY) continue;
                if (o->inner < 0) continue;
                ARegion *dest = regions.GetRegion(o->inner);
                if (dest) slots.push_back({ o, dest });
            }
        }
    }
    return slots;
}

#define MINIMUM_ACTIVE_QUESTS 5
#define MAXIMUM_ACTIVE_QUESTS 20
#define QUEST_EXPLORATION_PERCENT 30
#define QUEST_SPAWN_RATE 7
#define QUEST_MAX_REWARD 3000
#define QUEST_SPAWN_CHANCE 70
#define MAX_DESTINATIONS 5

//
// ---------------------------------------------------------------------------------------------
// The two invasion fronts
// ---------------------------------------------------------------------------------------------
//

// How far south the northern front has reached, as a row on the surface. A pure function of the
// turn number, which is the only form that survives a save cycle with no accumulator available —
// and it has the side benefit of being legible: players can see winter coming and plan against it.
// The origin is the SOURCE'S OWN ROW, not row zero. MakeLand never seeds the rows beside the pole,
// so a front starting at zero spends its first twenty-odd turns crossing empty water and does
// nothing — which looked like a working grace period and was really just geography. Starting at the
// Rimewell also reads correctly: the cold comes from the source and creeps away from it.
static int rimefall_front_row(int turn, int origin_row, ARegionArray *surface)
{
    int row = origin_row + (turn * RIMEFALL_FRONT_ROWS_PER_10_TURNS) / 10;
    if (row < 0) row = 0;
    if (row > surface->y) row = surface->y;
    return row;
}

// Find a source object by the name it was given at world creation. Names are the handle because
// object numbers come from buildingseq and are not predictable, and nothing else about a source is
// distinguishable from an ordinary lair of the same type.
static Object *rimefall_find_source(ARegionList& regions, const std::string& name, ARegion **where)
{
    ARegionArray *surface = regions.GetRegionArray(1);
    if (!surface) return nullptr;
    for (int x = 0; x < surface->x; x++) {
        for (int y = 0; y < surface->y; y++) {
            ARegion *reg = surface->GetRegion(x, y);
            if (!reg) continue;
            for (const auto o : reg->objects) {
                if (o->name.rfind(name, 0) == 0) {
                    if (where) *where = reg;
                    return o;
                }
            }
        }
    }
    return nullptr;
}

// A front is defeated when a player faction holds its source — a unit standing inside the object,
// not merely in the region. Derived from the world, so no flag is stored and none can drift.
static bool rimefall_source_held(Object *source)
{
    if (!source) return false;
    for (const auto u : source->units) {
        if (u->faction && !u->faction->is_npc) return true;
    }
    return false;
}

// Player-versus-player battles fought this turn, BY EXCLUSION: one referenced by neither the
// monster faction nor the guard faction was necessarily between players.
//
// Battle does not record its sides usably, and Faction::battles also collects mere bystanders —
// battle.cpp adds a battle to every faction merely Present in the region. So a player fight that a
// wandering monster happened to be standing next to is counted as a monster fight and dropped.
//
// THE FILTER ERRS CONSERVATIVELY ON PURPOSE. It under-counts, never over-counts, and that
// direction is what stops the discord term from driving a feedback loop where a large wave
// produces many battles and therefore a larger wave (0011 section 3).
static int rimefall_pvp_battles(std::vector<Battle *>& battles, Faction *mon, Faction *guard)
{
    int count = 0;
    for (const auto b : battles) {
        bool npc_involved = false;
        if (mon && std::find(mon->battles.begin(), mon->battles.end(), b) != mon->battles.end())
            npc_involved = true;
        if (guard && std::find(guard->battles.begin(), guard->battles.end(), b) != guard->battles.end())
            npc_involved = true;
        if (!npc_involved) count++;
    }
    return count;
}

// How many start slots are still free, over the whole world. Used to decide whether a new faction
// can be admitted at all.
//
// A free function taking the region list rather than a Game method: docs/decisions/0012 permits
// exactly ONE engine hook, and adding ruleset-specific method names to the shared game.h would
// quietly spend more of that allowance than was granted.
static int rimefall_free_slot_count(ARegionList& regions)
{
    int free_slots = 0;
    for (const auto& slot : rimefall_slots(regions)) {
        if (!rimefall_slot_taken(slot.second)) free_slots++;
    }
    return free_slots;
}

// Rebuild what the nexus advertises, once per turn. Called from CheckVictory, which runs inside
// RunOrders before WriteReport, so what a player reads is the true state at that moment.
//
// A taken slot is RENAMED, never removed. These objects carry numbers from buildingseq; destroying
// and recreating them would move object numbers around in reports and in the JSON, which 0010's
// consequences single out as visible downstream and easy to get wrong. Renaming also shows players
// the world filling up rather than silently shortening the list.
//
// It also reports which slots were claimed since the last rebuild, and THAT IS THE ARRIVAL EVENT.
//
// Starting alliances have to be applied exactly once, when a faction first takes its land, and
// nothing can be stored to remember that it has been done: rulesetSpecificData is not persisted,
// and the game.in format is a published interface. But the seal is itself persisted memory. A
// gateway still reading "Gateway to …" whose land is now held can only mean its holder arrived
// since the last time this ran, and the rename immediately below makes it unrepeatable.
//
// This is why the alliance is NOT applied by testing whether a faction looks unallied. That test
// cannot tell a newcomer from someone who has renounced every alliance, so a faction that quit its
// bloc would be forced back into it next turn — and 0010 section 8 is explicit that this is a
// starting default and not a pact. Reading the event instead of the state keeps a renunciation
// permanent, which is the whole point of making the attitude a heavy one.
//
static std::vector<std::pair<Faction *, ARegion *>> rimefall_rebuild_gateways(ARegionList& regions,
    int front_row)
{
    std::vector<std::pair<Faction *, ARegion *>> arrived;

    for (const auto& slot : rimefall_slots(regions)) {
        Object *o = slot.first;
        ARegion *dest = slot.second;
        std::string band = rimefall_band_name(rimefall_band_of(dest, regions));
        std::string terrain = TerrainDefs[dest->type].name;

        bool taken = rimefall_slot_taken(dest);
        bool was_sealed = o->name.rfind("Sealed gateway to ", 0) == 0;

        if (taken && !was_sealed) {
            Faction *holder = rimefall_slot_holder(dest);
            if (holder) arrived.push_back({ holder, dest });
        }

        // Comma rather than brackets, for the reason given where these are first named in
        // rimefall/map.cpp: '(' does not survive the name filters.
        // A slot the northern front has reached stops being offered. Otherwise the game sends
        // newcomers into ground that is already lost, and registration would go on inviting people
        // to a world that is being eaten. It also makes registration close itself as the world
        // falls, which 0011's consequences ask to be deliberate rather than accidental.
        //
        // The state is carried in the NAME because ARegion::movement_forbidden_by_ruleset has no
        // way to compute it: that hook gets no Game, so it cannot read the turn number. The name
        // is persisted and the rebuild refreshes it every turn, exactly as the seal already works.
        bool overrun = !taken && dest->yloc <= front_row;

        std::string wanted = taken
            ? "Sealed gateway to " + band + ", " + terrain
            : overrun
                ? "Lost gateway to " + band + ", " + terrain
                : "Gateway to " + band + ", " + terrain;

        // set_name appends the object number, so compare on the prefix rather than the whole
        // string or every gateway would be renamed every single turn.
        if (o->name.rfind(wanted, 0) != 0) o->set_name(wanted);
    }

    return arrived;
}

//
// Neighbours begin as allies. Applied when a faction first takes its start location, to the
// holders of every other start location within RIMEFALL_ALLY_RADIUS.
//
// WRITTEN ON BOTH SIDES, DELIBERATELY. set_attitude records only one faction's view of another
// (faction.h), so a single-sided write would give one party the obligations of an alliance and the
// other none, with nothing reporting the asymmetry. That same one-directionality is what later
// lets a faction defect quietly, which 0010 section 8 wants; it must not be how the alliance
// STARTS.
//
// An existing attitude is never overwritten. A faction that has already declared something about a
// neighbour has said what it means, and a newcomer's arrival is not a reason to overrule it.
//
static void rimefall_settle_alliances(ARegionList& regions,
    const std::vector<std::pair<Faction *, ARegion *>>& arrived)
{
    if (arrived.empty()) return;

    // Everyone currently holding a start location, so a newcomer can be related to those already
    // present and they to it in the same pass — 0010 section 8 requires both directions at once.
    std::vector<std::pair<Faction *, ARegion *>> settled;
    for (const auto& slot : rimefall_slots(regions)) {
        Faction *holder = rimefall_slot_holder(slot.second);
        if (holder) settled.push_back({ holder, slot.second });
    }

    for (const auto& newcomer : arrived) {
        int made = 0;
        for (const auto& other : settled) {
            if (other.first == newcomer.first) continue;
            if (regions.GetPlanarDistance(newcomer.second, other.second, 0) > RIMEFALL_ALLY_RADIUS) continue;

            if (newcomer.first->get_attitude(other.first->num) == AttitudeType::NEUTRAL) {
                newcomer.first->set_attitude(other.first->num, AttitudeType::ALLY);
                made++;
            }
            if (other.first->get_attitude(newcomer.first->num) == AttitudeType::NEUTRAL)
                other.first->set_attitude(newcomer.first->num, AttitudeType::ALLY);
        }

        std::string band = rimefall_band_name(rimefall_band_of(newcomer.second, regions));

        // Only claim neighbours when there are some. Slots are spaced, so a faction can easily
        // arrive with nobody inside the alliance radius, and telling it otherwise would read as a
        // bug the first time it checked its attitudes and found them empty.
        if (made > 0) {
            newcomer.first->event(
                "Your neighbours in " + band + " greet you as an ally. This is a custom of the "
                "land, not a pact: it binds no one, and either side may end it with DECLARE at any "
                "time.",
                "arrival"
            );
        } else {
            newcomer.first->event(
                "You have settled " + band + " with no neighbour close enough to greet you. "
                "Nobody here is bound to you, and you are bound to nobody.",
                "arrival"
            );
        }
    }
}


int Game::SetupFaction( Faction *pFac )
{
    // Refuse the faction when the world is full, rather than stranding it in a nexus with no
    // gateway it can use. AddFaction discards a faction whose setup returns 0 and logs it
    // (game.cpp), which is the same path NewOrigins uses to close registration at its end game.
    //
    // This is what makes an external signup cap optional politeness instead of a dependency: not
    // capping registrations is handled here, correctly and visibly.
    //
    // The count is 0 only once every slot is held, so the message says the world is full rather
    // than that something broke — a game master reading the log needs to tell those apart.
    if (rimefall_free_slot_count(regions) == 0) {
        logger::write("Rimefall: every start location is held; the world is full. "
            "Faction not created.");
        return 0;
    }

    // NewOrigins closed registration a second way here, by counting empowered altars around the
    // map centre. Rimefall has no altars and closes registration by its own means instead: bands
    // the northern front has overrun stop offering slots, so the count above falls to zero on its
    // own as the world is lost (0011 section 6). One gate, and it is the one this ruleset owns.

    pFac->unclaimed = Globals->START_MONEY + TurnNumber() * 300;

    if (pFac->noStartLeader) {
        return 1;
    }

    //
    // Set up first unit.
    //
    Unit *temp2 = GetNewUnit( pFac );
    temp2->SetMen(I_LEADERS, 1);
    pFac->DiscoverItem(I_LEADERS, 0, 1);
    temp2->reveal = REVEAL_FACTION;

    //
    // Set up magic
    //
    temp2->type = U_MAGE;
    temp2->Study(S_OBSERVATION, 30);
    temp2->Study(S_FORCE, 30);
    temp2->Study(S_PATTERN, 30);
    temp2->Study(S_SPIRIT, 30);
    temp2->Study(S_GATE_LORE, 30);
    temp2->Study(S_FIRE, 30);

    // Set up health
    temp2->Study(S_COMBAT, 180);

    // Set up flags
    temp2->SetFlag(FLAG_BEHIND, 1);
    temp2->SetFlag(FLAG_NOCROSS_WATER, 0);
    temp2->SetFlag(FLAG_HOLDING, 0);
    temp2->SetFlag(FLAG_NOAID, 0);

    if (Globals->UPKEEP_MINIMUM_FOOD > 0)
    {
        if (!(ItemDefs[I_FOOD].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_FOOD, 6);
            pFac->DiscoverItem(I_FOOD, 0, 1);
        } else if (!(ItemDefs[I_FISH].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_FISH, 6);
            pFac->DiscoverItem(I_FISH, 0, 1);
        } else if (!(ItemDefs[I_LIVESTOCK].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_LIVESTOCK, 6);
            pFac->DiscoverItem(I_LIVESTOCK, 0, 1);
        } else if (!(ItemDefs[I_GRAIN].flags & ItemType::DISABLED)) {
            temp2->items.SetNum(I_GRAIN, 2);
            pFac->DiscoverItem(I_GRAIN, 0, 1);
        }
        temp2->items.SetNum(I_SILVER, 10);
    }

    ARegion *reg = NULL;
    if (pFac->pStartLoc) {
        reg = pFac->pStartLoc;
    } else if (!Globals->MULTI_HEX_NEXUS) {
        reg = regions.front();
    } else {
        ARegionArray *pArr = regions.GetRegionArray(ARegionArray::LEVEL_NEXUS);
        while(!reg) {
            // Sequenced deliberately, and y is drawn BEFORE x. As two arguments of one call the
            // evaluation order is unspecified, and GCC differs by architecture: x86-64 evaluates
            // right to left, aarch64 left to right, so one seed gave two different worlds. x86-64
            // is what the servers run and what every recorded seed reproduces, so its order is the
            // one pinned here. Do not swap these two lines, and do not fold them back into the call.
            const int ry = rng::get_random(pArr->y);
            const int rx = rng::get_random(pArr->x);
            reg = pArr->GetRegion(rx, ry);
        }
    }
    temp2->MoveUnit(reg->GetDummy());

    if (Globals->LAIR_MONSTERS_EXIST || Globals->WANDERING_MONSTERS_EXIST) {
        // Try to auto-declare all player factions unfriendly
        // to Creatures, since all they do is attack you.
        pFac->set_attitude(monfaction, AttitudeType::UNFRIENDLY);
    }

    return( 1 );
}

static void CreateQuest(ARegionList& regions, int monfaction)
{
    int d, count, temple, i, j, clash, reward_count;
    ARegion *r;
    std::string rname;
    std::map <std::string, int> temples;
    std::map <std::string, int>::iterator it;
    std::string stlstr;
    int destprobs[MAX_DESTINATIONS] = { 0, 0, 80, 20, 0 };
    int destinations[MAX_DESTINATIONS];
    std::string destnames[MAX_DESTINATIONS];
    std::set<std::string> intersection;

    std::shared_ptr<Quest> q = std::make_shared<Quest>();
    q->type = -1;

    // Set up quest rewards
    count = 0;
    for (i=0; i<NITEMS; i++) {
        if (
                ((ItemDefs[i].type & IT_ADVANCED) || (ItemDefs[i].type & IT_MAGIC)) &&
                ItemDefs[i].baseprice <= QUEST_MAX_REWARD &&
                !(ItemDefs[i].type & IT_SPECIAL) &&
                !(ItemDefs[i].type & IT_SHIP) &&
                !(ItemDefs[i].type & IT_NEVER_SPOIL) &&
                !(ItemDefs[i].flags & ItemType::DISABLED)) {
            count ++;
        }
    }

    // No items? Are we playing a game without items?
    if (count == 0) return;

    count = rng::get_random(count) + 1;

    for (i=0; i<NITEMS; i++) {
        if (
                ((ItemDefs[i].type & IT_ADVANCED) || (ItemDefs[i].type & IT_MAGIC)) &&
                ItemDefs[i].baseprice <= QUEST_MAX_REWARD &&
                !(ItemDefs[i].type & IT_SPECIAL) &&
                !(ItemDefs[i].type & IT_SHIP) &&
                !(ItemDefs[i].type & IT_NEVER_SPOIL) &&
                !(ItemDefs[i].flags & ItemType::DISABLED)) {
            count--;
            if (count == 0) {
                // Quest reward is based on QUEST_MAX_REWARD silver
                reward_count = (QUEST_MAX_REWARD + rng::get_random(QUEST_MAX_REWARD / 2)) / ItemDefs[i].baseprice;

                printf("\nQuest reward: %s x %d.\n", ItemDefs[i].name.c_str(), reward_count);

                // Setup reward
                Item item;
                item.type = i;
                item.num = reward_count;

                q->rewards.push_back(item);
                break;
            }
        }
    }

    d = rng::get_random(100);
    if (d < 60) {
        // SLAY quest
        q->type = Quest::SLAY;
        count = 0;
        // Count our current monsters
        for(const auto r : regions) {
            if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
            // No need to check if quests do not require exploration
            if (!r->visited && QUEST_EXPLORATION_PERCENT != 0) continue;
            for(const auto o : r->objects) {
                for(const auto u : o->units) {
                    if (u->faction->num == monfaction) count++;
                }
            }
        }
        if (!count) return;
        // pick one as the object of the quest
        d = rng::get_random(count);
        for(const auto r : regions) {
            if (TerrainDefs[r->type].similar_type == R_OCEAN) continue;
            // No need to check if quests do not require exploration
            if (!r->visited && QUEST_EXPLORATION_PERCENT != 0) continue;
            for(const auto o : r->objects) {
                for(const auto u : o->units) {
                    if (u->faction->num == monfaction) {
                        if (!d--) q->target = u->num;
                    }
                }
            }
        }
        for(const auto& q2 : quests) {
            if (q2->type == Quest::SLAY && q2->target == q->target) {
                // Don't hunt the same monster twice
                q->type = -1;
                break;
            }
        }
    } else if (d < 80) {
        // Create a HARVEST quest
        count = 0;
        for(const auto r : regions) {
            // Do allow lakes though
            if (r->type == R_OCEAN)
                continue;
            // No need to check if quests do not require exploration
            if (!r->visited && QUEST_EXPLORATION_PERCENT != 0)
                continue;
            for (const auto& p : r->products) {
                if (p->itemtype != I_SILVER)
                    count++;
            }
        }
        count = rng::get_random(count);
        for(const auto r : regions) {
            // Do allow lakes though
            if (r->type == R_OCEAN)
                continue;
            // No need to check if quests do not require exploration
            if (!r->visited && QUEST_EXPLORATION_PERCENT != 0)
                continue;
            for (const auto& p : r->products) {
                if (p->itemtype != I_SILVER) {
                    if (!count--) {
                        q->type = Quest::HARVEST;
                        q->regionnum = r->num;
                        q->objective.type = p->itemtype;
                        q->objective.num = 1;
                    }
                }
            }
        }
        r = regions.GetRegion(q->regionnum);
        rname = r->name;
        for(const auto& q2: quests) {
            if (q2->type == Quest::HARVEST) {
                r = regions.GetRegion(q2->regionnum);
                if (rname == r->name) {
                    // Don't have 2 harvest quests
                    // active in the same region
                    q->type = -1;
                }
            }
        }
    } else if (d < 100) {
        // Create a BUILD or VISIT quest
        // Find all our current temples
        temple = O_TEMPLE;
        for(const auto r : regions) {
            // No need to check if quests do not require exploration
            if (r->Population() > 0 && (r->visited || QUEST_EXPLORATION_PERCENT == 0)) {
                stlstr = r->name;
                // This looks like a null operation, but
                // actually forces the map<> element creation
                temples[stlstr];
                for(const auto o : r->objects) {
                    if (o->type == temple) {
                        temples[stlstr]++;
                    }
                }
            }
        }
        // Work out how many destnations to use, based on destprobs[]
        for (i = 0, count = 0; i < MAX_DESTINATIONS; i++)
            count += destprobs[i];
        d = rng::get_random(count);
        for (count = 0; d >= destprobs[count]; count++)
            d -= destprobs[count];
        count++;
        if (count > (int) temples.size()) {
            q->type = -1;
            count = -1;
        }
        // Choose that many unique regions
        for (i = 0; i < count; i++) {
            do {
                destinations[i] = rng::get_random(temples.size());
                // give a slight preference to regions with temples
                for (it = temples.begin(), j = 0;
                        j < destinations[i];
                        it++, j++)
                // ...by rerolling (only once) if we get a
                // templeless region first time
                if (!it->second)
                    destinations[i] = rng::get_random(temples.size());
                // make sure we haven't chosen duplicates
                clash = 0;
                for (j = 0; j < i; j++)
                    if (destinations[i] == destinations[j])
                        clash = 1;
            } while (clash);
        }
        // Look up the names of the chosen regions
        for (it = temples.begin(); it != temples.end(); it++) {
            for (i = 0; i < count; i++) {
                if (!destinations[i]--) {
                    destnames[i] = it->first;
                }
            }
        }
        // If any of them don't have a temple, then make a quest to
        // build a temple there
        for (i = 0; i < count; i++) {
            if (!temples[destnames[i]]) {
                q->type = Quest::BUILD;
                q->building = temple;
                q->regionname = destnames[i].c_str();
                break;
            }
        }
        if (i == count) {
            // They all had temples, so make a VISIT quest
            q->type = Quest::VISIT;
            q->building = temple;
            for (j = 0; j < count; j++) {
                q->destinations.insert(destnames[j]);
            }
        }
        if (q->type == Quest::BUILD) {
            for(const auto& q2: quests) {
                if (q2->type == Quest::BUILD && q->building == q2->building && q->regionname == q2->regionname) {
                    // Don't have 2 build quests
                    // active in the same region
                    q->type = -1;
                }
            }
        } else if (q->type == Quest::VISIT) {
            // Make sure that a given region is only in one
            // pilgrimage at a time
            for(const auto& q2: quests) {
                if (q2->type == Quest::VISIT && q->building == q2->building) {
                    intersection.clear();
                    std::set_intersection(
                        q->destinations.begin(),
                        q->destinations.end(),
                        q2->destinations.begin(),
                        q2->destinations.end(),
                        std::inserter(intersection, intersection.begin()),
                        std::less<std::string>()
                    );
                    if (intersection.size() > 0)
                        q->type = -1;
                }
            }
        }
    }
    if (q->type != -1)
        quests.push_back(q);
}

// NewOrigins' four endgame helpers stood here -- counting empowered altars, empowering one at
// random, and reporting entities and anomalies. They went with the ending they served; rimefall is
// won by taking both sources and then being elected. See CheckVictory below and 0011 section 6.

//
// ---------------------------------------------------------------------------------------------
// Per-turn telemetry
// ---------------------------------------------------------------------------------------------
//
// Everything a game master or a simulation needs to tune this ruleset, as numbers, written to
// `rimefall.json` beside the reports. The same figures reach a human through the engine log and
// the times articles, but prose is not a series: the three threat terms in particular have to be
// separable if anyone is to know WHICH of them is driving the front, and no player report shows
// them at all.
//
// WRITTEN BY THE RULESET, NOT THE ENGINE, and into a file of its own rather than into
// report.<n>.json. The engine's own report is read by other projects, and 0012 permits exactly one
// engine hook for one purpose and says plainly that it is not a general licence. A ruleset writing
// a ruleset's file needs neither.
//
// WHY A DESTRUCTOR. Once the ballot is open CheckVictory leaves through six different returns, and
// the file has to be written on every one of them. A call before each return is a list that will
// be wrong the first time somebody adds a seventh.
//
namespace {

struct rimefall_telemetry {
    json data = json::object();

    ~rimefall_telemetry() {
        std::ofstream f("rimefall.json", std::ios::out | std::ios::trunc);
        if (f.is_open()) f << data.dump(2) << '\n';
    }
};

} // namespace

Faction *Game::CheckVictory()
{
    int visited, unvisited;
    int d, i, count;
    int dir;
    unsigned ucount;
    ARegion *r, *start;
    Object *o;
    Location *l;
    std::string message, times, temp;
    std::map <std::string, int> vRegions, uvRegions;
    std::map <std::string, int>::iterator it;
    std::string stlstr;
    std::set<std::string> intersection, un;
    std::set<std::string>::iterator it2;
    Faction *winner = nullptr;

    // Rimefall's per-turn work hangs here because CheckVictory is the only per-turn ruleset hook
    // the engine offers, and runorders.cpp calls it only while OPEN_ENDED is 0 — which is why
    // rimefall/rules.cpp pins that field with a comment. It runs inside RunOrders before
    // WriteReport (game.cpp), so a player's report shows the true state of the nexus.
    //
    // How far south the northern front has reached, computed once and used by both the gateway
    // rebuild and the fronts below. It has to be worked out here rather than inside either,
    // because TurnNumber() is a Game method and neither of them is one.
    //
    ARegion *front_origin = nullptr;
    rimefall_find_source(regions, RIMEFALL_NORTH_SOURCE_NAME, &front_origin);
    ARegionArray *front_surface = regions.GetRegionArray(1);
    int front_row = front_surface
        ? rimefall_front_row(TurnNumber(), front_origin ? front_origin->yloc : 0, front_surface)
        : 0;

    rimefall_telemetry telemetry;
    telemetry.data["turn"] = TurnNumber();
    telemetry.data["front"] = { {"row", front_row}, {"ran", false} };

    // The rebuild reports who claimed a start location since it last ran, and the alliances are
    // applied to exactly those. Order matters: the rebuild reads the old seals to find the event
    // and then writes the new ones, so this must happen before anything else touches them.
    rimefall_settle_alliances(regions, rimefall_rebuild_gateways(regions, front_row));

    //
    // Both fronts, run once a turn.
    //
    // THIS IS SPELLED OUT HERE RATHER THAN IN ITS OWN FUNCTION. Everything it touches — MakeLMon,
    // write_times_article, factions, battles, monfaction — is private to Game, and Game's only
    // ruleset-defined members are the four hooks in game.h. Declaring one more would be a third
    // engine change, which docs/decisions/0012 does not allow without its own record. Kept as a
    // clearly bounded section of CheckVictory instead.
    //
    // POSITION AND STRENGTH ARE SEPARATE MECHANISMS (0011 section 2). The spawn band's position is a
    // pure function of the turn and nothing players do slows it; the threat score decides only whether
    // it attacks this turn and how hard. Killing the source is the one thing that stops a front, and
    // it stops it completely.
    //
    // Scoped so an early exit skips the fronts without abandoning the rest of CheckVictory.
    do {
        ARegionArray *surface = front_surface;
        if (!surface) break;

        ARegion *north_where = front_origin, *east_where = nullptr;
        Object *north = rimefall_find_source(regions, RIMEFALL_NORTH_SOURCE_NAME, &north_where);
        Object *east  = rimefall_find_source(regions, RIMEFALL_EAST_SOURCE_NAME,  &east_where);

        bool north_dead = rimefall_source_held(north);
        bool east_dead  = rimefall_source_held(east);

        int turn = TurnNumber();

        Faction *monfac = GetFaction(factions, monfaction);
        Faction *guardfac = GetFaction(factions, guardfaction);
        if (!monfac) break;

        // Garrison a source that is standing empty and unclaimed. This is where the guard is placed
        // at all, because CreateWorld runs before the monster faction exists.
        //
        // NOT REFILLED WHILE A PLAYER IS IN THE REGION. Entry to an occupied object is forbidden
        // while its monsters hold it, so taking a source means clearing it one turn and entering
        // the next. Refilling on the turn it was cleared closed that window and made both sources
        // permanently unwinnable — which the first attempt did, and which is exactly the sort of
        // thing that only shows up by playing it out.
        //
        // Refilling once the attacker has gone is still right: it means a front must be taken and
        // KEPT rather than merely raided.
        struct { Object *obj; ARegion *where; } garrisons[] = { { north, north_where }, { east, east_where } };
        for (const auto& g : garrisons) {
            if (!g.obj || !g.where) continue;
            if (!g.obj->units.empty()) continue;
            if (rimefall_slot_taken(g.where)) continue;   // someone is standing over it
            MakeLMon(g.obj);
        }

        // The threat score, recomputed from scratch, never accumulated.
        int reach_top = front_row - RIMEFALL_FRONT_BAND_DEPTH;
        long population = 0;
        for (int x = 0; x < surface->x; x++) {
            for (int y = 0; y < surface->y; y++) {
                ARegion *reg = surface->GetRegion(x, y);
                if (!reg) continue;
                if (reg->yloc > front_row) continue;          // ahead of the front: out of reach
                if (reg->yloc < reach_top - 6) continue;      // long behind it: already consumed
                population += reg->population;
            }
        }

        int discord = rimefall_pvp_battles(battles, monfac, guardfac);
        int score = turn * RIMEFALL_THREAT_PER_TURN
                  + (int)(population / 10000) * RIMEFALL_THREAT_PER_10KPOP
                  + discord * RIMEFALL_THREAT_PER_BATTLE;

        telemetry.data["front"]["ran"] = true;
        telemetry.data["front"]["population_in_reach"] = population;
        telemetry.data["front"]["battles"] = discord;
        telemetry.data["front"]["threat"] = {
            {"total", score},
            {"time", turn * RIMEFALL_THREAT_PER_TURN},
            {"prosperity", (int)(population / 10000) * RIMEFALL_THREAT_PER_10KPOP},
            {"discord", discord * RIMEFALL_THREAT_PER_BATTLE},
            {"threshold", RIMEFALL_THREAT_THRESHOLD}
        };
        telemetry.data["sources"] = {
            {"north", {{"name", RIMEFALL_NORTH_SOURCE_NAME}, {"held", north_dead}}},
            {"east",  {{"name", RIMEFALL_EAST_SOURCE_NAME},  {"held", east_dead}}}
        };

        // One line per turn in the engine log, not in any player's report. A game master tuning a
        // live game needs to see which term is driving the front, and the three are impossible to
        // separate from the outside.
        logger::write(
            "Rimefall front: turn " + std::to_string(turn) + " row " + std::to_string(front_row) +
            " | threat " + std::to_string(score) +
            " = time " + std::to_string(turn * RIMEFALL_THREAT_PER_TURN) +
            " + prosperity " + std::to_string((int)(population / 10000) * RIMEFALL_THREAT_PER_10KPOP) +
            " + discord " + std::to_string(discord * RIMEFALL_THREAT_PER_BATTLE)
        );

        int stacks = 0;
        if (score >= RIMEFALL_THREAT_THRESHOLD) {
            stacks = 1 + (score - RIMEFALL_THREAT_THRESHOLD) / RIMEFALL_THREAT_PER_EXTRA_STACK;
            if (stacks > RIMEFALL_WAVE_MAX_STACKS) stacks = RIMEFALL_WAVE_MAX_STACKS;
        }

        telemetry.data["front"]["stacks_allowed"] = stacks;
        telemetry.data["front"]["stacks_placed"] = { {"north", 0}, {"east", 0} };

        // The northern front: a slow, broad wall of attrition across the full width of its band.
        if (!north_dead && stacks > 0) {
            static const int northern[] = { I_IWURM, I_ICEDRAGON, I_SKELETON, I_UNDEAD, I_LICH };
            int placed = 0;
            for (int attempt = 0; attempt < stacks * 20 && placed < stacks; attempt++) {
                int y = reach_top + rng::get_random(RIMEFALL_FRONT_BAND_DEPTH + 1);
                if (y < 0 || y >= surface->y) continue;
                int x = rng::get_random(surface->x);
                ARegion *reg = surface->GetRegion(x, y);
                if (!reg) continue;
                if (TerrainDefs[reg->type].similar_type == R_OCEAN) continue;

                int type = northern[rng::get_random(sizeof(northern) / sizeof(northern[0]))];
                auto monster = find_monster(ItemDefs[type].abr, (ItemDefs[type].type & IT_ILLUSION));
                if (!monster) continue;
                Unit *mon = GetNewUnit(monfac, 0);
                mon->MakeWMon(monster->get().name.c_str(), type,
                    (monster->get().number + rng::get_random(monster->get().number) + 1) / 2);
                mon->MoveUnit(reg->GetDummy());
                placed++;
            }
            telemetry.data["front"]["stacks_placed"]["north"] = placed;
            if (placed) {
                write_times_article(
                    "The cold deepens in the north. " + std::to_string(placed) +
                    " more of the rime-borne have been seen abroad, further south than last season."
                );
            }
        }

        // The eastern front wakes when the northern band has passed a set depth, OR immediately when
        // the northern source is taken — whichever comes first. Without the second condition, killing
        // the north early would stop the front before it reached the wake depth and the dragons would
        // never come, letting a fast group skip half the game. With it, success in the north brings
        // the second act forward instead of cancelling it (0011 section 4).
        bool dragons_awake = north_dead ||
            (front_row * 10 >= surface->y * RIMEFALL_DRAGON_WAKE_TENTHS);
        telemetry.data["front"]["dragons_awake"] = dragons_awake;

        if (!east_dead && dragons_awake && stacks > 0) {
            int want = stacks > RIMEFALL_DRAGON_STACKS_MAX ? RIMEFALL_DRAGON_STACKS_MAX : stacks;
            int placed = 0;
            for (int attempt = 0; attempt < want * 40 && placed < want; attempt++) {
                // Southern half only, and the coast it strikes from is found relative to the
                // continent, never from x near the array width — there is no eastern edge.
                int y = surface->y / 2 + rng::get_random(surface->y / 2);
                int offset = rng::get_random(surface->x / 2);
                ARegion *reg = surface->GetRegion((surface->x / 2 + offset) % surface->x, y);
                if (!reg) continue;
                if (TerrainDefs[reg->type].similar_type == R_OCEAN) continue;

                auto monster = find_monster(ItemDefs[I_DRAGON].abr, (ItemDefs[I_DRAGON].type & IT_ILLUSION));
                if (!monster) continue;
                Unit *mon = GetNewUnit(monfac, 0);
                mon->MakeWMon(monster->get().name.c_str(), I_DRAGON,
                    (monster->get().number + rng::get_random(monster->get().number) + 1) / 2);
                mon->MoveUnit(reg->GetDummy());
                placed++;
            }
            telemetry.data["front"]["stacks_placed"]["east"] = placed;
            if (placed) {
                write_times_article(
                    "Wings out of the eastern sea. " + std::to_string(placed) +
                    " saltdrakes have come inland against the south."
                );
            }
        }

        if (north_dead && east_dead) {
            write_times_article("Both hordes are ended. The land is quiet, and has no king.");
        }
    } while (false);

    for(const auto& q: quests) {
        if (q->type != Quest::VISIT) continue;
        for (auto dest: q->destinations) {
            un.insert(dest);
        }
    }

    visited = 0;
    unvisited = 0;
    for(const auto r : regions) {
        if (r->Population() > 0) {
            stlstr = r->name;
            if (r->visited) {
                visited++;
                vRegions[stlstr]++;
            } else {
                unvisited++;
                uvRegions[stlstr]++;
            }
        }
        for(const auto o : r->objects) {
            for(const auto u : o->units) {
                intersection.clear();
                std::set_intersection(
                    u->visited.begin(), u->visited.end(),
                    un.begin(), un.end(),
                    std::inserter(intersection, intersection.begin()),
                    std::less<std::string>()
                );
                u->visited = intersection;
            }
        }
    }

    printf("Players have visited %d regions; %d unvisited.\n", visited, unvisited);

    if (visited >= (unvisited + visited) * QUEST_EXPLORATION_PERCENT / 100) {
        // Exploration phase complete: start creating relic quests
        for (i = 0; i < QUEST_SPAWN_RATE; i++) {
            if (quests.size() < MAXIMUM_ACTIVE_QUESTS && rng::get_random(100) < QUEST_SPAWN_CHANCE)
                CreateQuest(regions, monfaction);
        }
        while (quests.size() < MINIMUM_ACTIVE_QUESTS) {
            CreateQuest(regions, monfaction);
        }
    }

    if (unvisited) {
        // Tell the players to get exploring :-)
        if (visited > 9 * unvisited) {
            // 90% explored; specific hints
            d = rng::get_random(12);
        } else if (visited > 3 * unvisited) {
            // 75% explored; some general hints
            d = rng::get_random(8);
        } else {
            // lots of unexplored area; just tell them to explore
            d = rng::get_random(6);
        }
        if (d == 2) {
            message = "Be productive and strong; explore new land and find a way to survive.";
            write_times_article(message);
        } else if (d == 3) {
            message = "Go into all the world, and tell all people that new world is great.";
            write_times_article(message);
        } else if (d == 4 || d == 5) {
            message = "Players have visited " + std::to_string(visited * 100 / (visited + unvisited)) +
                "% of all inhabited regions.";
            write_times_article(message);
        } else if (d == 6) {
            // report an incompletely explored region
            count = 0;
            // see how many incompletely explored regions we have
            for (it = vRegions.begin(); it != vRegions.end(); it++) {
                if (uvRegions[it->first] > 0)
                    count++;
            }
            if (count > 0) {
                // choose one, and find it
                count = rng::get_random(count);
                for (it = vRegions.begin(); it != vRegions.end(); it++) {
                    if (uvRegions[it->first] > 0)
                        if (!count--)
                            break;
                }
                // pick a hex within that region, and find it
                count = rng::get_random(it->second);
                for(const auto r : regions) {
                    if (it->first == r->name) {
                        if (!count--) {
                            // report this hex
                            message = "The " + TerrainDefs[TerrainDefs[r->type].similar_type].name + " of " + r->name +
                                (TerrainDefs[r->type].similar_type == R_TUNNELS ? " are" : " is") +
                                " only partly explored.";
                            write_times_article(message);
                        }
                    }
                }
            }
        } else if (d == 7) {
            // report a completely unknown region
            count = 0;
            // see how many completely unexplored regions we have
            for (it = uvRegions.begin(); it != uvRegions.end(); it++) {
                if (vRegions[it->first] == 0)
                    count++;
            }
            if (count > 0) {
                // choose one, and find it
                count = rng::get_random(count);
                for (it = uvRegions.begin(); it != uvRegions.end(); it++) {
                    if (vRegions[it->first] == 0) {
                        if (!count--)
                            break;
                    }
                }
                // pick a hex within that region, and find it
                count = rng::get_random(it->second);
                for(const auto r : regions) {
                    if (it->first == r->name) {
                        if (!count--) {
                            // report this hex
                            dir = -1;
                            start = regions.FindNearestStartingCity(r, &dir);
                            message = "The " + TerrainDefs[TerrainDefs[r->type].similar_type].name + " of " + r->name;
                            if (start == r) {
                                message += ", containing " + start->town->name + ",";
                            } else if (start && dir != -1) {
                                message += ", ";
                                if (r->zloc != start->zloc && dir != MOVE_IN)
                                    message += "through a shaft ";
                                switch (dir) {
                                    case D_NORTH:
                                    case D_NORTHWEST:
                                        message += "north of";
                                        break;
                                    case D_NORTHEAST:
                                        message += "east of";
                                        break;
                                    case D_SOUTH:
                                    case D_SOUTHEAST:
                                        message += "south of";
                                        break;
                                    case D_SOUTHWEST:
                                        message += "west of";
                                        break;
                                    case MOVE_IN:
                                        message += "through a shaft in";
                                        break;
                                }
                                message += " " + start->town->name + ",";
                            }
                            message += (TerrainDefs[r->type].similar_type == R_TUNNELS ? " have" : " has");
                            message += " yet to be visited by exiles from destroyed worlds.";
                            write_times_article(message);
                        }
                    }
                }
            }
        } else if (d > 7) {
            // report exact coords of an unexplored hex
            count = rng::get_random(unvisited);
            for(const auto r : regions) {
                if (r->Population() > 0 && !r->visited) {
                    if (!count--) {
                        message = "The people of the " + r->short_print();
                        switch (rng::get_random(4)) {
                            case 0:
                                message += " have not been visited by exiles.";
                                break;
                            case 1:
                                message += " are still in need of your guidance.";
                                break;
                            case 2:
                                message += " have not yet been graced by your presence.";
                                break;
                            case 3:
                                message += " are still in need of your guidance.";
                                break;
                        }
                        write_times_article(message);
                    }
                }
            }
        }
    }

    std::vector<std::shared_ptr<Quest>> questsWithProblems;
    for(const auto& q: quests) {
        switch(q->type) {
            case Quest::SLAY:
                l = regions.FindUnit(q->target);
                if (!l || l->unit->faction->num != monfaction) {
                    // Something has gone wrong with this quest!
                    // shouldn't ever happen, but...
                    questsWithProblems.push_back(q);
                    if (l) delete l;
                } else {
                    message = "Quest: In the ";
                    message += TerrainDefs[TerrainDefs[l->region->type].similar_type].name;
                    message += " of ";
                    message += l->region->name;
                    if (l->obj->type == O_DUMMY)
                        message += " roams";
                    else
                        message += " lurks";
                    message += " the ";
                    message += l->unit->name;
                    message += ".  Free the world from this menace and be rewarded!";
                    write_times_article(message);
                    delete l;
                }

                break;
            case Quest::HARVEST:
                r = regions.GetRegion(q->regionnum);
                message = "Quest: Seek a token of the Ancient Ones legacy amongst the ";
                message += ItemDefs[q->objective.type].names;
                message += " of ";
                message += r->name;
                message += ".";
                write_times_article(message);
                break;
            case Quest::BUILD:
                message = "Quest: Build a ";
                message += ObjectDefs[q->building].name;
                message += " in ";
                message += q->regionname;
                message += " for the glory of the Gods.";
                write_times_article(message);
                break;
            case Quest::VISIT:
                message = "Quest: Show your devotion by visiting ";
                message += ObjectDefs[q->building].name;
                message += "s in ";
                ucount = 0;
                for (it2 = q->destinations.begin();
                    it2 != q->destinations.end();
                    it2++) {
                    ucount++;
                    if (ucount == q->destinations.size()) {
                        message += " and ";
                    } else if (ucount > 1) {
                        message += ", ";
                    }
                    message += it2->c_str();
                }
                message += ".";
                write_times_article(message);
                break;
            case Quest::DEMOLISH:
                r = regions.GetRegion(q->regionnum);
                if (r)
                    o = r->GetObject(q->target);
                else
                    o = 0;
                if (!r || !o) {
                    // Something has gone wrong with this quest!
                    // shouldn't ever happen, but...
                    questsWithProblems.push_back(q);
                } else {
                    message = "Quest: Tear down the blasphemous ";
                    message += o->name;
                    message += " : ";
                    message += ObjectDefs[o->type].name;
                    message += " in ";
                    message += r->name;
                    message += "!";
                    write_times_article(message);
                }
                break;
            default:
                break;
        }
    }
    for(const auto& q: questsWithProblems) quests.erase(q);

    //
    // THE ELECTION (0011 section 6).
    //
    // Two gates, and the order is the design. WHILE EITHER SOURCE STANDS THERE IS NO WINNER AT
    // ALL -- not a provisional one, not a leading one. Only once both are held does the ballot
    // open, and then the crown goes to whoever holds mutual ALLY from at least
    // RIMEFALL_ELECTION_PERCENT of the other living factions.
    //
    // The losing condition needs no code here: if the fronts take the continent the factions die
    // out and the engine ends the game on !livingFacs by itself (game.cpp).
    //
    // Replaces NewOrigins' annihilation ending, which this ruleset inherited wholesale from the
    // stage 1 copy and which was live rather than dormant. Its objects are no longer enabled
    // either -- see ModifyTablesPerRuleset.
    {
        ARegion *north_at = nullptr, *east_at = nullptr;
        Object *north_source = rimefall_find_source(regions, RIMEFALL_NORTH_SOURCE_NAME, &north_at);
        Object *east_source  = rimefall_find_source(regions, RIMEFALL_EAST_SOURCE_NAME,  &east_at);

        // A source that cannot be found counts as STANDING, not as taken. The only way one goes
        // missing is a world this ruleset did not build, and crowning someone because a lookup
        // failed would be a worse outcome than never crowning anyone.
        bool north_taken = rimefall_source_held(north_source);
        bool east_taken  = rimefall_source_held(east_source);

        if (!north_taken || !east_taken) {
            std::string standing;
            if (!north_taken && !east_taken) {
                standing = "Both " + std::string(RIMEFALL_NORTH_SOURCE_NAME) + " and " +
                    RIMEFALL_EAST_SOURCE_NAME + " still stand.";
            } else if (!north_taken) {
                standing = std::string(RIMEFALL_NORTH_SOURCE_NAME) + " still stands.";
            } else {
                standing = std::string(RIMEFALL_EAST_SOURCE_NAME) + " still stands.";
            }
            write_times_article("The Crown: " + standing + " No throne can be claimed while a "
                "horde still has a home.");
            telemetry.data["election"] = { {"open", false} };
            return nullptr;
        }

        // Both sources are held: the ballot is open. Every faction is a candidate -- there is no
        // office to hold first, unlike NewOrigins' monolith owner.
        Faction *best = nullptr;
        int best_allies = -1;
        bool tied = false;
        int living = 0;
        for (const auto f : factions) {
            if (f->is_npc) continue;
            living++;
        }

        for (const auto candidate : factions) {
            if (candidate->is_npc) continue;

            int allies = 0;
            for (const auto f : factions) {
                if (f->is_npc || f == candidate) continue;
                // MUTUAL, both ways. A one-sided ALLY is a gift, not a vote.
                if (f->get_attitude(candidate->num) != AttitudeType::ALLY) continue;
                if (candidate->get_attitude(f->num) != AttitudeType::ALLY) continue;
                allies++;
            }

            if (allies > best_allies) {
                best_allies = allies;
                best = candidate;
                tied = false;
            } else if (allies == best_allies) {
                tied = true;
            }
        }

        if (!best) {
            telemetry.data["election"] = { {"open", true}, {"candidates", 0} };
            return nullptr;
        }

        // The last faction standing needs no electorate. Anything else would leave a game that
        // cannot end.
        int electorate = living - 1;
        if (electorate <= 0) {
            write_times_article("The Crown: " + best->name + " stands alone over a broken world, "
                "and is crowned by default.");
            telemetry.data["election"] = {
                {"open", true}, {"candidates", living}, {"electorate", 0},
                {"leader", best->name}, {"allies", best_allies}, {"crowned", true},
                {"reason", "alone"}
            };
            return best;
        }

        int percent = (best_allies * 100) / electorate;
        write_times_article("The Crown: both sources have fallen and the throne is open. " +
            best->name + " leads with " + std::to_string(best_allies) + " of " +
            std::to_string(electorate) + " factions in mutual alliance (" +
            std::to_string(percent) + "%, " + std::to_string(RIMEFALL_ELECTION_PERCENT) +
            "% needed).");

        telemetry.data["election"] = {
            {"open", true}, {"candidates", living}, {"electorate", electorate},
            {"leader", best->name}, {"allies", best_allies}, {"percent", percent},
            {"needed", RIMEFALL_ELECTION_PERCENT}, {"tied", tied},
            {"crowned", percent >= RIMEFALL_ELECTION_PERCENT && !tied}
        };

        if (percent < RIMEFALL_ELECTION_PERCENT) return nullptr;

        // A DEAD HEAT CROWNS NOBODY. Two candidates on the same count is a hung throne, and the
        // game continues until one of them wins somebody over. Breaking the tie by faction number
        // would decide the game on registration order, which is not a thing players can play.
        if (tied) {
            write_times_article("The Crown: the vote is tied, and a tied throne stays empty.");
            return nullptr;
        }

        return best;
    }

    return winner;
}

void Game::ModifyTablesPerRuleset(void)
{
    if (Globals->APPRENTICES_EXIST)
        EnableSkill(S_MANIPULATE);

    if (!Globals->GATES_EXIST)
        DisableSkill(S_GATE_LORE);

    if (Globals->FULL_TRUESEEING_BONUS) {
        ModifyAttribMod("observation", 1, AttribModItem::SKILL, "TRUE", AttribModItem::UNIT_LEVEL, 1);
    }
    if (Globals->IMPROVED_AMTS) {
        ModifyAttribMod("observation", 2, AttribModItem::ITEM, "AMTS", AttribModItem::CONSTANT, 3);
    }
    if (Globals->FULL_INVIS_ON_SELF) {
        ModifyAttribMod("stealth", 3, AttribModItem::SKILL, "INVI", AttribModItem::UNIT_LEVEL, 1);
    }

    if (Globals->NEXUS_IS_CITY && Globals->TOWNS_EXIST) {
        ClearTerrainRaces(R_NEXUS);
        ModifyTerrainRace(R_NEXUS, 0, I_HIGHELF);
        ModifyTerrainRace(R_NEXUS, 1, I_MAN);
        ModifyTerrainRace(R_NEXUS, 2, I_HILLDWARF);
        ClearTerrainItems(R_NEXUS);
        ModifyTerrainItems(R_NEXUS, 0, I_IRON, 100, 10);
        ModifyTerrainItems(R_NEXUS, 1, I_WOOD, 100, 10);
        ModifyTerrainItems(R_NEXUS, 2, I_STONE, 100, 10);
        ModifyTerrainEconomy(R_NEXUS, 1000, 15, 50, 2);
    }

    //
    // The two powers, renamed. Six existing gamedata.cpp entries reskinned through
    // ModifyItemName; NO ENTRY IS ADDED TO gamedata.cpp, per 0010 section 4. That table is the
    // union of every variant's needs and is shared with upstream, so adding to it would buy a
    // permanent merge conflict in exchange for flavour a rename gives for free.
    //
    // The names divide on purpose. The north takes frost words — rime, hoar, frost, thaw, winter —
    // and the east takes salt, because the eastern front strikes inland from the sea against the
    // dry half of the map. They have to read as two powers rather than one monster pool with two
    // doors, which is what 0010 section 5 keeps ice dragons off the eastern front for.
    //
    // All coined from genre vocabulary, which 0010 section 6 frees for use. No proper noun from
    // any existing work appears here, and none should be added later: "wintersmith" was rejected
    // for being someone's book title.
    //
    // BOTH TABLES HAVE TO BE RENAMED, and this is easy to get half right. ModifyItemName covers
    // ItemDefs, which is what a player sees in an item list or a battle report line. But a
    // wandering monster UNIT is named from MonDefs — Game::MakeLMon passes monster.name straight
    // to MakeWMon (npc.cpp) — so renaming only the item leaves every actual creature in the world
    // still called an ice wurm. It was written that way first, and a generated world still said
    // "Ice Wurms (158)".
    //
    // There is no modify_monster_name() among the modify_monster_* helpers, and adding one would
    // be a third engine change that 0012's boundary does not allow. None is needed: find_monster()
    // is declared in items.h and hands back a mutable reference, which is exactly what those
    // helpers use internally. MonType::name is a std::string, so there is no dangling-pointer
    // hazard of the kind intern() exists for in modify.cpp.
    //
    // Illusions are renamed alongside the real thing. An illusion of a saltdrake that announced
    // itself as a dragon would give the trick away for free.
    //
    // ONE LABEL STAYS BEYOND REACH, and it is not worth a third engine change. Game::MakeLMon
    // names a crypt's stack with the hard-coded literal "Undead" (npc.cpp) rather than reading
    // MonDefs, exactly as it does for "Demons", "Evil Mages" and "Dark Mages". So a crypt still
    // reports a unit called Undead, whose contents are frostbound, thawless and winterwrights.
    // That is a group label on ordinary lair furniture, not part of either invasion front — the
    // fronts spawn their own units from CheckVictory and are named entirely by this ruleset.
    struct rimefall_rename { int item; const char *abbr; const char *one; const char *many; };
    static const rimefall_rename renames[] = {
        { I_IWURM,     "ICEW", "rimeworm",     "rimeworms"     }, // burrows beneath the ice
        { I_ICEDRAGON, "IDRA", "hoarwyrm",     "hoarwyrms"     }, // hoar, as in hoarfrost
        { I_SKELETON,  "SKEL", "frostbound",   "frostbound"    }, // bones the ice holds together
        { I_UNDEAD,    "UNDE", "thawless",     "thawless"      }, // the dead that will not go
        { I_LICH,      "LICH", "winterwright", "winterwrights" }, // the one that makes the cold
        { I_DRAGON,    "DRAG", "saltdrake",    "saltdrakes"    }, // comes out of the eastern sea
    };

    for (const auto& r : renames) {
        ModifyItemName(r.item, r.one, r.many);
        for (int illusion = 0; illusion <= 1; illusion++) {
            auto monster = find_monster(r.abbr, illusion);
            if (monster) monster->get().name = r.one;
        }
    }

    // RIMEFALL STORES NOTHING HERE, AND THAT IS THE POINT. rulesetSpecificData is not persisted
    // (0011 section 3), so anything put in it is silently lost the moment a turn is saved and
    // reloaded — which is every turn. NewOrigins set five keys here for its annihilation ending;
    // rimefall's tuning lives in rimefall/rimefall.h as constants, and its state is read back out
    // of the persisted world. Cleared rather than filled, so nothing inherits a NewOrigins ending
    // by accident.
    rulesetSpecificData.clear();

    EnableItem(I_CAMEL);
    EnableItem(I_MCROSSBOW);
    EnableItem(I_MWAGON);
    EnableItem(I_GLIDER);
    EnableItem(I_LEATHERARMOR);
    EnableItem(I_SPEAR);
    EnableItem(I_JAVELIN);
    EnableItem(I_MSHIELD);
    EnableItem(I_ISHIELD);
    EnableItem(I_WSHIELD);
    EnableItem(I_AEGIS);
    EnableItem(I_WINDCHIME);
    EnableItem(I_GATE_CRYSTAL);
    EnableItem(I_STAFFOFH);
    EnableItem(I_SCRYINGORB);
    EnableItem(I_CORNUCOPIA);
    EnableItem(I_BOOKOFEXORCISM);
    EnableItem(I_HOLYSYMBOL);
    EnableItem(I_CENSER);
    EnableItem(I_FSWORD);
    EnableItem(I_MUSHROOM);
    EnableItem(I_HEALPOTION);
    EnableItem(I_GEMS);
    EnableItem(I_PIKE);
    EnableItem(I_BAXE);

    // Tools
    EnableItem(I_PICK);
    EnableItem(I_AXE);
    EnableItem(I_HAMMER);
    EnableItem(I_NET);
    EnableItem(I_LASSO);
    EnableItem(I_BAG);
    EnableItem(I_SPINNING);

    // FMI
    EnableItem(I_CATAPULT);
    EnableItem(I_STEEL_DEFENDER);

    //
    // Change craft: adamantium
    //
    EnableItem(I_ADMANTIUM);
    EnableItem(I_ADSWORD);
    EnableItem(I_ADRING);
    EnableItem(I_ADPLATE);
    ModifyItemProductionSkill(I_ADMANTIUM, "MINI", 5);
    ModifyItemProductionSkill(I_ADSWORD, "WEAP", 5);
    ModifyItemProductionSkill(I_ADBAXE, "WEAP", 5);
    ModifyItemProductionSkill(I_ADRING, "ARMO", 5);
    ModifyItemProductionSkill(I_ADPLATE, "ARMO", 5);

    // Artifacts of power
    DisableItem(I_RELICOFGRACE);

    // Disable items
    DisableItem(I_SUPERBOW);
    DisableItem(I_BOOTS);
    DisableItem(I_CLOTHARMOR);
    DisableItem(I_MBAXE);
    DisableItem(I_ADBAXE);
    DisableItem(I_ROUGHGEM);

    // No staff of lightning
    DisableSkill(S_CREATE_STAFF_OF_LIGHTNING);
    DisableItem(I_STAFFOFL);

    EnableSkill(S_ENCHANT_SHIELDS);
    EnableSkill(S_CREATE_AEGIS);
    EnableSkill(S_CREATE_WINDCHIME);
    EnableSkill(S_CREATE_GATE_CRYSTAL);
    EnableSkill(S_CREATE_STAFF_OF_HEALING);
    EnableSkill(S_CREATE_SCRYING_ORB);
    EnableSkill(S_CREATE_CORNUCOPIA);
    EnableSkill(S_CREATE_BOOK_OF_EXORCISM);
    EnableSkill(S_CREATE_HOLY_SYMBOL);
    EnableSkill(S_CREATE_CENSER);
    EnableSkill(S_CREATE_FLAMING_SWORD);
    EnableSkill(S_TRANSMUTATION);
    DisableSkill(S_CAMELTRAINING);
    DisableSkill(S_RANCHING);

    // No endurance
    DisableSkill(S_ENDURANCE);

    DisableSkill(S_GEMCUTTING);

    // Food
    EnableSkill(S_COOKING);
    EnableItem(I_FOOD);

    // Magic

    ModifySkillDependancy(S_RAISE_UNDEAD, 0, "SUSK", 3);
    ModifySkillDependancy(S_SUMMON_LICH, 0, "RAIS", 3);
    ModifySkillDependancy(S_DRAGON_LORE, 1, "WOLF", 3);

    ModifyItemMagicOutput(I_SKELETON, 200);
    ModifyItemMagicOutput(I_WOLF, 200);
    ModifyItemMagicOutput(I_IMP, 200);
    ModifyItemMagicOutput(I_UNDEAD, 100);
    ModifyItemMagicOutput(I_EAGLE, 100);
    ModifyItemMagicOutput(I_DEMON, 100);
    ModifyItemMagicOutput(I_DRAGON, 20);
    ModifyItemMagicOutput(I_LICH, 30);
    ModifyItemEscape(I_IMP, ItemType::ESC_LEV_LINEAR | ItemType::LOSE_LINKED, "SUIM", 20);
    ModifyItemEscape(I_DEMON, ItemType::ESC_LEV_LINEAR | ItemType::LOSE_LINKED, "SUDE", 20);
    ModifyItemEscape(I_BALROG, ItemType::ESC_LEV_LINEAR | ItemType::LOSE_LINKED, "SUBA", 20);

    //
    // Roads
    //
    EnableObject(O_ROADN);
    EnableObject(O_ROADNE);
    EnableObject(O_ROADNW);
    EnableObject(O_ROADS);
    EnableObject(O_ROADSE);
    EnableObject(O_ROADSW);
    ModifyObjectConstruction(O_ROADN, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADNE, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADNW, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADS, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADSE, I_STONE, 30, "BUIL", 2);
    ModifyObjectConstruction(O_ROADSW, I_STONE, 30, "BUIL", 2);

    EnableObject(O_TEMPLE);
    EnableObject(O_MQUARRY);
    EnableObject(O_AMINE);
    EnableObject(O_PRESERVE);
    EnableObject(O_SACGROVE);
    EnableObject(O_MTOWER);
    EnableObject(O_MFORTRESS);
    EnableObject(O_MCITADEL);
    EnableObject(O_STABLE);
    EnableObject(O_MSTABLE);
    EnableObject(O_HUT);
    EnableObject(O_TRAPPINGLODGE);
    EnableObject(O_FAERIERING);
    EnableObject(O_ALCHEMISTLAB);
    EnableObject(O_OASIS);
    EnableObject(O_TRAPPINGHUT);

    DisableObject(O_GEMAPPRAISER);
    DisableObject(O_PALACE);

    ModifyObjectName(O_MFORTRESS, "Magical Fortress");
    ModifyObjectName(O_MCASTLE, "Magical Castle");

    EnableObject(O_ISLE);
    EnableObject(O_DERELICT);
    EnableObject(O_OCAVE);
    EnableObject(O_WHIRL);

    //
    // Monsters
    //
    EnableItem(I_PIRATES);
    EnableItem(I_KRAKEN);
    EnableItem(I_MERFOLK);
    EnableItem(I_ELEMENTAL);
    EnableItem(I_HYDRA);
    EnableItem(O_BOG);
    EnableItem(I_ICEDRAGON);
    EnableItem(O_ICECAVE);
    EnableItem(I_ILLYRTHID);
    EnableItem(O_ILAIR);
    EnableItem(I_DEVIL);

    EnableItem(I_STORMGIANT);
    EnableItem(I_CLOUDGIANT);
    EnableItem(O_GIANTCASTLE);

    EnableItem(I_WARRIORS);

    EnableItem(I_DARKMAGE);
    EnableItem(O_DARKTOWER);

    EnableItem(I_MAGICIANS);
    EnableItem(O_MAGETOWER);

    //
    // Change races
    //
    DisableItem(I_ESKIMO);
    DisableItem(I_TRIBESMAN);
    DisableItem(I_NOMAD);
    DisableItem(I_TRIBALELF);
    DisableItem(I_VIKING);
    DisableItem(I_BARBARIAN);
    DisableItem(I_DARKMAN);
    DisableItem(I_DESERTDWARF);
    DisableItem(I_PLAINSMAN);
    DisableItem(I_SEAELF);
    DisableItem(I_GREYELF);
    DisableItem(I_MINOTAUR);
    DisableItem(I_OGREMAN);
    DisableItem(I_HOBBIT);

    ModifyItemBasePrice(I_LEADERS, 700);

    EnableItem(I_MAN);
    ModifyItemBasePrice(I_MAN, 40);
    modify_race_skill_levels("HUMN", 4, 2);
    modify_race_skills("HUMN", 0, "BUIL");
    modify_race_skills("HUMN", 1, "RIDI");
    modify_race_skills("HUMN", 2, "COMB");
    modify_race_skills("HUMN", 3, "MINI");
    modify_race_skills("HUMN", 4, "FARM");
    modify_race_skills("HUMN", 5, "COOK");

    EnableItem(I_HILLDWARF);
    ModifyItemBasePrice(I_HILLDWARF, 40);
    modify_race_skill_levels("HDWA", 5, 2);
    modify_race_skills("HDWA", 0, "ARMO");
    modify_race_skills("HDWA", 1, "WEAP");
    modify_race_skills("HDWA", 2, "QUAR");
    modify_race_skills("HDWA", 3, "MINI");
    modify_race_skills("HDWA", 4, "BUIL");

    EnableItem(I_ICEDWARF);
    ModifyItemBasePrice(I_ICEDWARF, 40);
    modify_race_skill_levels("IDWA", 5, 2);
    modify_race_skills("IDWA", 0, "COMB");
    modify_race_skills("IDWA", 1, "WEAP");
    modify_race_skills("IDWA", 2, "MINI");
    modify_race_skills("IDWA", 3, "FISH");
    modify_race_skills("IDWA", 4, "ARMO");

    EnableItem(I_HIGHELF);
    ModifyItemBasePrice(I_HIGHELF, 40);
    modify_race_skill_levels("HELF", 5, 2);
    modify_race_skills("HELF", 0, "HORS");
    modify_race_skills("HELF", 1, "FISH");
    modify_race_skills("HELF", 2, "LBOW");
    modify_race_skills("HELF", 3, "SHIP");
    modify_race_skills("HELF", 4, "SAIL");

    EnableItem(I_WOODELF);
    ModifyItemBasePrice(I_WOODELF, 40);
    modify_race_skill_levels("WELF", 5, 2);
    modify_race_skills("WELF", 0, "LUMB");
    modify_race_skills("WELF", 1, "LBOW");
    modify_race_skills("WELF", 2, "ENTE");
    modify_race_skills("WELF", 3, "CARP");
    modify_race_skills("WELF", 4, "FISH");
    modify_race_skills("WELF", 5, "COOK");

    EnableItem(I_GNOME);
    ModifyItemBasePrice(I_GNOME, 30);
    modify_race_skill_levels("GNOM", 5, 2);
    modify_race_skills("GNOM", 0, "HERB");
    modify_race_skills("GNOM", 1, "QUAR");
    modify_race_skills("GNOM", 2, "ENTE");
    modify_race_skills("GNOM", 3, "XBOW");
    modify_race_skills("GNOM", 4, "HEAL");
    ModifyItemCapacities(I_GNOME,7,0,0,0);
    ModifyItemWeight(I_GNOME, 5);

    EnableItem(I_CENTAURMAN);
    ModifyItemBasePrice(I_CENTAURMAN, 70);
    modify_race_skill_levels("CTAU", 5, 2);
    modify_race_skills("CTAU", 0, "LUMB");
    modify_race_skills("CTAU", 1, "HORS");
    modify_race_skills("CTAU", 2, "RIDI");
    modify_race_skills("CTAU", 3, "HEAL");
    modify_race_skills("CTAU", 4, "FARM");

    EnableItem(I_LIZARDMAN);
    ModifyItemBasePrice(I_LIZARDMAN, 40);
    modify_race_skill_levels("LIZA", 5, 2);
    modify_race_skills("LIZA", 0, "HUNT");
    modify_race_skills("LIZA", 1, "HERB");
    modify_race_skills("LIZA", 2, "CARP");
    modify_race_skills("LIZA", 3, "SAIL");
    modify_race_skills("LIZA", 4, "HEAL");

    EnableItem(I_GOBLINMAN);
    ModifyItemBasePrice(I_GOBLINMAN, 30);
    modify_race_skill_levels("GBLN", 5, 2);
    modify_race_skills("GBLN", 0, "QUAR");
    modify_race_skills("GBLN", 1, "XBOW");
    modify_race_skills("GBLN", 2, "SHIP");
    modify_race_skills("GBLN", 3, "WEAP");
    modify_race_skills("GBLN", 4, "ENTE");
    ModifyItemCapacities(I_GOBLINMAN,7,0,0,0);
    ModifyItemWeight(I_GOBLINMAN, 5);

    EnableItem(I_GNOLL);
    ModifyItemBasePrice(I_GNOLL, 40);
    modify_race_skill_levels("GNOL", 5, 2);
    modify_race_skills("GNOL", 0, "HORS");
    modify_race_skills("GNOL", 1, "HUNT");
    modify_race_skills("GNOL", 2, "COMB");
    modify_race_skills("GNOL", 3, "ARMO");
    modify_race_skills("GNOL", 4, "CARP");
    modify_race_skills("GNOL", 5, "COOK");

    EnableItem(I_ORC);
    ModifyItemBasePrice(I_ORC, 40);
    modify_race_skill_levels("ORC", 5, 2);
    modify_race_skills("ORC", 0, "MINI");
    modify_race_skills("ORC", 1, "LUMB");
    modify_race_skills("ORC", 2, "COMB");
    modify_race_skills("ORC", 3, "BUIL");
    modify_race_skills("ORC", 4, "SHIP");

    // Underworld races
    EnableItem(I_DROWMAN);
    ModifyItemBasePrice(I_DROWMAN, 40);
    modify_race_skill_levels("DRLF", 5, 2);
    modify_race_skills("DRLF", 0, "COMB");
    modify_race_skills("DRLF", 1, "HUNT");
    modify_race_skills("DRLF", 2, "LBOW");
    modify_race_skills("DRLF", 3, "LUMB");
    modify_race_skills("DRLF", 4, "COOK");

    EnableItem(I_UNDERDWARF);
    ModifyItemBasePrice(I_UNDERDWARF, 40);
    modify_race_skill_levels("UDWA", 5, 2);
    modify_race_skills("UDWA", 0, "WEAP");
    modify_race_skills("UDWA", 1, "ARMO");
    modify_race_skills("UDWA", 2, "QUAR");
    modify_race_skills("UDWA", 3, "MINI");
    modify_race_skills("UDWA", 4, "BUIL");


    //
    // Change races per terrain
    //

    // Upper world
    // TODO: add ocean

    ClearTerrainRaces(R_PLAIN);
    ModifyTerrainRace(R_PLAIN, 0, I_HIGHELF);
    ModifyTerrainRace(R_PLAIN, 1, I_CENTAURMAN);
    ModifyTerrainRace(R_PLAIN, 2, I_GNOLL);
    ModifyTerrainRace(R_PLAIN, 3, I_MAN);
    ModifyTerrainCoastRace(R_PLAIN, 0, I_HIGHELF);
    ModifyTerrainCoastRace(R_PLAIN, 1, I_CENTAURMAN);
    ModifyTerrainCoastRace(R_PLAIN, 2, I_MAN);
    ModifyTerrainEconomy(R_PLAIN, 800, 12, 40, 1);

    ClearTerrainRaces(R_FOREST);
    ModifyTerrainRace(R_FOREST, 0, I_WOODELF);
    ModifyTerrainRace(R_FOREST, 1, I_CENTAURMAN);
    ModifyTerrainRace(R_FOREST, 2, I_HIGHELF);
    ModifyTerrainCoastRace(R_FOREST, 0, I_WOODELF);
    ModifyTerrainCoastRace(R_FOREST, 1, I_CENTAURMAN);
    ModifyTerrainCoastRace(R_FOREST, 2, I_HIGHELF);
    ModifyTerrainEconomy(R_FOREST, 600, 12, 20, 2);

    ClearTerrainRaces(R_MOUNTAIN);
    ModifyTerrainRace(R_MOUNTAIN, 0, I_HILLDWARF);
    ModifyTerrainRace(R_MOUNTAIN, 1, I_ORC);
    ModifyTerrainRace(R_MOUNTAIN, 2, I_MAN);
    ModifyTerrainCoastRace(R_MOUNTAIN, 0, I_HILLDWARF);
    ModifyTerrainCoastRace(R_MOUNTAIN, 1, I_ORC);
    ModifyTerrainCoastRace(R_MOUNTAIN, 2, I_MAN);
    ModifyTerrainEconomy(R_MOUNTAIN, 400, 11, 20, 2);

    ClearTerrainRaces(R_SWAMP);
    ModifyTerrainRace(R_SWAMP, 0, I_LIZARDMAN);
    ModifyTerrainRace(R_SWAMP, 1, I_GOBLINMAN);
    ModifyTerrainRace(R_SWAMP, 2, I_GNOLL);
    ModifyTerrainRace(R_SWAMP, 3, I_ORC);
    ModifyTerrainCoastRace(R_SWAMP, 0, I_LIZARDMAN);
    ModifyTerrainCoastRace(R_SWAMP, 1, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_SWAMP, 2, I_GNOLL);
    ModifyTerrainEconomy(R_SWAMP, 500, 11, 10, 2);

    ClearTerrainRaces(R_JUNGLE);
    ModifyTerrainRace(R_JUNGLE, 0, I_ORC);
    ModifyTerrainRace(R_JUNGLE, 1, I_WOODELF);
    ModifyTerrainRace(R_JUNGLE, 2, I_LIZARDMAN);
    ModifyTerrainRace(R_JUNGLE, 3, I_GNOME);
    ModifyTerrainCoastRace(R_JUNGLE, 0, I_ORC);
    ModifyTerrainCoastRace(R_JUNGLE, 1, I_WOODELF);
    ModifyTerrainCoastRace(R_JUNGLE, 2, I_LIZARDMAN);
    ModifyTerrainEconomy(R_JUNGLE, 500, 11, 20, 2);

    ClearTerrainRaces(R_DESERT);
    ModifyTerrainRace(R_DESERT, 0, I_GNOLL);
    ModifyTerrainRace(R_DESERT, 1, I_GOBLINMAN);
    ModifyTerrainRace(R_DESERT, 2, I_MAN);
    ModifyTerrainCoastRace(R_DESERT, 0, I_GNOLL);
    ModifyTerrainCoastRace(R_DESERT, 1, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_DESERT, 2, I_MAN);
    ModifyTerrainEconomy(R_DESERT, 400, 11, 10, 1);

    ClearTerrainRaces(R_TUNDRA);
    ModifyTerrainRace(R_TUNDRA, 0, I_ICEDWARF);
    ModifyTerrainRace(R_TUNDRA, 1, I_GNOME);
    ModifyTerrainRace(R_TUNDRA, 2, I_GNOLL);
    ModifyTerrainCoastRace(R_TUNDRA, 0, I_ICEDWARF);
    ModifyTerrainCoastRace(R_TUNDRA, 1, I_GNOME);
    ModifyTerrainCoastRace(R_TUNDRA, 2, I_GNOLL);
    ModifyTerrainEconomy(R_TUNDRA, 400, 11, 10, 2);

    // Underworld terrain

    ClearTerrainRaces(R_CAVERN);
    ModifyTerrainRace(R_CAVERN, 0, I_DROWMAN);
    ModifyTerrainRace(R_CAVERN, 1, I_UNDERDWARF);
    ModifyTerrainRace(R_CAVERN, 2, I_ORC);
    ModifyTerrainCoastRace(R_CAVERN, 0, I_UNDERDWARF);
    ModifyTerrainCoastRace(R_CAVERN, 1, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_CAVERN, 2, I_ORC);
    ModifyTerrainEconomy(R_CAVERN, 300, 11, 10, 2);

    ClearTerrainRaces(R_UFOREST);
    ModifyTerrainRace(R_UFOREST, 0, I_DROWMAN);
    ModifyTerrainRace(R_UFOREST, 1, I_GNOME);
    ModifyTerrainRace(R_UFOREST, 2, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_UFOREST, 0, I_DROWMAN);
    ModifyTerrainCoastRace(R_UFOREST, 1, I_GNOME);
    ModifyTerrainCoastRace(R_UFOREST, 2, I_GOBLINMAN);
    ModifyTerrainEconomy(R_UFOREST, 400, 11, 10, 2);

    ClearTerrainRaces(R_CHASM);
    ModifyTerrainRace(R_CHASM, 0, I_DROWMAN);
    ModifyTerrainRace(R_CHASM, 1, I_GNOME);
    ModifyTerrainRace(R_CHASM, 2, I_GOBLINMAN);
    ModifyTerrainCoastRace(R_CHASM, 0, I_UNDERDWARF);
    ModifyTerrainCoastRace(R_CHASM, 1, I_DROWMAN);
    ModifyTerrainCoastRace(R_CHASM, 2, I_GOBLINMAN);
    ModifyTerrainEconomy(R_CHASM, 200, 11, 10, 4);

    ClearTerrainRaces(R_TUNNELS);
    ModifyTerrainEconomy(R_TUNNELS, 0, 0, 0, 2);


    // Modify the various spells which are allowed to cross levels
    if (Globals->EASIER_UNDERWORLD) {
        modify_range_flags("rng_teleport", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_portal", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_farsight", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_clearsky", RangeType::RNG_CROSS_LEVELS);
        modify_range_flags("rng_weather", RangeType::RNG_CROSS_LEVELS);
    }

    if (Globals->TRANSPORT & GameDefs::ALLOW_TRANSPORT) {
        EnableSkill(S_QUARTERMASTER);
        EnableObject(O_CARAVANSERAI);
        if (Globals->EASIER_UNDERWORLD) modify_range_level_penalty("rng_transport", 4);
    }

    // NEWORIGINS' ENDGAME OBJECTS ARE NOT ENABLED HERE, DELIBERATELY. Rimefall is won by taking
    // both horde sources and then being elected (0011 section 6); ritual altars, entity cages,
    // monoliths and the ANNIHILATE skill belong to a different ending and would offer a second,
    // undesigned way to win — one that ends the game while a source still stands, which 0011
    // section 6 says explicitly cannot happen.
    //
    // Leaving them enabled was the state inherited from the stage 1 copy of NewOrigins, and it was
    // live rather than dormant: rulesetSpecificData carried victory_type = "annihilation".

    // Weapon BM example

    // Make SWOR to have malus of -1 on attack and -2 on defense vs. SPEA
    // modify_weapon_bonus_malus("SWOR", 0, "SPEA", -1, -2);

    // At the same time give SPEA bonus of 2 on attacka and 2 on defense vs. SWOR
    // modify_weapon_bonus_malus("SPEA", 0, "SWOR", 2, 2);
    return;
}

const std::optional<std::string> ARegion::movement_forbidden_by_ruleset(Unit *, ARegion *origin,
    ARegionList& regions) {

    //
    // THE TRAP THIS GUARDS AGAINST: the engine calls this on EVERY move in the game, not just on
    // arrivals through a gateway (monthorders.cpp). A start-slot check written without the
    // origin test below would make every start region permanently unenterable for the rest of the
    // game — nobody could ever walk into one, including its own owner coming home.
    //
    // So it keys on the SOURCE being the nexus, which is the only way a gateway arrival happens.
    //
    if (origin && origin->type == R_NEXUS) {
        // Arriving from the nexus is only legal into a free start slot. The engine falls back to
        // the gateway's nominal destination when the candidate list comes back empty, which is
        // exactly the case where the band had no room left, so this is the backstop that turns
        // that fallback into a readable refusal instead of a silent misplacement.
        bool is_slot = false;
        bool lost = false;
        for (const auto& slot : rimefall_slots(regions)) {
            if (slot.second != this) continue;
            is_slot = true;
            // The overrun state is read off the gateway's name, which CheckVictory refreshes every
            // turn. This hook receives no Game, so it cannot work out where the front is; the name
            // is persisted world state and is the only channel available.
            lost = slot.first->name.rfind("Lost gateway to ", 0) == 0;
            break;
        }
        if (!is_slot) {
            return "That gateway leads nowhere that is still open";
        }
        if (lost) {
            return "The cold has already taken that land";
        }
        if (rimefall_slot_taken(this)) {
            // Two ways here: the gateway has read "Sealed" for turns and someone tried it anyway,
            // or two factions stepped through in the same turn and the other one was processed
            // first. The wording has to be true of both, so it says nothing about when.
            return "That start location is already held";
        }
    }

    // NewOrigins guarded its map centre with a barrier that lifted once six altars were
    // empowered. Rimefall enables no altars, so the check could never fire; removed rather than
    // left inert, because a dead guard reads like a live one.

    return std::nullopt;
}

// Ruleset hook from docs/decisions/0012, and the reason that record exists.
//
// Rimefall keys its gateways on latitude band. The engine cannot express that on its own: it keeps
// only the TERRAIN of the region a gateway names and rebuilds the candidate set from the whole
// map, and no terrain identifies a band here — mountain occurs in all five, deliberately, because
// adamantium comes from mountain alone.
//
// So this replaces the candidate list with the free start slots of the band the gateway belongs
// to. The band comes from the destination's own latitude, never from parsing the gateway's name.
//
// An empty result is a legitimate answer: it means that band is full. The engine then leaves the
// unit's destination at the gateway's nominal region, and ARegion::movement_forbidden_by_ruleset
// refuses it with something a player can read. The two work as a pair.
void Game::filter_gateway_destinations(Object *gateway, ARegion *nominal, std::vector<ARegion *>& candidates)
{
    if (!gateway || !nominal) return;

    // ONE GATEWAY IS ONE SLOT. The destination is the hex the gateway actually names, so the band
    // and the terrain in its name are the truth rather than a hint.
    //
    // The first cut of this pooled every free slot in the band instead, and it read badly in
    // testing: a faction entering "Gateway to the middle lands, desert" was put on a plain,
    // because the pool had other slots in it. Naming a place and delivering a different one is
    // worse than offering less choice.
    //
    // Note this SETS the list rather than only shrinking it. docs/decisions/0012 describes the
    // hook as narrowing, and in the ordinary case this is a narrowing to one element — but a slot
    // curated at world creation has to stay reachable even if the engine's candidacy test would no
    // longer list it, since a region's production can drift over a long game. Losing a start
    // location silently, years in, would be the worse failure.
    candidates.clear();
    if (rimefall_slot_taken(nominal)) return;

    // Overrun slots are not offered. Read from the gateway's own name for the same reason
    // movement_forbidden_by_ruleset reads it: the state is refreshed once a turn by CheckVictory
    // and persisted in between.
    if (gateway->name.rfind("Lost gateway to ", 0) == 0) return;

    candidates.push_back(nominal);
}

