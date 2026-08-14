// Rimefall starts as a skeleton over NewOrigins and diverges one file at a time.
// Stage 2 replaces this shim: Game::CreateWorld drops the generator prompt and the underworld,
// underdeep, abyss and shaft blocks, and ARegionList::GetRegType becomes a monotonic north-to-south
// band index instead of a latitude fold symmetric about the equator. See docs/decisions/0010.
#include "../neworigins/world.cpp"
