// nlohmann::json for the JSON report (report.<n>.json, `map hex`). Every engine
// file gets it through this header so the configuration below is the same in
// every translation unit.
#ifndef ATL_JSON_H
#define ATL_JSON_H

// No std::filesystem support: json.hpp would otherwise #include <filesystem>
// into nearly every file (faction.h is everywhere), and its std::distance
// overloads, under the engine's file-wide `using namespace std` (astring.h),
// make unqualified calls such as distance({0, 0}, {0, 4}) to aregion.cpp's
// distance() (unittest/aregion_mapgen_test.cpp has them)
// ambiguous. The report never serialises a path.
#define JSON_HAS_FILESYSTEM 0
#define JSON_HAS_EXPERIMENTAL_FILESYSTEM 0
#include "external/nlohmann/json.hpp"

using json = nlohmann::json;

#endif
