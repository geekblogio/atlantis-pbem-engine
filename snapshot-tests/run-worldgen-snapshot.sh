#!/bin/bash
#
# Replays WORLD CREATION, which the turn snapshots cannot reach.
#
# `run-game-snapshots.sh` replays `<game> run` against a world that already exists, so every line
# of world generation -- terrain, towns, city guards, starting locations -- is exercised by
# nothing. That is not a theoretical gap: `#66` fixed a defect that only ever showed there, in
# `kingdoms`, the one ruleset whose guards are made by MakeManUnit.
#
# A recorded world is cheap. `worldgen/<game>/` holds the answers the ruleset's `new` asks for,
# the seed, and the files it wrote; a 24x24 kingdoms world is under 100 KB and takes a second.

cleanup()
{
  game="$1"
  rm -f "${game}"
  rm -f game.out players.out names.out engine-output.txt worldgen-difference.txt
  rm -rf ./worldgen-output
}

game="$1"
if [[ "$game" = "" ]]; then
  echo "Usage: $0 <game>.  Test failed."
  exit 1
fi

executable="../$game/$game"
[ -f "${executable}" ] || executable="../build/$game"

if [[ ! -f "${executable}" ]]; then
  echo "Please build the $game executable before running the world generation snapshot.  Test failed."
  exit 1
fi

if [[ ! -d "worldgen/$game" ]]; then
  echo "No recorded world for $game.  Test failed."
  exit 1
fi

cp "${executable}" "./$game"
chmod +x "./$game"

echo -n "Regenerating the $game world..."

seed=$(<"worldgen/$game/seed")

# The generated world lands in the working directory, so it is built in a subdirectory of its own
# and compared there -- the turn runners use the same directory and would otherwise collide.
rm -rf ./worldgen-output
mkdir -p ./worldgen-output
if ! (cd ./worldgen-output && ATLANTIS_SEED="$seed" "../$game" new < "../worldgen/$game/answers" \
        &> engine-output.txt) ; then
  echo "executable crashed. -- Test failed."
  cat ./worldgen-output/engine-output.txt
  cleanup "${game}"
  exit 1
fi

if ! diff -ur "worldgen/$game/output" ./worldgen-output &> worldgen-difference.txt ; then
  echo "output differed. -- Test failed."
  cat worldgen-difference.txt
  cleanup "${game}"
  exit 1
fi

echo "identical. -- Test succeeded."
cleanup "${game}"
exit 0
