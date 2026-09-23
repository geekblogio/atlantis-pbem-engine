#!/bin/bash
#
# Replays the recorded `standard` turns with ATLANTIS_PHASE_DUMPS set and checks two things:
#
#   1. every turn writes the 32 phase files, phase.00-orders.out to phase.31-post-turn.out;
#   2. everything else the turn writes is byte for byte the recording, the engine's stdout
#      apart from the one line that announces the variable.
#
# The second is the point: the phase files are only useful if writing them leaves the turn
# alone, and one stray draw from the RNG would move every file after it. A last run of turn 0
# without the variable checks that it then writes no phase file at all.

game=standard
executable=../standard/standard
turndir=turns
announcement="Writing the world after every phase (ATLANTIS_PHASE_DUMPS)."
phases="00-orders 01-find 02-enter 03-promote 04-combat 05-stealth 06-give 07-enter-new 08-exchange
09-destroy 10-pillage 11-tax 12-guard1 13-magic 14-sell 15-buy 16-forget 17-mid-turn 18-quit
19-empty-units 20-withdraw 21-sacrifice 22-movement 23-teach 24-month 25-economics 26-teleport
27-transport 28-annihilate 29-maintenance 30-migration 31-post-turn"

[ -f "${executable}" ] || executable="../build/$game"

if [[ ! -e "${executable}" ]]; then
  echo "Please build the $game executable before running the phase dump test.  Test failed."
  exit 1
fi

cp "${executable}" "./${game}"
chmod +x "./$game"

cleanup() {
  rm -f game.* players.* times.* orders.* template.* report.* phase.* engine-output.txt
  rm -f turn-difference.txt
  rm -rf ./output
  rm -f "./$game"
}

stage() {
  cp -f "$turndir/turn_$1/game.in" game.in
  cp -f "$turndir/turn_$1/players.in" players.in
  for ordersfile in "$turndir/turn_$1"/orders.*; do
    [[ -e "$ordersfile" ]] || continue
    cp -f "$ordersfile" "$(basename "$ordersfile")"
  done
}

lastTurn=$(<"$turndir/turn")

for turn in $(seq 0 "$lastTurn")
do
  echo -n "Replaying $game turn $turn with phase dumps..."

  if [[ ! -d "$turndir/turn_$turn" ]]; then
    echo "turn $turn missing. -- Test failed."
    cleanup
    exit 1
  fi

  stage "$turn"
  if ! ATLANTIS_PHASE_DUMPS=1 "./$game" run &> engine-output.txt ; then
    echo "executable crashed. -- Test failed."
    cat engine-output.txt
    cleanup
    exit 1
  fi

  for phase in $phases; do
    if [[ ! -s "phase.$phase.out" ]]; then
      echo "phase.$phase.out missing or empty. -- Test failed."
      cleanup
      exit 1
    fi
  done
  count=$(shopt -s nullglob; set -- phase.*; echo $#)
  if [[ "$count" -ne 32 ]]; then
    echo "$count phase files instead of 32. -- Test failed."
    ls phase.*
    cleanup
    exit 1
  fi
  rm -f phase.*

  if ! grep -qxF "$announcement" engine-output.txt; then
    echo "the variable was not announced. -- Test failed."
    cleanup
    exit 1
  fi
  grep -vxF "$announcement" engine-output.txt > engine-output.stripped
  mv engine-output.stripped engine-output.txt

  mkdir -p "output/turn_$turn"
  mv game.* players.* orders.* template.* report.* engine-output.txt "output/turn_$turn"
  if [[ $(shopt -s nullglob; set -- times.*; echo $#) -ge 1 ]]; then
    mv times.* "output/turn_$turn"
  fi

  if ! diff -ur "$turndir/turn_$turn" "output/turn_$turn" &> turn-difference.txt ; then
    echo "output differed from the recording. -- Test failed."
    cat turn-difference.txt
    cleanup
    exit 1
  fi

  echo "32 phase files, the turn identical. -- Test succeeded."
  rm -rf ./output turn-difference.txt
done

echo -n "Replaying $game turn 0 without phase dumps..."
stage 0
if ! "./$game" run &> engine-output.txt ; then
  echo "executable crashed. -- Test failed."
  cat engine-output.txt
  cleanup
  exit 1
fi
if [[ $(shopt -s nullglob; set -- phase.*; echo $#) -ne 0 ]]; then
  echo "phase files written without the variable. -- Test failed."
  cleanup
  exit 1
fi
echo "no phase files. -- Test succeeded."

cleanup
exit 0
