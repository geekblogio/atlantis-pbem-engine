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

#endif
