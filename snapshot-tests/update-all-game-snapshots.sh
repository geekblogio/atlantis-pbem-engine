#!/bin/bash

echo "Updating standard snapshots"
./update-game-snapshots.sh
echo "Updating neworigin snapshots"
./update-game-snapshots.sh neworigins
echo "Updating rimefall snapshots"
./update-game-snapshots.sh rimefall

exit 0
