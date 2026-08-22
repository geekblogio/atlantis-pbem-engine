#!/bin/bash
#
# Replays WORLD CREATION, which the turn snapshots cannot reach.
#
# `run-game-snapshots.sh` replays `<game> run` against a world that already exists, so every line
# of world generation -- terrain, towns, city guards, starting locations -- is exercised by
# nothing. That is not a theoretical gap: `#66` fixed a defect that only ever showed there, in
# `kingdoms`, the one ruleset whose guards are made by MakeManUnit.
#
# A recorded world is cheap. `worldgen/<game>/` holds the answers the ruleset's `new` asks for on
# stdin, the seed, and the files it wrote; every recorded world is under 550 KB and none takes a
# second.

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

cleanup()
{
  game="$1"
  rm -f "${game}"
  rm -f worldgen-difference.txt
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

# Exported rather than prefixed onto the call: a `VAR=value func` prefix on a shell FUNCTION is
# unspecified by POSIX, and bash keeps the assignment afterwards.
export ATLANTIS_SEED
ATLANTIS_SEED=$(<"worldgen/$game/seed")

# The generated world lands in the working directory, so it is built in a subdirectory of its own
# and compared there -- the turn runners use this directory too and would otherwise collide.
rm -rf ./worldgen-output
mkdir -p ./worldgen-output
cd ./worldgen-output || exit 1
run_bounded "../worldgen/$game/answers" "../$game" new &> engine-output.txt
status=$?
cd .. || exit 1

if [[ $status = 124 ]]; then
  echo "did not finish within ${LIMIT_SECONDS}s -- is worldgen/$game/answers complete? -- Test failed."
  cleanup "${game}"
  exit 1
fi

if [[ $status != 0 ]]; then
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
