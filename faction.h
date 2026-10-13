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
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
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
// Date				Person				 Comments
// ----				------				 --------
// 2000/MAR/14 Davis Kulis		Added a new reporting Template.
// 2001/Feb/18 Joseph Traub	 Added Apprentices from Lacandon Conquest
#ifndef FACTION_CLASS
#define FACTION_CLASS

class Faction;
class Game;

#include "gameio.h"
#include "aregion.h"
#include "fileio.h"
#include "unit.h"
#include "battle.h"
#include "skills.h"
#include "items.h"
#include "alist.h"
#include "astring.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "atljson.h"

class ShowSkill;
class ItemShow;
class ObjectShow;

// The JSON report's record of one line of "Events during turn:" or "Errors
// during turn:". The text report keeps its AString lists exactly as before;
// these are kept alongside them, in the same order, by Faction::Event/Error.
//
// Gaps against master's FactionEvent/FactionError, which this branch cannot
// fill honestly:
//  - no "category": stable's call sites carry none, and inventing one per
//    message would be guesswork;
//  - no "region": stable never passes one.
// The "unit" is known exactly when the line came through Unit::Event or
// Unit::Error (which compose "Name (123): message"); the JSON then carries the
// bare message and the unit, as master does. unit_number 0 means "no unit"
// (unit numbers start at 1). Name and number are taken when the line is
// written, not at report time, so a unit renamed or gone later still matches
// the text.
struct FactionEvent {
	std::string message;
	std::string unit_name;
	int unit_number;
};
struct FactionError {
	std::string message;
	std::string unit_name;
	int unit_number;
};
void to_json(json &j, const FactionEvent &e);
void to_json(json &j, const FactionError &e);

enum {
	A_HOSTILE,
	A_UNFRIENDLY,
	A_NEUTRAL,
	A_FRIENDLY,
	A_ALLY,
	NATTITUDES
};

extern const std::string F_WAR;
extern const std::string F_TRADE;
extern const std::string F_MAGIC;
extern const std::string F_MARTIAL;

// DK
// LLS - make templates cleaner for save/restore
enum {
	TEMPLATE_OFF,
	TEMPLATE_SHORT,
	TEMPLATE_LONG,
	TEMPLATE_MAP,
	NTEMPLATES
};

enum {
	QUIT_NONE,
	QUIT_BY_ORDER,
	QUIT_BY_GM,
	QUIT_AND_RESTART,
	QUIT_WON_GAME,
	QUIT_GAME_OVER,
};

extern char const ** AttitudeStrs;
extern std::vector<std::string> *FactionTypes;

// LLS - include strings for the template enum
extern char const **TemplateStrs;
int ParseTemplate(AString *);

int ParseAttitude(AString *);

int MagesByFacType(int);

class FactionVector {
public:
	FactionVector(int);
	~FactionVector();

	void ClearVector();
	void SetFaction(int, Faction *);
	Faction *GetFaction(int);

	Faction **vector;
	int vectorsize;
};
	
class Attitude : public AListElem {
public:
	Attitude();
	~Attitude();
	void Writeout(Aoutfile * );
	void Readin( Ainfile *, ATL_VER version );
	
	int factionnum;
	int attitude;
};

enum FactionActivity {
	TAX     = 1,
	TRADE   = 2
};

class FactionPtr : public AListElem {
public:
	Faction * ptr;
};

class Faction : public AListElem
{
public:
	Faction();
	Faction(int);
	~Faction();
	
	void Readin( Ainfile *, ATL_VER version );
	void Writeout( Aoutfile * );
	void View();
	
	void SetName(AString *);
	void SetNameNoChange( AString *str );
	void SetAddress( AString &strNewAddress );
	
	void CheckExist(ARegionList *);
	void Error(const AString &);
	void Event(const AString &);
	// From Unit::Error/Unit::Event: `s` is the line the text report prints
	// ("Name (123): message"), `message` the bare message and `u` its unit,
	// for the JSON report.
	void Error(const AString &s, Unit *u, const AString &message);
	void Event(const AString &s, Unit *u, const AString &message);

	AString FactionTypeStr();
	void WriteReport( Areport *f, Game *pGame, int ** citems);

	// JSON report (report.<n>.json), ported from master 4e04648.
	//
	// Order matters: Game::WriteReport calls build_json_report BEFORE
	// WriteReport, because WriteReport empties the errors/events/battles
	// lists, and add_json_show_reports AFTER it, because the skill/item/
	// object reports only exist once WriteReport has produced them -- see
	// RecordSkillShow. clear_json_report_state ends the cycle.
	bool gets_gm_report(Game *game);
	void build_json_report(json &j, Game *game, int **citems);
	void build_gm_json_report(json &j, Game *game);
	void add_json_show_reports(json &j);
	void clear_json_report_state();
	// Called by WriteReport for every skill/item/object report it prints.
	// The text report is the only safe place to collect them: ShowSkill::
	// Report(this) has side effects (DiscoverItem records the item on the
	// faction, persisted in game.out, and queues more item/object/skill
	// reports while the list is being walked), so the JSON report must not
	// call it a second time. Capturing what was printed keeps the text
	// report and game.out byte-identical and the JSON equal to the text.
	void RecordSkillShow(ShowSkill *s, const AString &description);
	void RecordItemShow(ItemShow *s);
	void RecordObjectShow(ObjectShow *s);
	std::vector<FactionEvent> json_events;
	std::vector<FactionError> json_errors;
	json json_shows;
	// LLS - write order template
	void WriteTemplate(Areport *f, Game *pGame);
	void WriteFacInfo(Aoutfile *);
	
	void SetAttitude(int,int); /* faction num, attitude */
	/* if attitude == -1, clear it */
	int GetAttitude(int);
	void RemoveAttitude(int);
	
	int CanCatch(ARegion *,Unit *);
	/* Return 1 if can see, 2 if can see faction */
	int CanSee(ARegion *,Unit *, int practice = 0);
	
	void DefaultOrders();
	void TimesReward();
	
	void SetNPC();
	int IsNPC();

	void DiscoverItem(int item, int force, int full);

	int num;

	//
	// The type is only used if Globals->FACTION_LIMIT_TYPE ==
	// FACLIM_FACTION_TYPES
	//
	std::unordered_map<std::string, int> type;

	int lastchange;
	int lastorders;
	int unclaimed;
	int bankaccount;
	int interest; // not written to game.out
	AString * name;
	AString * address;
	AString * password;
	int times;
	int showunitattitudes;
	int temformat;
	int battleLogFormat;
	char exists;
	int quit;
	int numshows;
	
	int nummages;
	int numapprentices;
	int numqms;
	int numtacts;
	// AList war_regions;
	// AList trade_regions;

	std::unordered_map<ARegion *, std::unordered_set<FactionActivity, std::hash<int>>> activity;
	int GetActivityCost(FactionActivity type);
	void RecordActivity(ARegion *region, FactionActivity type);
	bool IsActivityRecorded(ARegion *region, FactionActivity type);

	/* Used when writing reports */
	AList present_regions;
	
	int defaultattitude;
	AList attitudes;
	SkillList skills;
	ItemList items;
	
	//
	// Both are lists of AStrings
	//
	AList extraPlayers;
	AList errors;
	AList events;
	AList battles;
	AList shows;
	AList itemshows;
	AList objectshows;

	// These are used for 'granting' units to a faction via the players.in
	// file
	ARegion *pReg;
	ARegion *pStartLoc;
	int noStartLeader;
	int startturn;

	void WriteFactionStats(Areport *f, Game *pGame, int ** citems);
	json compute_faction_statistics(Game *game, int **citems);
};

Faction * GetFaction(AList *,int);
Faction * GetFaction2(AList *,int); /*This AList is a list of FactionPtr*/

#endif
