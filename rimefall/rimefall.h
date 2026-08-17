// Rimefall's shape constants, in one place because more than one file needs them.
//
// The climate bands are read in world.cpp (GetRegType) and the taper in map.cpp (MakeLand), and
// from stage 3 onwards the bands also key the gateways, the start slot registry and both invasion
// fronts. Keeping the count in a header is what stops those from disagreeing about how many bands
// the world has.
//
// Everything here is a tuning value only this ruleset reads, so none of it is a GameDefs field —
// see docs/decisions/0010 section 5 for why that distinction matters.

#ifndef RIMEFALL_H
#define RIMEFALL_H

#include <string>

class ARegion;
class ARegionArray;
class ARegionList;

// How many latitude bands the continent is divided into, north to south. Five is the coarsest
// split that still distinguishes frontier from crush zone from dry frontier; see
// docs/decisions/0010 section 7. Changing this changes the terrain gradient AND, from stage 3,
// the start location distribution.
#define RIMEFALL_BANDS 5

// The continent is a taper: wide in the north, narrow in the south. These are the half-widths at
// the northern and southern edges of the map, as a fraction of the array width.
//
// THESE TWO NUMBERS AND `OCEAN` IN rules.cpp ARE ONE SETTING, NOT THREE. MakeLand grows land until
// the ocean count falls below its target, and rimefall confines land to the taper — so if the
// taper cannot hold the land the target demands, generation cannot finish. rimefall_taper_area()
// and the stall guard in MakeLand exist to make that failure loud instead of a hang. Raising
// OCEAN shrinks the demand; widening the taper raises the supply. Move one without checking the
// other and world generation stops working.
// The southern figure was 0.10 first, and that was too narrow to be a place: across five
// generated worlds the far south held between 8 and 44 land hexes, and at the low end it carried
// no mountains at all — which is exactly the silent economy hole the band weighting in
// world.cpp exists to prevent. 0.18 keeps the taper obvious (the north is still better than twice
// the south) while leaving the far south large enough to live in and to place a start location in.
#define RIMEFALL_TAPER_NORTH 0.40
#define RIMEFALL_TAPER_SOUTH 0.18

// Half-width of the taper, in columns, at row y.
int rimefall_taper_half_width(ARegionArray *pRegs, int y);

// Is this cell inside the taper? The surface wraps in x, so the distance to the continent's
// centre line is measured modulo the array width.
bool rimefall_inside_taper(ARegionArray *pRegs, int x, int y);

// How many hexes the taper contains, for the sanity check in MakeLand.
int rimefall_taper_area(ARegionArray *pRegs);

// Which climate band a row belongs to, 0 in the far north to RIMEFALL_BANDS-1 in the far south.
// ONE definition, used by GetRegType, by the gateways and by the start slots alike — if these ever
// disagreed about where a band ends, terrain and start locations would silently drift apart.
int rimefall_band_of_row(ARegionArray *pRegs, int y);
int rimefall_band_of(ARegion *reg, ARegionList& regions);

// What players see. Index by band.
const std::string& rimefall_band_name(int band);

// How many starting locations each band offers, far north first.
//
// THIS IS THE DENSITY CURVE, AND IT IS THE BALANCING AXIS OF THE WHOLE MAP (0010 section 7): the
// edges are pressured by monsters, the middle by other players. Read it against the land each band
// actually holds, measured over the six worlds generated for #39:
//
//   band 0  far north    89-117 hexes    3 starts   roomy, poor ground, the northern front
//   band 1  near north  157-225 hexes    5 starts   the frontier: open, already contested
//   band 2  middle      117-198 hexes    8 starts   the crush zone — least land per faction
//   band 3  near south   94-125 hexes    3 starts   breathing room, within dragon reach
//   band 4  far south    30-58  hexes    1 start    the most land per faction anywhere
//
// 0010 section 7 assumed the far north held the most land because the continent is widest there.
// It is widest, but it holds LESS land than the middle bands: MakeLand never seeds the rows next
// to the pole. These counts are set against measured land rather than against that assumption, so
// the crowding actually comes out where the design wants it — roughly twice as tight in the middle
// as at either edge.
//
// Expect to revisit this after the first real game; it is the parameter most likely to be wrong.
extern const int rimefall_starts_per_band[RIMEFALL_BANDS];

// How far apart two start locations may be and still count as neighbours for the starting
// alliance, in planar distance.
//
// PROXIMITY, NOT BAND. 0010 section 8's heading says "same band" and its body says "clustered by
// starting location"; 0011 section 5 settled that the body wins, because the middle band holds by
// far the most start locations and a band-wide alliance would turn the most contested zone into a
// single bloc — the opposite of what the density curve is for.
//
// It also has to stay well under what an election needs: 0011 section 6 wins the game by mutual
// ALLY from a share of all living factions, and that only means anything if no starting cluster is
// already big enough. Raise this and the endgame gets easier without anything saying so.
//
// Measured on a 64x64 world filled to all twenty start slots, allies per faction:
//
//   radius   mean   largest cluster   factions with no ally
//     16      9.2        15                   0             a single bloc is three quarters of the game
//     12      5.9         9                   0
//      9      3.8         6                   0
//      7      2.3         4                   1             <-- chosen
//      5      1.1         4                   9             half the players never get the mechanic
//      3      0           0                  20             nothing happens at all
//
// Seven keeps clusters small enough that the largest is a fifth of the field while almost everyone
// still starts with someone. Stable across three seeds: mean 2.3 to 2.8, largest 4 to 6, at most
// one faction left without.
#define RIMEFALL_ALLY_RADIUS 7

//
// ---------------------------------------------------------------------------------------------
// The two invasion fronts
// ---------------------------------------------------------------------------------------------
//
// Everything below is derived from TurnNumber() or read back out of the persisted world. There is
// no accumulator anywhere and there cannot be one: rulesetSpecificData is not saved and the
// game.in format is a published interface. See docs/decisions/0011.
//

// HOW FAST THE NORTHERN FRONT CREEPS SOUTH, in rows per ten turns. Expressed per ten so the speed
// can be tuned below one row a turn without floating point.
//
// THIS IS THE SINGLE MOST IMPORTANT BALANCE CONSTANT IN THE RULESET (0011 section 1). The front
// never stops and nothing players do slows it, so it is the game's hard clock: too fast and the
// game is unwinnable, too slow and it is scenery. At 7, on a 64-row map, the front reaches the
// middle band around turn 45 and the southern edge around turn 90.
//
// It is the first thing to revisit after a real game is played.
#define RIMEFALL_FRONT_ROWS_PER_10_TURNS 7

// How many rows deep the spawn band is. The front is a wall, not a line.
#define RIMEFALL_FRONT_BAND_DEPTH 3

//
// The threat score: WHEN the front attacks and HOW HARD, recomputed from scratch every turn.
// Position and strength are separate mechanisms (0011 section 2) — the creep above decides where
// the front is, these decide what it does there.
//
// There is deliberately NO separate grace period. The time term starts near zero and that IS the
// grace: at these weights nothing happens until roughly turn thirty.
//
// The three weights are set so that no single term can decide the score on its own. Measured over
// 45 turns of a six-faction game, the first attempt had prosperity at 747 against time at 90 — an
// eight-to-one split that made the other two terms decoration. Prosperity is now counted per ten
// thousand people rather than per thousand, which brings it alongside time through the midgame.
#define RIMEFALL_THREAT_PER_TURN      2   // time: rises steadily, and is the de-facto grace
#define RIMEFALL_THREAT_PER_10KPOP    4   // prosperity: per 10000 people within the front's reach
#define RIMEFALL_THREAT_PER_BATTLE   10   // discord: per player-versus-player battle this turn
#define RIMEFALL_THREAT_THRESHOLD    60   // below this the front does not attack at all

// Above the threshold, one more stack per this much excess. Keeps the front escalating rather
// than firing one identical wave forever.
#define RIMEFALL_THREAT_PER_EXTRA_STACK 40
#define RIMEFALL_WAVE_MAX_STACKS 6

// WHEN THE DRAGONS WAKE. The eastern front opens once the northern spawn band has passed this
// fraction of the map, in tenths — or immediately when the northern source is taken, whichever
// comes first (0011 section 4). The second condition is what stops a fast group from skipping half
// the game by killing the north early: success in the north brings the second act forward rather
// than cancelling it.
#define RIMEFALL_DRAGON_WAKE_TENTHS 4

// The eastern front is few and fast against the northern front's wall of attrition.
#define RIMEFALL_DRAGON_STACKS_MAX 2

// The two sources are named individually rather than by renaming their object types, because
// O_ICECAVE and O_DCLIFFS also occur naturally as ordinary lairs and renaming the type would
// rename every one of them.
#define RIMEFALL_NORTH_SOURCE_NAME "The Rimewell"
#define RIMEFALL_EAST_SOURCE_NAME "The Saltspire"

// Where the two horde sources stand. Pure lookups: Game::CreateWorld builds the objects and
// garrisons them, because MakeLMon is private to Game and only a Game member can reach it.
//
// The eastern site is found from the LANDMASS, never from x near the array width.
// ARegionArray::GetRegion reduces both coordinates modulo the array size, so the surface has no
// eastern edge and the eastern ocean is also the western one (0010 section 5). "East" is well
// defined only relative to the continent.
ARegion *rimefall_north_source_site(ARegionArray *pRegs);
ARegion *rimefall_east_source_site(ARegionArray *pRegs);

#endif
