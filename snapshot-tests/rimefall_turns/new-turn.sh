#!/bin/bash

curTurn=$(< ./turn)
nextTurn=$((curTurn + 1))

if [[ ! -f ../../rimefall/rimefall ]]; then
  echo "Please build the rimefall executable before trying to create a new snapshot turn."
  exit 1
fi

if [[ ! -d turn_$nextTurn ]]; then
  mkdir turn_$nextTurn
  cp turn_$curTurn/game.out turn_$nextTurn/game.in
  cp turn_$curTurn/players.out turn_$nextTurn/players.in
  # One orders file per faction that got a template, so a turn can exercise two factions acting at
  # once. The gateway collision test needs exactly that.
  for tmpl in turn_$curTurn/template.*; do
    [[ -e "$tmpl" ]] || continue
    cp "$tmpl" "turn_$nextTurn/orders.${tmpl##*.}"
  done
fi

echo "Modify the input orders for turn_$nextTurn to test desired changes"
echo "When that is complete, rerun this script."

changed=0
for tmpl in turn_$curTurn/template.*; do
  [[ -e "$tmpl" ]] || continue
  if ! diff "$tmpl" "turn_$nextTurn/orders.${tmpl##*.}" &> /dev/null; then
    changed=1
  fi
done

if [[ $changed = 0 ]]; then
  echo "No order changes have occured.   Exiting."
  exit 0
fi

echo "Running turn to produce new output"
cd turn_$nextTurn
../../../rimefall/rimefall run &> engine-output.txt

if [[ $? != 0 ]]; then
   echo "Game engine crashed while generating turn. Turn not generated."
   cat engine-output.txt
   rm -f *.out
   rm -f template.*
   rm -f report.*
   rm -f times.*
   rm -f engine-output.txt
fi

echo "$nextTurn" > ../turn
