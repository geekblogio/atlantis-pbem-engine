#!/bin/bash

# neworigins8 is deliberately absent: it shares all world generation code with neworigins and
# produces the same bytes, so recording it duplicates coverage rather than adding it -- the same
# reasoning the divergence register gives for leaving out its turn fixtures.
for game in standard basic fracas havilah kingdoms neworigins rimefall; do
  ./update-worldgen-snapshot.sh "$game"
done

exit 0
