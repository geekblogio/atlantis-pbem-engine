#!/bin/bash
#
# Re-records the world `run-worldgen-snapshot.sh` compares against. Read docs/snapshot-tests.md
# before using it: a re-record is a statement that the change to world generation was intended.

game="$1"
if [[ "$game" = "" ]]; then
  echo "Usage: $0 <game>.  Not updated."
  exit 1
fi

executable="../$game/$game"
[ -f "${executable}" ] || executable="../build/$game"

if [[ ! -f "${executable}" ]]; then
  echo "Please build the $game executable before updating the world generation snapshot.  Not updated."
  exit 1
fi

if [[ ! -d "worldgen/$game" ]]; then
  echo "No recorded world for $game -- create worldgen/$game/{answers,seed} first.  Not updated."
  exit 1
fi

cp "${executable}" "./$game"
chmod +x "./$game"

echo -n "Regenerating the $game world..."

seed=$(<"worldgen/$game/seed")

rm -rf ./worldgen-output
mkdir -p ./worldgen-output
if ! (cd ./worldgen-output && ATLANTIS_SEED="$seed" "../$game" new < "../worldgen/$game/answers" \
        &> engine-output.txt) ; then
  echo "executable crashed. -- Not updated."
  cat ./worldgen-output/engine-output.txt
  rm -rf ./worldgen-output
  rm -f "./$game"
  exit 1
fi

rm -rf "worldgen/$game/output"
mv ./worldgen-output "worldgen/$game/output"
echo "-- Updated."
rm -f "./$game"
exit 0
