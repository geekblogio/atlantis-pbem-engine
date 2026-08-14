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

class ARegionArray;

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

#endif
