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
#ifndef MARKET_CLASS
#define MARKET_CLASS

#include <iostream>

class Market {
public:
	enum struct MarketType : int {
		M_BUY,
		M_SELL
	};

	Market() :
		type(MarketType::M_BUY), item(0), price(0), amount(0),
		minpop(0), maxpop(0), minamt(0), maxamt(0), baseprice(0), activity(0)
	{}

	/* type, item, price, amount, minpop, maxpop, minamt, maxamt */
	Market(MarketType type, int item, int price, int amount, int minpop, int maxpop, int minamt, int maxamt) :
		type(type), item(item), price(price), amount(amount),
		minpop(minpop), maxpop(maxpop), minamt(minamt), maxamt(maxamt),
		baseprice(price), activity(0)
	{}

	MarketType type;
	int item;
	int price;
	int amount;

	int minpop;
	int maxpop;
	int minamt;
	int maxamt;

	int baseprice;
	int activity;

	// Does this market name an item at all?
	//
	// A market with item == -1 is a SUPPORTED STATE, not corruption: write_out() persists it as
	// NO_ITEM and read_in() restores it as -1, because lookup_item() returns -1 for a name the
	// build does not know. Both ends of the save file have always agreed about it.
	//
	// What must never happen is ItemDefs[item] for one. The index lands in front of the table --
	// on a 64-bit build, 124 bytes before it -- and reads whatever happens to be there. That is
	// usually some other static data and passes unnoticed, which is exactly why it surfaced as a
	// turn replay that crashed in some runs and not others.
	bool has_item() const { return item >= 0; }

	void post_turn(int population, int wages);
	void write_out(std::ostream& f);
	void read_in(std::istream& f);
};
#endif
