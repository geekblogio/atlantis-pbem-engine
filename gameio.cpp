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
#include <iostream>
#include <fstream>

using namespace std;

#include "gameio.h"
#include "gamedefs.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "i_rand.h"

static randctx isaac_ctx;

#define ENDLINE '\n'
char buf[256];

void cleartoendl()
{
	char ch = ' ';
	while (!(cin.eof()) && (ch != ENDLINE))
	{
		ch = cin.get();
	}
}

void initIO()
{
	seedrandom( 1783 );
}

void doneIO()
{
}

int getrandom(int range)
{
	int neg = (range < 0);
	if (!range) return 0;
	int ret = 0;
	if (neg) range = -range;
	unsigned long i = isaac_rand( &isaac_ctx );
	i = i % range;
	if (neg) ret = (int)(i*-1);
	else ret = (int)i;
	return ret;
}

void seedrandom(int num)
{
	ub4 i;
	isaac_ctx.randa = isaac_ctx.randb = isaac_ctx.randc = (ub4)0;
	for (i=0; i<256; ++i)
	{
		isaac_ctx.randrsl[i]=(ub4)num+i;
	}
	randinit( &isaac_ctx, TRUE );
}

// ATLANTIS_SEED, once main() has parsed it. Unset is the normal case: every
// seed taken during world generation then comes from the clock, as it always
// has.
static bool fixed_seed_set = false;
static int fixed_seed = 0;

void setfixedseed(int seed)
{
	fixed_seed = seed;
	fixed_seed_set = true;
}

time_t seedclock()
{
	// time_t on purpose: callers used to pass time(0) straight through, and the
	// implicit conversions they apply to it (to int for seedrandom, to the
	// engine's result_type for std::default_random_engine::seed) must stay
	// exactly what they were for a clock reading.
	if (fixed_seed_set) return (time_t) fixed_seed;
	return time(0);
}

void seedrandomrandom()
{
	seedrandom( seedclock() );
}

int makeRoll(int rolls, int sides) {
	int result = 0;
	for (int i = 0; i < rolls; i++) {
		result += getrandom(sides) + 1;
	}

	return result;
}

int clamp(int imin, int ivalue, int imax) {
	return max(imin, min(ivalue, imax));
}

// Every prompt in the engine sits in a loop that asks again until it gets a
// usable answer. Once stdin has ended, a read fails immediately and forever, so
// such a loop would repeat its question at full speed and never return -- a hung
// job when the engine is driven as a subprocess with a short answers file. There
// is nobody left to answer, so stop: nothing has been saved at this point, and
// a non-zero exit is something the caller can see.
[[noreturn]] static void inputended()
{
	cout << "The input ended before the question was answered. "
		"Quitting without saving." << ENDLINE;
	cout.flush();
	exit(1);
}

int Agetint()
{
	int x;
	if (!(cin >> x)) {
		if (cin.eof()) inputended();
		// Not a number. Before this guard the stream stayed in its failed
		// state and cleartoendl() spun on it forever, so no working input ever
		// took this path. Skip the bad line and return 0, which every caller
		// already treats as invalid and answers by asking again.
		cin.clear();
		cleartoendl();
		return 0;
	}
	cleartoendl();
	return x;
}

void Awrite(const AString & s)
{
	cout << s << ENDLINE;
}

void Adot()
{
	cout << ".";
}

void message(char * c)
{
	cout << c << ENDLINE;
	morewait();
}

void morewait()
{
	cout << ENDLINE;
	cin.getline(buf,256,ENDLINE);
	cout << ENDLINE;
}


AString * getfilename(const AString & s)
{
	cout << s;
	return( AGetString() );
}

AString *AGetString()
{
	if (!cin.getline( buf, 256, ENDLINE )) {
		// Nothing extracted and at the end: the input is over (see
		// inputended). The edit menu would otherwise loop on "" forever.
		if (cin.eof() && !cin.gcount()) inputended();
		// A line too long for buf: getline returns the first 255 characters
		// and sets failbit, which used to make every later read fail. Keep
		// what was read (as before), drop the rest of the line, and recover.
		cin.clear();
		cleartoendl();
	}
	return( new AString( buf ));
}
