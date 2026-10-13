// START A3HEADER
//
// This source file is part of the Atlantis PBM game program.
// Copyright (C) 1995-1999 Geoff Dunbar
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program, in the file license.txt. If not, write
// to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
//
// See the Atlantis Project web page for details:
// http://www.prankster.com/project
//
// END A3HEADER
// MODIFICATIONS
// Date        Person          Comments
// ----        ------          --------
// 2000/MAR/14 Larry Stanbery  Added a unit:faction map capability.
#include "gamedefs.h"
#include "game.h"
#include "items.h"
#include "skills.h"
#include "gamedata.h"

#include <cstdlib>
#include <stdexcept>
#include <string>

void usage()
{
	Awrite("atlantis new");
	Awrite("atlantis run");
	Awrite("atlantis edit");
	Awrite("");
	Awrite("atlantis map <geo|wmon|lair|gate|hex> <mapfile>");
	Awrite("atlantis mapunits");
	Awrite("atlantis genrules <introfile> <cssfile> <rules-outputfile>");
	Awrite("");
	Awrite("atlantis check <orderfile> <checkfile>");
	Awrite("");
	Awrite("Environment:");
	Awrite("  ATLANTIS_SEED=<integer>   fix the world generation seed used by `new`");
	Awrite("  ATLANTIS_FORCE_GM_REPORT  write the world-wide GM report every turn");
}

int main(int argc, char *argv[])
{
	Game game;
	int retval = 1;

	initIO();

	Awrite(AString("Atlantis Engine Version: ") +
			ATL_VER_STRING(CURRENT_ATL_VER));
	Awrite(AString(Globals->RULESET_NAME) + ", Version: " +
			ATL_VER_STRING(Globals->RULESET_VERSION));
	Awrite("");

	if (argc == 1) {
		usage();
		doneIO();
		return 0;
	}

	// ATLANTIS_SEED makes `new` reproducible: the same seed builds a
	// byte-identical game.out. It stands in for the clock reading the world
	// would otherwise be seeded from (see seedclock() in gameio.cpp), so the
	// seed of a game created live is the creation time in Unix seconds.
	// Unset -- the normal case -- nothing changes. `run` is unaffected either
	// way: a running game reseeds from the value saved in game.in.
	if (const char *seed_env = std::getenv("ATLANTIS_SEED")) {
		try {
			int seed = std::stoi(seed_env);
			setfixedseed(seed);
			Awrite(AString("Using deterministic world seed ") + seed +
					" (ATLANTIS_SEED).");
		} catch (const std::exception&) {
			Awrite(AString("Ignoring ATLANTIS_SEED: '") + seed_env +
					"' is not an integer.");
		}
	}

	// Writes the world-wide GM report (faction 1) every turn, not just on the
	// first one. It flips the ruleset global, exactly as master does; nothing
	// in the game reads GM_REPORT except the report writers, so the turn
	// itself plays out the same. (Writing the GM report does record "seen"
	// items/skill levels on faction 1 via ShowSkill::Report -> DiscoverItem,
	// so a ruleset that ships with GM_REPORT = 0 can get a different faction 1
	// record in game.out. NewOrigins ships with GM_REPORT = 1: no difference.)
	if (std::getenv("ATLANTIS_FORCE_GM_REPORT")) {
		Globals->GM_REPORT = 1;
		Awrite("World GM report forced on (ATLANTIS_FORCE_GM_REPORT).");
	}

	game.ModifyTablesPerRuleset();

	do {
		if (AString(argv[1]) == "new") {
			if (!game.NewGame()) {
				Awrite( "Couldn't make the new game!" );
				break;
			}

			if ( !game.SaveGame() ) {
				Awrite( "Couldn't save the game!" );
				break;
			}

			if ( !game.WritePlayers() ) {
				Awrite( "Couldn't write the players file!" );
				break;
			}
		} else if (AString(argv[1]) == "map") {
			if (argc != 4) {
				usage();
				break;
			}

			if ( !game.OpenGame() ) {
				Awrite( "Couldn't open the game file!" );
				break;
			}

			if ( !game.ViewMap( argv[2], argv[3] )) {
				Awrite( "Couldn't write the map file!" );
				break;
			}
		} else if (AString(argv[1]) == "run") {
			if ( !game.OpenGame() ) {
				Awrite( "Couldn't open the game file!" );
				break;
			}

			if ( !game.RunGame() ) {
				Awrite( "Couldn't run the game!" );
				break;
			}

			if ( !game.SaveGame() ) {
				Awrite( "Couldn't save the game!" );
				break;
			}
		} else if (AString(argv[1]) == "edit") {
			if ( !game.OpenGame() ) {
				Awrite( "Couldn't open the game file!" );
				break;
			}

			int saveGame = 0;
			if ( !game.EditGame( &saveGame ) ) {
				Awrite( "Couldn't edit the game!" );
				break;
			}

			if ( saveGame ) {
				if ( !game.SaveGame() ) {
					Awrite( "Couldn't save the game!" );
					break;
				}
			}
		} else if ( AString( argv[1] ) == "check" ) {
			if (argc != 4) {
				usage();
				break;
			}

			game.DummyGame();
			if ( !game.DoOrdersCheck( argv[ 2 ], argv[ 3 ] )) {
				Awrite( "Couldn't check the orders!" );
				break;
			}
		} else if ( AString( argv[1] ) == "mapunits" ) {
			if ( !game.OpenGame() ) {
				Awrite( "Couldn't open the game file!" );
				break;
			}
			game.UnitFactionMap();
		} else if (AString(argv[1])== "genrules") {
			if (argc != 5) {
				usage();
				break;
			}
			if (!game.GenRules(argv[4], argv[3], argv[2])) {
				Awrite("Unable to generate rules!");
				break;
			}
		} else {
			Awrite(AString("Unknown option: ") + argv[1]);
			break;
		}
		retval = 0;
	} while( 0 );

	doneIO();
	return retval;
}
