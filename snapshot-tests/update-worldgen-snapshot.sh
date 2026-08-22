#!/bin/bash
#
# Re-records the world `run-worldgen-snapshot.sh` compares against. Read docs/snapshot-tests.md
# before using it: a re-record is a statement that the change to world generation was intended.

# The engine asks its world size questions in a loop that never checks for end of input, so an
# answers file with one line too few does not fail -- it spins at full speed for ever. Bound the
# run rather than hand CI a job that only the six hour limit can stop. `timeout` is not on macOS,
# so this is done by hand.
LIMIT_SECONDS=120

# $1 is the file to feed the command on stdin. It has to be named here rather than redirected onto
# the call: a background job in a non-interactive shell reads /dev/null unless told otherwise, and
# the engine would then hit end of input and spin.
run_bounded()
{
  local stdin_file="$1"
  shift
  "$@" < "$stdin_file" &
  local pid=$!
  local waited=0
  while kill -0 "$pid" 2>/dev/null; do
    if [[ $waited -ge $LIMIT_SECONDS ]]; then
      kill -9 "$pid" 2>/dev/null
      wait "$pid" 2>/dev/null
      return 124
    fi
    sleep 1
    waited=$((waited + 1))
  done
  wait "$pid"
}

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

export ATLANTIS_SEED
ATLANTIS_SEED=$(<"worldgen/$game/seed")

rm -rf ./worldgen-output
mkdir -p ./worldgen-output
cd ./worldgen-output || exit 1
run_bounded "../worldgen/$game/answers" "../$game" new &> engine-output.txt
status=$?
cd .. || exit 1

if [[ $status = 124 ]]; then
  echo "did not finish within ${LIMIT_SECONDS}s -- is worldgen/$game/answers complete? -- Not updated."
  rm -rf ./worldgen-output
  rm -f "./$game"
  exit 1
fi

if [[ $status != 0 ]]; then
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
