#include <stdio.h>
#include <string.h>
#include "game.h"
#include "gamedata.h"
#include "rng.hpp"
#include "rimefall.h"

#include <algorithm>
#include <list>
#include <unordered_set>
#include <stack>
#include <random>

enum ZoneType {
    UNDECIDED,  // zone type not yet determined
    OCEAN,      // no land will be generated in this zone
    CONTINENT,  // normal land will grow
    STRAIT      // ocean, but formed at two contient border area
    // ARCHIPELAGO  // another round of smaller zone generation will be executed to make many smaller islands
};

struct Coords {
    int x;
    int y;

    int Distance(const Coords& other);
};

int hexDistance(const Coords& a, const Coords& b) {
    int qA = a.x;
    int rA = (a.y - a.x) / 2;
    int sA = -qA - rA;

    int qB = b.x;
    int rB = (b.y - b.x) / 2;
    int sB = -qB - rB;

    int q = qA - qB;
    int r = rA - rB;
    int s = sA - sB;

    int d = (std::abs(q) + std::abs(r) + std::abs(s)) / 2;
    return d;
}

int Coords::Distance(const Coords& other) {
    return hexDistance(*this, other);
}

struct Zone;
struct Province;

struct ZoneRegion {
    int id;
    Coords location;
    bool exclude = false;
    int biome = -1;
    Zone *zone = nullptr;
    Province *province = nullptr;
    ARegion* region = nullptr;
    ZoneRegion *neighbors[NDIRS];

    bool HaveBorderWith(Zone* zone);
    bool HaveBorderWith(ZoneRegion* other);
    bool IsInner();
    bool IsOuter();
    bool AtBorderWith(ZoneType type);
    std::vector<Zone *> GetNeihborZones();
    int CountNeighbors(Zone* zone);
    int CountNeighbors(Province* province);
    std::map<int, int> CountNeighborBiomes();
};

std::map<int, int> ZoneRegion::CountNeighborBiomes() {
    std::map<int, int> stats;
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n == NULL) continue;

        if (stats.find(n->biome) == stats.end()) {
            stats[n->biome] = 1;
        }
        else {
            stats[n->biome] += 1;
        }
    }

    return stats;
}

int ZoneRegion::CountNeighbors(Zone* zone) {
    int count = 0;
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n != NULL && n->zone == zone) count++;
    }

    return count;
}

int ZoneRegion::CountNeighbors(Province* province) {
    int count = 0;
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n != NULL && n->province == province) count++;
    }

    return count;
}

std::vector<Zone *> ZoneRegion::GetNeihborZones() {
    std::vector<Zone *> items;
    items.reserve(NDIRS);

    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n == NULL) continue;
        if (n->zone == zone) continue;

        auto z = n->zone;
        for (size_t j = 0; j < items.size(); j++) {
            if (items[j] == z) {
                z = NULL;
                break;
            }
        }

        if (z != NULL) items.push_back(z);
    }

    return items;
}

bool ZoneRegion::HaveBorderWith(Zone* zone) {
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n == NULL) continue;
        if (n->zone == zone) return true;
    }

    return false;
}

bool ZoneRegion::HaveBorderWith(ZoneRegion* other) {
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n == NULL) continue;
        if (n == other) return true;
    }

    return false;
}

bool ZoneRegion::IsInner() {
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n == NULL) continue;
        if (n->zone != this->zone) return false;
    }

    return true;
}

bool ZoneRegion::IsOuter() {
    return !IsInner();
}

struct Province {
    int id;
    int h;
    int biome = -1;
    Zone* zone = nullptr;
    std::map<int, ZoneRegion *> regions;

    Coords GetLocation();
    void AddRegion(ZoneRegion* region);
    void RemoveRegion(ZoneRegion* region);
    bool Grow();
    int GetLatitude();
    int GetSize();
    std::unordered_set<Province *> GetNeighbors();
    std::unordered_set<int> GetNeighborBiomes();
};


std::unordered_set<Province *> Province::GetNeighbors() {
    std::unordered_set<Province *> items;

    for (auto &kv : this->regions) {
        auto reg = kv.second;

        for (int i = 0; i < NDIRS; i++) {
            auto n = reg->neighbors[i];
            if (n == NULL) continue;

            if (n->zone == this->zone && n->province != this) {
                items.insert(n->province);
            }
        }
    }

    return items;
}

std::unordered_set<int> Province::GetNeighborBiomes() {
    std::unordered_set<int> biomes;

    auto tmp = this->GetNeighbors();
    for (auto &n : tmp) {
        if (n->biome != -1) biomes.insert(n->biome);
    }

    return biomes;
}

Coords Province::GetLocation() {
    // xMin doubles as the "first region seen" sentinel. The other three are only
    // written on that first iteration, so a province with no regions at all would
    // otherwise build its return value out of four uninitialised ints.
    int xMin = -1;
    int yMin = -1;
    int xMax = -1;
    int yMax = -1;

    for (auto &kv : regions) {
        auto reg = kv.second;

        if (xMin == -1) {
            xMin = reg->location.x;
            yMin = reg->location.y;
            xMax = xMin;
            yMax = yMin;

            continue;
        }

        xMin = std::min(xMin, reg->location.x);
        yMin = std::min(yMin, reg->location.y);
        xMax = std::max(xMax, reg->location.x);
        yMax = std::max(yMax, reg->location.y);
    }

    return {
        .x = xMin + (xMax - xMin + 1) / 2,
        .y = yMin + (yMax - yMin + 1) / 2
    };
}

int Province::GetLatitude() {
    auto loc = GetLocation();
    int lat = ( loc.y * 8 ) / this->h;
    if (lat > 3) {
        lat = std::max(0, 7 - lat);
    }

    return lat;
}

int Province::GetSize() {
    return this->regions.size();
}

void Province::AddRegion(ZoneRegion* region) {
    region->province = this;
    this->regions.insert(std::make_pair(region->id, region));
}

void Province::RemoveRegion(ZoneRegion* region) {
    if (region->province == this) {
        region->province = NULL;
    }

    this->regions.erase(region->id);
}

bool Province::Grow() {
    std::vector<ZoneRegion *> candidates;
    for (auto &kv : regions) {
        auto reg = kv.second;

        for (int i = 0; i < NDIRS; i++) {
            auto n = reg->neighbors[i];
            if (n == NULL) continue;
            if (n->zone != this->zone) continue;
            if (n->province != NULL) continue;

            candidates.push_back(n);
        }
    }

    if (candidates.size() == 0) return false;

    ZoneRegion* next = candidates[rng::get_random(candidates.size())];
    int connections = next->CountNeighbors(this);

    // 2d6
    int roll = rng::make_roll(2, 6);
    int diff = 12 - connections;

    // connections: 5, diff: 7, prob: 58%
    // connections: 4, diff: 8, prob: 41%
    // connections: 3, diff: 9, prob: 27%
    // connections: 2, diff: 10, prob: 16%
    // connections: 1, diff: 11, prob: 8%

    if (GetSize() == 1 || roll >= diff) {
        AddRegion(next);
    }

    return true;
}


struct Zone {
    ~Zone();

    int id;
    Coords location;
    ZoneType type;
    std::map<int, Zone *> neighbors;
    std::map<int, ZoneRegion *> regions;
    std::map<int, Province *> provinces;

    void RemoveRegion(ZoneRegion* region);
    void AddRegion(ZoneRegion* region);
    void SetConnections();
    std::unordered_set<ZoneRegion *> TraverseRegions();
    bool CheckZoneIntegerity();
    bool AtBorderWith(Zone* zone);
    void RemoveNeighbor(Zone* zone);
    void AddNeighbor(Zone* zone);

    bool IsIsland();

    Province* CreateProvince(ZoneRegion* region, int h);
    void RemoveProvince(Province* province);
};

Zone::~Zone() {
    for (auto &kv : provinces) {
        delete kv.second;
    }
}

Province* Zone::CreateProvince(ZoneRegion* region, int h) {
    Province* province = new Province();
    province->id = this->provinces.size();
    province->zone = this;
    province->h = h;

    province->AddRegion(region);
    this->provinces.insert(std::make_pair(province->id, province));

    return province;
}

void Zone::RemoveProvince(Province* province) {
    if (province->zone == this) {
        province->zone = NULL;
    }

    this->provinces.erase(province->id);

    delete province;
}

bool ZoneRegion::AtBorderWith(ZoneType type) {
    for (int i = 0; i < NDIRS; i++) {
        auto n = this->neighbors[i];
        if (n == NULL) continue;
        if (n->zone != this->zone && n->zone->type == type) return true;
    }

    return false;
}

bool Zone::IsIsland() {
    if (this->type != ZoneType::CONTINENT) return false;

    for (auto &kv : this->neighbors) {
        auto zone = kv.second;
        if (zone->type == ZoneType::CONTINENT) return false;
    }

    return true;
}

void Zone::RemoveRegion(ZoneRegion* region) {
    if (region->zone == this) {
        region->zone = NULL;
    }

    this->regions.erase(region->id);
}

void Zone::AddRegion(ZoneRegion* region) {
    if (region == NULL) {
        logger::write("Region cannot be NULL");
        exit(1);
    }

    region->zone = this;
    this->regions.insert(std::make_pair(region->id, region));

    if (this->regions.size() == 1) {
        this->location = region->location;
    }
}

void Zone::RemoveNeighbor(Zone* zone) {
    zone->neighbors.erase(this->id);
    this->neighbors.erase(zone->id);
}

void Zone::AddNeighbor(Zone* zone) {
    zone->neighbors.insert(std::make_pair(this->id, this));
    this->neighbors.insert(std::make_pair(zone->id, zone));
}

void Zone::SetConnections() {
    std::unordered_set<Zone *> visited;

    for (auto &region : this->regions) {
        for (auto &n : region.second->neighbors) {
            if (n == NULL) continue;
            if (n->zone == this) continue;

            if (n->zone == NULL) {
                logger::write("Region zone cannot be NULL");
                exit(1);
            }

            visited.insert(n->zone);
        }
    }

    this->neighbors.clear();
    for (auto &z : visited) {
        AddNeighbor(z);
    }
}

std::unordered_set<ZoneRegion *> Zone::TraverseRegions() {
    std::unordered_set<ZoneRegion *> v;
    std::stack<ZoneRegion *> s;

    if (this->regions.size() == 0) return v;

    s.push(this->regions.begin()->second);
    while (!s.empty()) {
        ZoneRegion* reg = s.top();
        s.pop();
        v.insert(reg);

        for (int i = 0; i < NDIRS; i++) {
            auto n = reg->neighbors[i];
            if (n == NULL) continue;
            if (n->zone != this) continue;
            if (v.find(n) != v.end()) continue;

            s.push(n);
        }
    }

    return v;
}

bool Zone::CheckZoneIntegerity() {
    if (this->regions.size() == 0) {
        return true;
    }

    auto v = this->TraverseRegions();

    return v.size() == this->regions.size();
}

bool Zone::AtBorderWith(Zone* zone) {
    for (auto &z : zone->neighbors) {
        if (this == z.second) return true;
    }

    return false;
}

class MapBuilder {
public:
    MapBuilder(ARegionArray* aregs);
    ~MapBuilder();

    size_t maxZones;
    int gapMin;
    int gapMax;
    int volcanoesMin;
    int volcanoesMax;
    int lakesMin;
    int lakesMax;

    int w;
    int h;
    std::map<int, Zone *> zones;
    std::vector<ZoneRegion *> regions;
    size_t maxContinentArea;

    ZoneRegion* GetRegion(int x, int y);
    ZoneRegion* GetRegion(Coords location);
    void CreateZones(int minDistance, int maxAtempts);
    void GrowZones();
    void SpecializeZones(size_t continents, int continentAreaFraction);
    void GrowTerrain();
    void GrowLandInZone(Zone* zone);
    void SetOceanNames();
    void ClearEmptyZones();
    void MergeZoneInto(Zone* src, Zone* dest);
    void SplitZone(Zone* zone);
    void Clear();
    void ConnectZones();
    Zone* CreateZone(ZoneType type);
    Zone* GetNotIsland();
    Zone* GetZoneOfMaxSize(ZoneType type, size_t maxSize);
    void AddVolcanoes();
    void AddLakes();

private:
    int lastZoneId;
};

int EstimateMaxZones(int area, int radius) {
    int zoneArea = (1 + (12 * radius + 6 * (radius - 1) * radius) / 2);
    int maxZones = area / zoneArea;

    return maxZones;
}

int GetRegionIndex(const int x, const int y, const int w, const int h) {
    int xx = (x + w) % w;
    int yy = (y + h) % h;

    if ((xx + yy) % 2) {
        return -1;
    }

    int i = xx / 2 + yy * w / 2;

    return i;
}

Zone* MapBuilder::GetNotIsland() {
    for (auto &kv : this->zones) {
        auto zone = kv.second;
        if (zone->type != ZoneType::CONTINENT) continue;

        if (!zone->IsIsland()) {
            return zone;
        }
    }

    return NULL;
}

Zone* MapBuilder::GetZoneOfMaxSize(ZoneType type, size_t maxSize) {
    for (auto &kv : this->zones) {
        auto zone = kv.second;
        if (zone->type != type) continue;

        if (zone->regions.size() <= maxSize) {
            return zone;
        }
    }

    return NULL;
}

MapBuilder::MapBuilder(ARegionArray* aregs) {
    this->w = aregs->x;
    this->h = aregs->y;
    this->lastZoneId = 0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!((x + y) % 2)) {
                ZoneRegion *reg = new ZoneRegion();
                reg->id = GetRegionIndex(x, y, this->w, this->h);
                reg->location.x = x;
                reg->location.y = y;
                reg->region = aregs->GetRegion(x, y);

                if (reg->location.x != reg->region->xloc || reg->location.y != reg->region->yloc) {
                    logger::write("Region location do not match");
                    exit(1);
                }

                this->regions.push_back(reg);
            }
        }
    }

    for (auto &item : this->regions) {
        ARegion* areg = item->region;
        for (int i = 0; i < NDIRS; i++) {
            ARegion* nreg = areg->neighbors[i];

            item->neighbors[i] = nreg == NULL
                ? NULL
                : this->GetRegion(nreg->xloc, nreg->yloc);
        }
    }
}

Zone* MapBuilder::CreateZone(ZoneType type) {
    Zone* zone = new Zone();
    zone->id = this->lastZoneId++;
    zone->type = type;

    this->zones.insert(std::make_pair(zone->id, zone));

    return zone;
}

void MapBuilder::Clear() {
    for (auto &region : this->regions) {
        delete region;
    }

    for (auto &zone : this->zones) {
        delete zone.second;
    }

    this->regions.clear();
    this->zones.clear();
}

MapBuilder::~MapBuilder() {
    Clear();
}

void MapBuilder::ConnectZones() {
    for (auto &zone : this->zones) {
        zone.second->SetConnections();
    }
}

ZoneRegion* MapBuilder::GetRegion(const int x, const int y) {
    return this->regions[GetRegionIndex(x, y, this->w, this->h)];
}

ZoneRegion* MapBuilder::GetRegion(Coords location) {
    return this->GetRegion(location.x, location.y);
}

void MapBuilder::ClearEmptyZones() {
    int count = 0;
    std::vector<int> keys;
    for (auto &z : this->zones) {
        keys.push_back(z.first);
    }

    for (auto &key : keys) {
        Zone* zone = this->zones[key];
        if (zone->regions.size() == 0) {
            delete zone;
            this->zones.erase(key);
            count++;
        }
    }

    logger::write("Removed " + std::to_string(count) + " empty zones");
}

void MapBuilder::CreateZones(int minDistance, int maxAtempts) {
    logger::write("Create zones");

    int attempts = 0;
    while (this->zones.size() < this->maxZones && attempts++ < maxAtempts) {
        Coords location = {
            .x = rng::get_random(this->w),
            .y = rng::get_random(this->h)
        };

        if ((location.x + location.y) % 2) continue;

        int distance = w + h;
        for (auto &kv : this->zones) {
            auto zone = kv.second;
            distance = std::min(distance, hexDistance(zone->location, location));
            if (distance < minDistance) break;
        }

        if (distance < minDistance) continue;

        Zone* z = CreateZone(ZoneType::UNDECIDED);
        ZoneRegion* r = this->GetRegion(location);
        z->AddRegion(r);

        attempts = 0;
    }
}

void MapBuilder::GrowZones() {
    logger::write("Grow zones");

    std::unordered_set<Zone *> v;

    // run loop while there are regions not assigned to any zone
    std::vector<ZoneRegion *> nextRegions;
    while (this->zones.size() != v.size()) {
        for (auto &kv : this->zones) {
            auto zone = kv.second;
            if (v.find(zone) != v.end()) {
                // already processed zone
                continue;
            }

            // find all regions where zone can grow
            nextRegions.clear();
            for (auto &kv2 : zone->regions) {
                auto region = kv2.second;
                for (auto &n : region->neighbors) {
                    if (n == NULL) continue;
                    if (n->zone != NULL) continue;

                    nextRegions.push_back(n);
                }
            }

            if (nextRegions.size() == 0) {
                v.insert(zone);
                continue;
            }

            // select one region to grow
            ZoneRegion* next = nextRegions[rng::get_random(nextRegions.size())];
            int connections = 0;
            for (auto &n : next->neighbors) {
                if (n != NULL && n->zone == zone) {
                    connections++;
                }
            }

            // 2d6
            int roll = rng::make_roll(2, 6);
            int diff = 12 - connections;

            // connections: 5, diff: 7, prob: 58%
            // connections: 4, diff: 8, prob: 41%
            // connections: 3, diff: 9, prob: 27%
            // connections: 2, diff: 10, prob: 16%
            // connections: 1, diff: 11, prob: 8%

            if (zone->regions.size() == 1 || roll >= diff) {
                zone->AddRegion(next);
            }
        }
    }

    ClearEmptyZones();
    ConnectZones();
}

void MapBuilder::MergeZoneInto(Zone* src, Zone* dest) {
    if (src == NULL) {
        logger::write("src cannot be null");
        exit(1);
    }

    if (dest == NULL) {
        logger::write("dest cannot be null");
        exit(1);
    }

    if (!src->AtBorderWith(dest)) {
        logger::write("Zones must have common border");
        exit(1);
    }

    std::unordered_set<Zone *> conn;
    for (auto &kv : src->regions) {
        auto reg = kv.second;

        auto zones = reg->GetNeihborZones();
        for (auto &z : zones) {
            conn.insert(z);
        }

        dest->AddRegion(kv.second);
    }

    src->regions.clear();
    src->neighbors.clear();

    dest->SetConnections();
    for (auto &z : conn) {
        z->SetConnections();
    }
}

void MapBuilder::SplitZone(Zone* zone) {
    if (zone == NULL) {
        logger::write("Cannot split NULL zone");
        exit(1);
    }

    auto blob = zone->TraverseRegions();
    while (zone->regions.size() != blob.size()) {
        Zone* newZone = CreateZone(zone->type);

        for (auto &region : blob) {
            zone->RemoveRegion(region);
            newZone->AddRegion(region);
        }

        for (auto &z : zone->neighbors) {
            z.second->SetConnections();
        }

        zone->SetConnections();
        newZone->SetConnections();

        blob = zone->TraverseRegions();
    }
}

Zone* FindConnectedContinent(Zone* zone) {
    logger::write("Searching for continent");
    for (auto &kv : zone->neighbors) {
        auto n = kv.second;
        if (n->type == ZoneType::CONTINENT) {
            return n;
        }
    }

    return NULL;
}

std::vector<ZoneRegion *> FindBorderRegions(Zone* zone, Zone* borderZone, const int depth, const bool useRandom = false) {
    std::unordered_set<ZoneRegion *> visited;

    const int diff = 3;

    std::vector<ZoneRegion *> v;
    for (int i = 0; i < depth; i++) {
        if (i == 0) {
            for (auto &kv : zone->regions) {
                auto &region = kv.second;
                if (!region->HaveBorderWith(borderZone)) continue;

                if (useRandom && depth == 1) {
                    int roll = rng::make_roll(1, 5);
                    if (roll < diff) continue;
                }

                visited.insert(region);
            }

            continue;
        }

        v.clear();
        for (auto &region : visited) {
            for (auto &next : region->neighbors) {
                if (next == NULL) continue;
                if (next->zone != zone) continue;
                if (visited.find(next) != visited.end()) continue;

                if (i == depth - 1) {
                    int roll = rng::make_roll(1, 5);
                    if (roll < diff) continue;
                }

                v.push_back(region);
            }
        }

        for (auto &region : v) {
            visited.insert(region);
        }
    }

    std::vector<ZoneRegion *> list;
    list.reserve(visited.size());
    for (auto &region : visited) {
        list.push_back(region);
    }

    return list;
}

void MapBuilder::SpecializeZones(size_t continents, int continentAreaFraction) {
    logger::write("Specialize zones");

    int maxArea = (this->w * this->h * continentAreaFraction) / 200;
    int attempts = 0;

    logger::write("Place continent cores");
    std::vector<Zone *> cores;
    attempts = 0;
    while (attempts++ < 1000 && cores.size() != continents) {
        Zone* candidate = this->zones[rng::get_random(this->zones.size())];

        for (auto &core : cores) {
            if (core == candidate || core->AtBorderWith(candidate)
                || core->location.Distance(candidate->location) < 8) {
                candidate = NULL;
                break;
            }
        }

        if (candidate != NULL) {
            cores.push_back(candidate);
            attempts = 0;
        }
    }

    int coveredArea = 0;
    std::vector<Zone *> S;
    for (auto &zone : cores) {
        zone->type = ZoneType::CONTINENT;
        coveredArea += zone->regions.size();
        S.push_back(zone);
    }

    logger::write("Grow continents");
    attempts = 0;
    std::vector<Zone *> next;
    while (attempts++ < 10000 && coveredArea <= maxArea && S.size() > 0) {
        int i = rng::get_random(S.size());
        Zone* zone = S[i];

        if (zone->regions.size() > maxContinentArea) {
            S.erase(S.begin() + i);
            continue;
        }

        next.clear();
        for (auto &nz : zone->neighbors) {
            Zone* nextZone = nz.second;
            if (nextZone->type != ZoneType::UNDECIDED) continue;
            next.push_back(nextZone);
        }

        if (next.size() == 0) {
            S.erase(S.begin() + i);
            continue;
        }

        Zone* target = next[rng::get_random(next.size())];

        coveredArea += target->regions.size();
        MergeZoneInto(target, zone);
    }

    ClearEmptyZones();

    std::vector<Zone *> oceans;
    for (auto &kv : this->zones) {
        auto zone = kv.second;
        if (zone->regions.size() != 0 && zone->type == ZoneType::UNDECIDED) {
            oceans.push_back(zone);
        }
    }

    logger::write("Add gaps between continents");
    Zone* nonIsland = GetNotIsland();
    while (nonIsland != NULL) {
        logger::write("Starting border cleanup ");
        Zone* otherZone = FindConnectedContinent(nonIsland);

        int depthRoll = rng::make_roll(1, this->gapMax - this->gapMin) + this->gapMin;
        int randomRoll = rng::make_roll(1, 2);
        int sideRoll = rng::make_roll(1, 2);

        int sideADepth;
        int sideBDepth;
        if (sideRoll == 1) {
            sideADepth = depthRoll / 2;
            sideBDepth = depthRoll - sideADepth;
        }
        else {
            sideBDepth = depthRoll / 2;
            sideADepth = depthRoll - sideBDepth;
        }

        bool sideARandom = false;
        bool sideBRandom = false;
        if (depthRoll == 1) {
            if (sideADepth == 0) {
                sideADepth = 1;
                sideARandom = true;
            }
            else {
                sideBDepth = 1;
                sideBRandom = true;
            }
        }
        else if (depthRoll == 2) {
            sideBRandom = depthRoll == 2 && randomRoll == 1;
            sideARandom = depthRoll == 2 && randomRoll == 2;
        }

        // find regions that are on border
        Zone* strait = CreateZone(ZoneType::STRAIT);
        std::list<ZoneRegion *> list;
        if (sideADepth) {
            auto tmp = FindBorderRegions(nonIsland, otherZone, sideADepth, sideARandom);
            for (auto &r : tmp) {
                list.push_back(r);
            }
        }

        if (sideBDepth) {
            auto tmp = FindBorderRegions(otherZone, nonIsland, sideBDepth, sideBRandom);
            for (auto &r : tmp) {
                list.push_back(r);
            }
        }

        for (auto &r : list) {
            nonIsland->RemoveRegion(r);
            otherZone->RemoveRegion(r);
            strait->AddRegion(r);
        }

        SplitZone(strait);

        if (sideADepth > 0) SplitZone(nonIsland);
        if (sideBDepth > 0) SplitZone(otherZone);

        ConnectZones();
        nonIsland = GetNotIsland();
    }

    ClearEmptyZones();

    logger::write("Grow oceans");
    int oceanCores = std::max(1, (int) oceans.size() / 4);

    rng::shuffle(oceans);

    oceans.resize(oceanCores);
    for (auto &zone : oceans) {
        zone->type = ZoneType::OCEAN;
    }

    attempts = 0;
    while (oceans.size() > 0) {
        int i = rng::get_random(oceans.size());
        Zone* ocean = oceans[i];

        std::vector<Zone *> next;
        for (auto &kv : ocean->neighbors) {
            auto zone = kv.second;
            if (zone->type == ZoneType::UNDECIDED) {
                next.push_back(zone);
            }
        }

        if (next.size() > 0) {
            MergeZoneInto(next[rng::get_random(next.size())], ocean);
        }
        else {
            oceans.erase(oceans.begin() + i);
        }
    }

    ClearEmptyZones();
    ConnectZones();


    logger::write("Cleanup oceans, islands and straits");

    std::stack<Zone *> toCleanUp;
    for (auto &kv : this->zones) {
        auto zone = kv.second;
        int size = zone->regions.size();

        switch (zone->type) {
            case ZoneType::CONTINENT:
            case ZoneType::OCEAN:
                if (size <= 3) toCleanUp.push(zone);
                break;
            case ZoneType::STRAIT:
                toCleanUp.push(zone);
                break;
            case ZoneType::UNDECIDED:
                logger::write("An unexpected situation occured.  zone type was UNDECIDED. Ignoring/skipping it.");
                break;
        }
    }

    while (!toCleanUp.empty()) {
        Zone* src = toCleanUp.top();
        size_t size = src->regions.size();

        toCleanUp.pop();

        if (size == 0) continue;

        switch (src->type) {
            case ZoneType::CONTINENT: {
                for (auto &kv : src->neighbors) {
                    auto n = kv.second;
                    if (n->type == ZoneType::CONTINENT) continue;

                    // merge into nearest ocean or strait
                    MergeZoneInto(src, n);
                    break;
                }
                break;
            }

            case ZoneType::OCEAN: {
                size_t roll = rng::make_roll(3, 12);
                if (size > roll) break;

                for (auto &kv : src->neighbors) {
                    auto n = kv.second;
                    if (n->type != ZoneType::OCEAN) continue;

                    // merge into nearest ocean
                    MergeZoneInto(src, n);
                    break;
                }

                break;
            }

            case ZoneType::STRAIT: {
                size_t maxStraitSize = 12 + rng::make_roll(1, 6) - 3;
                if (size > maxStraitSize) continue;

                std::vector<Zone *> otherStraits;
                for (auto &kv : src->neighbors) {
                    auto n = kv.second;
                    if (n->type != ZoneType::STRAIT) continue;
                    if (n->regions.size() > maxStraitSize) continue;

                    // merge surrounding straits into src
                    otherStraits.push_back(n);
                }

                for (auto &n : otherStraits) {
                    if (src->regions.size() > maxStraitSize) break;

                    MergeZoneInto(n, src);
                }

                break;
            }

            case ZoneType::UNDECIDED: {
                logger::write("An unexpected situation occured.  zone type was UNDECIDED. Ignoring/skipping it.");
                break;
            }
        }
    }

    ClearEmptyZones();

    // make straits surounded only by water to be ZoneType::OCEAN
    for (auto &kv : this->zones) {
        auto zone = kv.second;
        if (zone->type != ZoneType::STRAIT) continue;

        bool skip = false;
        for (auto &n : zone->neighbors) {
            if (n.second->type != ZoneType::STRAIT && n.second->type != ZoneType::OCEAN) {
                skip = true;
                break;
            }
        }

        if (skip) continue;

        zone->type = ZoneType::OCEAN;
    }
}

const int BIOME_ZONES = 4;
const int MAX_BIOMES = 8;
const int BIOMES[BIOME_ZONES][MAX_BIOMES] = {
    // 0: Arctic regions
    { 4, R_TUNDRA, R_MOUNTAIN, R_FOREST, R_PLAIN, -1, -1, -1 },

    // 1: Colder regions
    { 5, R_TUNDRA, R_MOUNTAIN, R_FOREST, R_PLAIN, R_SWAMP, -1, -1 },

    // 2: Warmer regions
    { 7, R_TUNDRA, R_MOUNTAIN, R_FOREST, R_PLAIN, R_SWAMP, R_JUNGLE, R_DESERT },

    // 3: Tropical regions
    { 5, R_MOUNTAIN, R_PLAIN, R_SWAMP, R_JUNGLE, R_DESERT, -1, -1 }
};

void MapBuilder::AddVolcanoes() {
    logger::write("Adding volcanoes");
    // volcanos will be added anywhere

    size_t count = rng::make_roll(1, this->volcanoesMax - this->volcanoesMin) + this->volcanoesMin;
    int distance = (std::min(this->w, this->h / 2) * 2) / count + 2;

    int cols = count / rng::make_roll(1, 4) + 1;
    int rows = count / cols + 1;
    int dX = this->w / cols;
    int dY = this->h / rows;

    std::vector<ZoneRegion *> candidates;
    std::vector<Coords> volcanoes;
    volcanoes.reserve(cols * rows);

    for (int c = 0; c < cols && volcanoes.size() < count; c++) {
        for (int r = 0; r < rows && volcanoes.size() < count; r++) {
            int attempts = 1000;
            while (attempts-- > 0) {
                int x = c * dX + rng::get_random(dX);
                int y = r * dY + rng::get_random(dY);
                if ((x + y) % 2) continue;

                attempts = 0;

                ZoneRegion* seed = this->GetRegion(x, y);
                for (size_t i = 0; i < volcanoes.size(); i++) {
                    int d = volcanoes[i].Distance(seed->location);
                    if (d < distance) {
                        seed = NULL;
                        break;
                    }
                }
                if (seed == NULL) continue;

                volcanoes.push_back(seed->location);

                int volcano = rng::make_roll(1, 3) - 1;
                int mountains = rng::make_roll(2, 3);

                Zone* zone = this->CreateZone(ZoneType::CONTINENT);
                seed->biome = R_VOLCANO;
                seed->region->type = R_VOLCANO;
                seed->region->wages = AGetName(0, seed->region);

                seed->zone->RemoveRegion(seed);
                zone->AddRegion(seed);

                while (volcano > 0 || mountains > 0) {
                    candidates.clear();
                    for (auto &kv : zone->regions) {
                        auto region = kv.second;

                        if (region->IsInner()) continue;

                        for (int i = 0; i < NDIRS; i++) {
                            auto n = region->neighbors[i];
                            if (n == NULL) continue;
                            if (n->zone == zone) continue;

                            candidates.push_back(n);
                        }
                    }

                    size_t regionSize = zone->regions.size();
                    for (int i = 0; regionSize == zone->regions.size(); i++) {
                        auto &next = candidates[i % candidates.size()];
                        int neigbors = next->CountNeighbors(zone);

                        // 2d6
                        int roll = rng::make_roll(2, 6);
                        int diff = 12 - neigbors;
                        if (regionSize == 1 || candidates.size() == 1) {
                            diff = 0;
                        }

                        // connections: 5, diff: 7, prob: 58%
                        // connections: 4, diff: 8, prob: 41%
                        // connections: 3, diff: 9, prob: 27%
                        // connections: 2, diff: 10, prob: 16%
                        // connections: 1, diff: 11, prob: 8%

                        if (roll >= diff) {
                            if (volcano > 0) {
                                next->biome = R_VOLCANO;
                                next->region->type = R_VOLCANO;
                                next->region->wages = seed->region->wages;

                                volcano--;
                            }
                            else {
                                next->biome = R_MOUNTAIN;
                                next->region->type = R_MOUNTAIN;
                                next->region->wages = seed->region->wages;

                                mountains--;
                            }

                            next->zone->RemoveRegion(next);
                            zone->AddRegion(next);
                        }
                    }
                }
            }
        }
    }

    this->ConnectZones();
}

void MapBuilder::AddLakes() {
    logger::write("Adding lakes");
    // lakes can appear only on the land, not near water and not on the mountains

    int attempts = 1000;
    int count = rng::make_roll(1, this->lakesMax - this->lakesMin) + this->lakesMin;
    int distance = (std::min(this->w, this->h / 2) * 2) / count;

    std::vector<Coords> lakes;
    lakes.reserve(count);

    while (attempts-- > 0 && count > 0) {
        int x = rng::get_random(this->w);
        int y = rng::get_random(this->h);
        if ((x + y) % 2) continue;

        ZoneRegion* lake = this->GetRegion(x, y);
        if (lake->zone->type != ZoneType::CONTINENT) continue;
        if (!lake->IsInner()) continue;
        if (lake->biome == R_VOLCANO || lake->biome == R_MOUNTAIN) continue;

        for (size_t i = 0; i < lakes.size(); i++) {
            int d = lakes[i].Distance(lake->location);
            if (d < distance) {
                lake = NULL;
                break;
            }
        }

        if (lake == NULL) continue;

        lakes.push_back(lake->location);

        lake->biome = R_LAKE;
        lake->region->type = R_LAKE;
        lake->region->wages = AGetName(0, lake->region);

        count--;
    }
}

void MapBuilder::GrowTerrain() {
    for (auto &kv : this->zones) {
        auto zone = kv.second;
        if (zone->type == ZoneType::OCEAN || zone->type == ZoneType::STRAIT || zone->type == ZoneType::UNDECIDED) {
            logger::write("Grow ocean");
            if (zone->type == ZoneType::UNDECIDED) {
                zone->type = ZoneType::OCEAN;
            }

            for (auto &reg : zone->regions) {
                reg.second->biome = R_OCEAN;
                reg.second->region->type = R_OCEAN;
            }
        }

        if (zone->type == ZoneType::CONTINENT) {
            this->GrowLandInZone(zone);
        }
    }

    this->AddVolcanoes();
    this->AddLakes();
}

void MapBuilder::GrowLandInZone(Zone* zone) {
    logger::write("Growing land in zone");

    if (!zone->CheckZoneIntegerity()) {
        logger::write("Land zone cannot be grown as it have lost integrity");
        exit(1);
    }

    // start with creating provinces
    std::vector<ZoneRegion *> candidates;
    for (auto &kv : zone->regions) {
        candidates.push_back(kv.second);
    }

    // get average province size for this zone
    size_t provinceSize = rng::make_roll(2, 4) + 6;
    size_t provinceCount = zone->regions.size() / provinceSize + 1;

    // absolute size one province cannot exceed
    size_t maxProvinceSize = zone->regions.size() / 2 + 2;

    // place province seeds so that there are at least 2 free hexes between
    std::vector<ZoneRegion *> S;
    int attempts = 0;
    while (attempts++ < 1000 && S.size() < provinceCount) {
        ZoneRegion* next = candidates[rng::get_random(candidates.size())];

        for (auto &seed : S) {
            if (seed->location.Distance(next->location) < 3) {
                next = NULL;
                break;
            }
        }
        if (next == NULL) continue;

        S.push_back(next);
        attempts = 0;
    }

    // assign seeds to provinces
    std::vector<Province *> provinces;
    for (auto &seed : S) {
        auto p = zone->CreateProvince(seed, this->h);
        provinces.push_back(p);
    }

    // as each islands must have min 2 biomes, then there must be min 2 provinces
    if (zone->provinces.size() < 2) {
        attempts = 0;
        while (attempts++ < 1000) {
            ZoneRegion* next = candidates[rng::get_random(candidates.size())];
            if (next->province != NULL) continue;

            provinces.push_back(zone->CreateProvince(next, this->h));
            break;
        }
    }

    // grow all provinces in random order
    while (provinces.size() > 0) {
        int i = rng::get_random(provinces.size());
        auto p = provinces[i];
        size_t size = p->GetSize();

        if (size >= maxProvinceSize || (size >= provinceSize + rng::get_random(5) - 2) || !p->Grow()) {
            provinces.erase(provinces.begin() + i);
            continue;
        }
    }

    // find "holes" and assign them to the provinces
    provinces.clear();
    for (auto &kv : zone->regions) {
        auto reg = kv.second;
        if (reg->province == NULL) {
            provinces.push_back(zone->CreateProvince(reg, this->h));
        }
    }

    while (provinces.size() > 0) {
        int i = rng::get_random(provinces.size());
        auto p = provinces[i];

        if (!p->Grow()) {
            provinces.erase(provinces.begin() + i);
            continue;
        }
    }

    // merge all 1 hex provinces with other provinces
    if (zone->provinces.size() > 2) {
        Province* small;
        do {
            small = NULL;
            for (auto &kv : zone->provinces) {
                auto p = kv.second;
                if (p->GetSize() > 1) continue;

                small = p;
                break;
            }

            if (small == NULL) continue;

            // find neighbor provinces
            auto tmp = small->GetNeighbors();
            Province* target = NULL;
            for (auto x : tmp) {
                if (target == NULL) {
                    target = x;
                    continue;
                }

                if (target->GetSize() > x->GetSize()) {
                    target = x;
                }
            }

            for (auto &kv : small->regions) {
                auto reg = kv.second;
                target->AddRegion(reg);
            }
            zone->RemoveProvince(small);
        }
        while (small != NULL);
    }

    // set biomes for provinces
    std::unordered_set<int> biomes;
    for (auto &kv : zone->provinces) {
        auto p = kv.second;

        int lat = p->GetLatitude();
        auto latBiomes = BIOMES[lat];
        int biomeCount = latBiomes[0];

        std::vector<int> weights;
        weights.resize(biomeCount);
        int w = 0;
        switch ((int) biomes.size()) {
            // biomes = 0, all weights are equal
            case 0:
                for (int i = 0; i < biomeCount; i++) {
                    weights.at(i) = ++w;
                }
                break;

            // biomes = 1, all weights are equal except already present biome where weight will be 0
            case 1:
                for (int i = 0; i < biomeCount; i++) {
                    int biome = latBiomes[i + 1];
                    weights.at(i) = biomes.find(biome) == biomes.end()
                        ? ++w
                        : 0;
                }
                break;

            // biomes > 2
            // neighbor province biomes: 1 (1/7)
            // already present biomeson the islands: 2 (2/7)
            // missing biome: 4 (4/7)
            default:
                auto nb = p->GetNeighborBiomes();
                for (int i = 0; i < biomeCount; i++) {
                    int biome = latBiomes[i + 1];
                    if (nb.find(biome) != nb.end()) {
                        weights.at(i) = ++w;
                    }
                    else if (biomes.find(biome) != biomes.end()) {
                        weights.at(i) = ++w;
                        w += 1;
                    }
                    else {
                        weights.at(i) = ++w;
                        w += 3;
                    }
                }
                break;
        }

        int roll = rng::make_roll(1, w);
        for (int i = biomeCount - 1; i >= 0; i--) {
            int diff = weights.at(i);
            if (diff == 0) continue;
            if (roll >= diff) {
                int biome = latBiomes[i + 1];
                p->biome = biome;
                biomes.insert(biome);
                break;
            }
        }
    }

    // set biomes for regions
    for (auto &kv : zone->provinces) {
        auto p = kv.second;

        int name = -1;
        for (auto &rkv : p->regions) {
            auto reg = rkv.second;

            reg->biome = p->biome;
            reg->region->type = p->biome;

            if (name == -1) {
                name = AGetName(0, reg->region);
            }

            reg->region->wages = name;
        }
    }
}

void MapBuilder::SetOceanNames() {
    for (auto &kv : this->zones) {
        auto ocean = kv.second;
        if (ocean->type == ZoneType::OCEAN || ocean->type == ZoneType::STRAIT) {
            int nameIndex = AGetName(0, ocean->regions.begin()->second->region);
            std::string name = AGetNameString(nameIndex);
            name = ocean->type == ZoneType::OCEAN
                ? name + " Sea"
                : name + " Strait";

            for (auto &reg : ocean->regions) {
                reg.second->region->set_name(name);
            }
        }
    }
}

int ARegion::CheckSea(int dir, int range, int remainocean)
{
    if (type != R_OCEAN) return 0;
    if (range-- < 1) return 1;
    for (int d2 = -1; d2< 2; d2++) {
        int direc = (dir + d2 + NDIRS) % NDIRS;
        ARegion *newregion = neighbors[direc];
        if (!newregion) continue;
        remainocean += newregion->CheckSea(dir, range, remainocean);
        if (remainocean) break;
    }
    return remainocean;
}

void ARegionList::create_abyss_level(int level, const std::string& name)
{
    MakeRegions(level, 4, 4);
    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_NEXUS;

    ARegion *reg = NULL;
    for (int x = 0; x < 4; x++) {
        for (int y = 0; y < 4; y++) {
            reg = pRegionArrays[level]->GetRegion(x, y);
            if (!reg) continue;
            reg->set_name("Abyssal Plains");
            reg->type = R_DESERT;
            reg->wages = -2;
        }
    }

    int tempx, tempy;
    if (Globals->GATES_EXIST) {
        int gateset = 0;
        do {
            tempx = rng::get_random(4);
            tempy = rng::get_random(4);
            reg = pRegionArrays[level]->GetRegion(tempx, tempy);
            if (reg) {
                gateset = 1;
                numberofgates++;
                reg->gate = -1;
            }
        } while(!gateset);
    }

    FinalSetup(pRegionArrays[level]);

    ARegion *lair = NULL;
    do {
        tempx = rng::get_random(4);
        tempy = rng::get_random(4);
        lair = pRegionArrays[level]->GetRegion(tempx, tempy);
    } while(!lair || lair == reg);
    Object *o = new Object(lair);
    o->num = lair->buildingseq++;
    o->set_name(ObjectDefs[O_BKEEP].name);
    o->type = O_BKEEP;
    o->incomplete = 0;
    o->inner = -1;
    lair->objects.push_back(o);
}


void ARegionList::create_nexus_level(int level, int xSize, int ySize, const std::string& name)
{
    MakeRegions(level, xSize, ySize);

    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_NEXUS;

    std::string nex_name = Globals->WORLD_NAME;
    nex_name += " Nexus";

    int x, y;
    for (y = 0; y < ySize; y++) {
        for (x = 0; x < xSize; x++) {
            ARegion *reg = pRegionArrays[level]->GetRegion(x, y);
            if (reg) {
                reg->set_name(nex_name);
                reg->type = R_NEXUS;
            }
        }
    }

    FinalSetup(pRegionArrays[level]);

    for (y = 0; y < ySize; y++) {
        for (int x = 0; x < xSize; x++) {
            ARegion *reg = pRegionArrays[level]->GetRegion(x, y);
            if (reg && Globals->NEXUS_IS_CITY && Globals->TOWNS_EXIST) {
                reg->MakeStartingCity();
                if (Globals->GATES_EXIST) {
                    numberofgates++;
                }
            }
        }
    }
}

void ARegionList::create_surface_level(int level, int xSize, int ySize, const std::string& name)
{
    if (Globals->ICOSAHEDRAL_WORLD) {
        MakeIcosahedralRegions(level, xSize, ySize);
    } else {
        MakeRegions(level, xSize, ySize);
    }

    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_SURFACE;
    int sea = Globals->OCEAN;
    if (Globals->SEA_LIMIT)
        sea = sea * (100 + 2 * Globals->SEA_LIMIT) / 100;

    MakeLand(pRegionArrays[level], sea, Globals->CONTINENT_SIZE);

    CleanUpWater(pRegionArrays[level]);

    SetupAnchors(pRegionArrays[level]);

    GrowTerrain(pRegionArrays[level], 0);

    AssignTypes(pRegionArrays[level]);

    SeverLandBridges(pRegionArrays[level]);

    if (Globals->LAKES) RemoveCoastalLakes(pRegionArrays[level]);

    if (Globals->GROW_RACES) GrowRaces(pRegionArrays[level]);

    FinalSetup(pRegionArrays[level]);
}

void ARegionList::create_island_level(int level, int nPlayers, const std::string& name)
{
    int xSize, ySize;
    xSize = 20 + (nPlayers + 3) / 4 * 6 - 2;
    ySize = xSize;

    MakeRegions(level, xSize, ySize);

    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_SURFACE;

    MakeCentralLand(pRegionArrays[level]);
    MakeIslands(pRegionArrays[level], nPlayers);
    RandomTerrain(pRegionArrays[level]);

    if (Globals->LAKES) RemoveCoastalLakes(pRegionArrays[level]);

    if (Globals->GROW_RACES) GrowRaces(pRegionArrays[level]);

    FinalSetup(pRegionArrays[level]);
}

void ARegionList::create_island_ring_level(int level, int xSize, int ySize, const std::string& name)
{
    MakeRegions(level, xSize, ySize);
    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_SURFACE;

    MakeRingLand(pRegionArrays[level], 12, 20);

    CleanUpWater(pRegionArrays[level]);

    SetupAnchors(pRegionArrays[level]);

    GrowTerrain(pRegionArrays[level], 0);

    AssignTypes(pRegionArrays[level]);

    PlaceVolcanos(pRegionArrays[level]);

    SeverLandBridges(pRegionArrays[level]);

    if (Globals->LAKES) RemoveCoastalLakes(pRegionArrays[level]);
    if (Globals->GROW_RACES) GrowRaces(pRegionArrays[level]);

    // Put the altars in the ring around the center
    ARegion *center = pRegionArrays[level]->GetRegion(xSize/2, ySize/2);
    for (int i = 0; i < NDIRS; i++) {
        ARegion *n = center->neighbors[i];
        if (n) {
            Object *o = new Object(n);
            o->num = n->buildingseq++;
            o->set_name(ObjectDefs[O_RITUAL_ALTAR].name);
            o->type = O_RITUAL_ALTAR;
            o->incomplete = -(ObjectDefs[O_RITUAL_ALTAR].sacrifice_amount);
            n->objects.push_back(o);
        }
    }
    FinalSetup(pRegionArrays[level]);
}

void ARegionList::create_underworld_ring_level(int level, int xSize, int ySize, const std::string& name)
{
    if (Globals->ICOSAHEDRAL_WORLD) {
        MakeIcosahedralRegions(level, xSize, ySize);
    } else {
        MakeRegions(level, xSize, ySize);
    }

    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_UNDERWORLD;

    ARegion *reg = pRegionArrays[level]->GetRegion(xSize/2, ySize/2);
    reg->type = R_BARREN;

    // Break all connections between barrens and surrounding hexes
    for (int i = 0; i < NDIRS; i++) {
        ARegion *n = reg->neighbors[i];
        if (n) {
            reg->neighbors[i] = nullptr;
            n->neighbors[reg->GetRealDirComp(i)] = nullptr;
        }
    }

    SetRegTypes(pRegionArrays[level], R_NUM);

    SetupAnchors(pRegionArrays[level]);

    GrowTerrain(pRegionArrays[level], 1);

    AssignTypes(pRegionArrays[level]);

    MakeUWMaze(pRegionArrays[level]);

    if (Globals->LAKES) RemoveCoastalLakes(pRegionArrays[level]);

    if (Globals->GROW_RACES) GrowRaces(pRegionArrays[level]);

    // Connect the center region to the surface via a shaft
    ARegionArray *surface = pRegionArrays[level-1];
    ARegion *center = surface->GetRegion(surface->x/2, surface->y/2);
    Object *o = new Object(reg);
    o->num = reg->buildingseq++;
    o->set_name("Shaft");
    o->type = O_SHAFT;
    o->incomplete = 0;
    o->inner = center->num;
    reg->objects.push_back(o);

    o = new Object(center);
    o->num = center->buildingseq++;
    o->set_name("Shaft");
    o->type = O_SHAFT;
    o->incomplete = 0;
    o->inner = reg->num;
    center->objects.push_back(o);

    // Put the monolith in the underworld center
    o = new Object(reg);
    o->num = reg->buildingseq++;
    o->set_name(ObjectDefs[O_DORMANT_MONOLITH].name);
    o->type = O_DORMANT_MONOLITH;
    o->incomplete = -(ObjectDefs[O_DORMANT_MONOLITH].sacrifice_amount);
    reg->objects.push_back(o);

    FinalSetup(pRegionArrays[level]);

}

void ARegionList::create_underworld_level(int level, int xSize, int ySize, const std::string& name)
{
    if (Globals->ICOSAHEDRAL_WORLD) {
        MakeIcosahedralRegions(level, xSize, ySize);
    } else {
        MakeRegions(level, xSize, ySize);
    }

    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_UNDERWORLD;

    SetRegTypes(pRegionArrays[level], R_NUM);

    SetupAnchors(pRegionArrays[level]);

    GrowTerrain(pRegionArrays[level], 1);

    AssignTypes(pRegionArrays[level]);

    MakeUWMaze(pRegionArrays[level]);

    if (Globals->LAKES) RemoveCoastalLakes(pRegionArrays[level]);

    if (Globals->GROW_RACES) GrowRaces(pRegionArrays[level]);

    FinalSetup(pRegionArrays[level]);
}

void ARegionList::create_underdeep_level(int level, int xSize, int ySize, const std::string& name)
{
    if (Globals->ICOSAHEDRAL_WORLD) {
        MakeIcosahedralRegions(level, xSize, ySize);
    } else {
        MakeRegions(level, xSize, ySize);
    }

    pRegionArrays[level]->set_name(name);
    pRegionArrays[level]->levelType = ARegionArray::LEVEL_UNDERDEEP;

    SetRegTypes(pRegionArrays[level], R_NUM);

    SetupAnchors(pRegionArrays[level]);

    GrowTerrain(pRegionArrays[level], 1);

    AssignTypes(pRegionArrays[level]);

    MakeUWMaze(pRegionArrays[level]);

    if (Globals->LAKES) RemoveCoastalLakes(pRegionArrays[level]);

    if (Globals->GROW_RACES) GrowRaces(pRegionArrays[level]);

    FinalSetup(pRegionArrays[level]);
}

int rimefall_taper_half_width(ARegionArray *pRegs, int y)
{
    // 0 at the northern edge of the map, 1 at the southern.
    double t = (pRegs->y > 1) ? (double) y / (double) (pRegs->y - 1) : 0.0;
    double frac = RIMEFALL_TAPER_NORTH + t * (RIMEFALL_TAPER_SOUTH - RIMEFALL_TAPER_NORTH);
    return (int) (frac * pRegs->x);
}

bool rimefall_inside_taper(ARegionArray *pRegs, int x, int y)
{
    // ARegionArray::GetRegion reduces both coordinates modulo the array size, so the surface has
    // no eastern or western edge. The continent is centred on one meridian and the distance to it
    // is measured the short way round; taking a plain absolute difference would cut the continent
    // in half across the seam.
    int centre = pRegs->x / 2;
    int dx = abs(x - centre);
    if (dx > pRegs->x / 2) dx = pRegs->x - dx;
    return dx <= rimefall_taper_half_width(pRegs, y);
}

const int rimefall_starts_per_band[RIMEFALL_BANDS] = { 3, 5, 8, 3, 1 };

static const std::string rimefall_band_names[RIMEFALL_BANDS] = {
    "the frozen north",
    "the northern frontier",
    "the middle lands",
    "the southern reaches",
    "the far south",
};

const std::string& rimefall_band_name(int band)
{
    if (band < 0) band = 0;
    if (band >= RIMEFALL_BANDS) band = RIMEFALL_BANDS - 1;
    return rimefall_band_names[band];
}

int rimefall_band_of_row(ARegionArray *pRegs, int y)
{
    int band = (y * RIMEFALL_BANDS) / pRegs->y;
    if (band < 0) band = 0;
    if (band >= RIMEFALL_BANDS) band = RIMEFALL_BANDS - 1;
    return band;
}

int rimefall_band_of(ARegion *reg, ARegionList& regions)
{
    return rimefall_band_of_row(regions.GetRegionArray(reg->zloc), reg->yloc);
}

// Is this a land hex with at least one ocean neighbour?
static bool rimefall_is_coastal(ARegion *reg)
{
    if (!reg || TerrainDefs[reg->type].similar_type == R_OCEAN) return false;
    for (int d = 0; d < NDIRS; d++) {
        ARegion *n = reg->neighbors[d];
        if (n && TerrainDefs[n->type].similar_type == R_OCEAN) return true;
    }
    return false;
}

ARegion *rimefall_north_source_site(ARegionArray *pRegs)
{
    // The deepest north the continent reaches. Where the cold comes from, so it should be behind
    // the front rather than in front of it — the front starts on top of its own source and creeps
    // away from it.
    for (int y = 0; y < pRegs->y; y++) {
        for (int x = 0; x < pRegs->x; x++) {
            ARegion *reg = pRegs->GetRegion(x, y);
            if (!reg) continue;
            if (TerrainDefs[reg->type].similar_type == R_OCEAN) continue;
            return reg;
        }
    }
    return nullptr;
}

ARegion *rimefall_east_source_site(ARegionArray *pRegs)
{
    // The eastern flank of the southern half, measured from the continent's own centre line
    // rather than from the array edge, which does not exist. Scanning x downward from the centre
    // line's opposite side finds the outermost eastern coast; the first coastal hex found in the
    // southern half wins.
    int centre = pRegs->x / 2;
    for (int y = pRegs->y / 2; y < pRegs->y; y++) {
        for (int offset = pRegs->x / 2; offset > 0; offset--) {
            ARegion *reg = pRegs->GetRegion((centre + offset) % pRegs->x, y);
            if (rimefall_is_coastal(reg)) return reg;
        }
    }
    // A world whose southern half has no eastern coast at all is not one this ruleset can place a
    // dragon source in. Fall back to any southern coast rather than returning nothing, since the
    // game needs both sources to be winnable.
    for (int y = pRegs->y / 2; y < pRegs->y; y++) {
        for (int x = 0; x < pRegs->x; x++) {
            ARegion *reg = pRegs->GetRegion(x, y);
            if (rimefall_is_coastal(reg)) return reg;
        }
    }
    return nullptr;
}

int rimefall_taper_area(ARegionArray *pRegs)
{
    int area = 0;
    for (int x = 0; x < pRegs->x; x++)
        for (int y = 0; y < pRegs->y; y++)
            if (pRegs->GetRegion(x, y) && rimefall_inside_taper(pRegs, x, y))
                area++;
    return area;
}

// How many consecutive passes of the land loop may fail to claim a single hex before generation
// is declared stuck. Every pass draws a fresh seed, so a run this long without progress does not
// happen by chance on a taper that has room; it means the taper is full and the ocean target is
// unreachable. Generous, and still instant to hit.
#define RIMEFALL_MAKELAND_STALL_LIMIT 10000

void ARegionList::MakeLand(ARegionArray *pRegs, int percentOcean,
        int continentSize)
{
    int total, ocean;

    total = 0;
    for (int x=0; x < pRegs->x; x++)
        for (int y=0; y < pRegs->y; y++)
            if (pRegs->GetRegion(x, y))
                total++;
    ocean = total;

    // The loop below runs until the ocean count falls below its target. Rimefall confines land to
    // the taper, so if the taper is smaller than the land the target demands, that condition can
    // never be met. Say so up front, with the numbers, rather than hanging: this is the failure
    // mode that follows from changing OCEAN or the taper constants without checking the other.
    int wanted_land = total - (total * percentOcean) / 100;
    int taper = rimefall_taper_area(pRegs);
    if (taper < wanted_land) {
        logger::write(
            "Rimefall: the taper holds " + std::to_string(taper) + " hexes but OCEAN demands " +
            std::to_string(wanted_land) + " of land. Raise OCEAN in rimefall/rules.cpp or widen "
            "RIMEFALL_TAPER_* in rimefall/rimefall.h; see the note in that header."
        );
        throw "Rimefall: the taper cannot hold the land OCEAN demands.";
    }

    logger::write("Making land");
    int stalled = 0;
    int ocean_at_last_pass = ocean;
    while (ocean > (total * percentOcean) / 100) {
        // Checked at the top of the pass rather than the bottom, because the body reaches the
        // next iteration through several `continue`s and a bottom check would miss all of them.
        if (ocean == ocean_at_last_pass) {
            if (++stalled > RIMEFALL_MAKELAND_STALL_LIMIT) {
                logger::write(
                    "Rimefall: land generation stalled with " + std::to_string(ocean) +
                    " ocean hexes left and a target of " +
                    std::to_string((total * percentOcean) / 100) + "."
                );
                throw "Rimefall: land generation made no progress; the taper is saturated.";
            }
        } else {
            stalled = 0;
            ocean_at_last_pass = ocean;
        }

        int sz = rng::get_random(continentSize);
        sz = sz * sz;

        int tempx = rng::get_random(pRegs->x);
        int yoff = pRegs->y / 40;
        int yband = pRegs->y / 2 - 2 * yoff;
        int tempy = (rng::get_random(yband)+yoff) * 2 + tempx % 2;

        // Seed inside the taper only. Together with the growth check below this is what makes the
        // continent a taper at all; the randomness of which cells actually get claimed is what
        // keeps its coastline ragged rather than a drawn trapezoid.
        if (!rimefall_inside_taper(pRegs, tempx, tempy)) continue;

        ARegion *reg = pRegs->GetRegion(tempx, tempy);
        if (!reg) continue;
        ARegion *newreg = reg;
        ARegion *seareg = reg;

        // Archipelago or Continent?
        //
        // ARCHIPELAGO is 0 in rimefall/rules.cpp, so this branch is unreachable and is left as
        // upstream wrote it. It is NOT taper-constrained: raising ARCHIPELAGO would scatter
        // islands across the whole ocean and break the one-continent premise. Constrain it here
        // first if it is ever wanted.
        if (rng::get_random(100) < Globals->ARCHIPELAGO) {
            // Make an Archipelago:
            sz = sz / 5 + 1;
            int first = 1;
            int tries = 0;
            for (int i=0; i<sz; i++) {
                int direc = rng::get_random(NDIRS);
                newreg = reg->neighbors[direc];
                while (!newreg) {
                    direc = rng::get_random(NDIRS);
                    newreg = reg->neighbors[direc];
                }
                tries++;
                for (int m = 0; m < 2; m++) {
                    seareg = newreg;
                    newreg = seareg->neighbors[direc];
                    if (!newreg) break;
                }
                if (!newreg) break;
                if (newreg) {
                    seareg = newreg;
                    newreg = seareg->neighbors[rng::get_random(NDIRS)];
                    if (!newreg) break;
                    // island start point (~3 regions away from last island)
                    seareg = newreg;
                    if (first) {
                        seareg = reg;
                        first = 0;
                    }
                    if (seareg->type == -1) {
                        reg = seareg;
                        tries = 0;
                        reg->type = R_NUM;
                        ocean--;
                    } else {
                        if (tries > 5) break;
                        continue;
                    }
                    int growit = rng::get_random(20);
                    int growth = 0;
                    int growch = 2;
                    // grow this island
                    while (growit > growch) {
                        growit = rng::get_random(20);
                        tries = 0;
                        int newdir = rng::get_random(NDIRS);
                        while (direc == reg->GetRealDirComp(newdir)) {
                            newdir = rng::get_random(NDIRS);
                        }
                        newreg = reg->neighbors[newdir];
                        while ((!newreg) && (tries < 36)) {
                            while (direc == reg->GetRealDirComp(newdir)) {
                                newdir = rng::get_random(NDIRS);
                            }
                            newreg = reg->neighbors[newdir];
                            tries++;
                        }
                        if (!newreg) continue;
                        reg = newreg;
                        tries = 0;
                        if (reg->type == -1) {
                            reg->type = R_NUM;
                            growth++;
                            if (growth > growch) growch = growth;
                            ocean--;
                        } else continue;
                    }
                }
            }
        } else {
            // make a continent
            if (reg->type == -1) {
                reg->type = R_NUM;
                ocean--;
            }
            for (int i=0; i<sz; i++) {
                int dir = rng::get_random(NDIRS);
                if ((reg->yloc < yoff*2) && ((dir < 2) || (dir == (NDIRS-1)))
                    && (rng::get_random(4) < 3)) continue;
                if ((reg->yloc > (yband+yoff)*2) && ((dir < 5) && (dir > 1))
                    && (rng::get_random(4) < 3)) continue;
                ARegion *newreg = reg->neighbors[dir];
                if (!newreg) break;
                // Growth stops at the taper. `continue` rather than `break` so the walk tries
                // another direction instead of abandoning the continent the moment it touches the
                // edge — that fills the taper's flanks properly. The loop is bounded by sz, so
                // this cannot spin.
                if (!rimefall_inside_taper(pRegs, newreg->xloc, newreg->yloc)) continue;
                int polecheck = 0;
                for (int v=0; v < NDIRS; v++) {
                    ARegion *creg = newreg->neighbors[v];
                    if (!creg) polecheck = 1;
                }
                if (polecheck) break;
                reg = newreg;
                if (reg->type == -1) {
                    reg->type = R_NUM;
                    ocean--;
                }
            }
        }
    }

    // At this point, go back through and set all the rest to ocean
    SetRegTypes(pRegs, R_OCEAN);
    logger::write("");
}

void ARegionList::MakeRingLand(ARegionArray *pRegs, int minDistance, int maxDistance) {
    ARegion *center = pRegs->GetRegion(pRegs->x / 2, pRegs->y / 2);
    if (!center) { throw "Center region not found"; }

    logger::write("Making land in ring");
    for (int i = 0; i < pRegs->x; i++) {
        for (int j = 0; j < pRegs->y; j++) {
            ARegion *reg = pRegs->GetRegion(i, j);
            if (!reg) continue;
            // By default, everything is ocean
            reg->type = R_OCEAN;

            // If the regions is between minDistance and maxDistance from the center, 65% chance of being land.
            int distance = GetPlanarDistance(center, reg, 1000, -1);
            bool awayFromEdge = i >= 4 && i <= pRegs->x - 4 && j >= 4 && j <= pRegs->y - 4;
            if (distance >= minDistance && distance <= maxDistance && rng::get_random(100) < 60) {
                reg->type = R_NUM;
            } else if (distance >= maxDistance && awayFromEdge && rng::get_random(100) < 20) {
                MakeOneIsland(pRegs, i, j);
            } else if (distance < minDistance && distance > 4 && rng::get_random(100) < 15) {
                MakeOneIsland(pRegs, i, j);
            }
        }
    }

    logger::write("Perturbing the coastlines");
    for (int iter = 0; iter < 10; iter++) {
        for (int i = 0; i < pRegs->x; i++) {
            for (int j = 0; j < pRegs->y; j++) {
                ARegion *reg = pRegs->GetRegion(i, j);
                if (!reg) continue;
                int distance = GetPlanarDistance(center, reg, 1000, -1);
                int different = 0;
                for (int d = 0; d < NDIRS; d++) {
                    ARegion *newreg = reg->neighbors[d];
                    if (!newreg) continue;
                    if (newreg->type != reg->type) different++;
                }

                if (distance <= minDistance + 2 && distance > 3) {
                    // inner coastline.  High chance of land becoming ocean, low chance of ocean becoming land.
                    // Chance is proportional to neighbors that are different.
                    if (rng::get_random(100) < (different * (reg->type == R_NUM ? 2 : 1))) {
                        reg->wages = -2;
                    }
                } else if (distance >= maxDistance - 1 && i >= 2 && i <= pRegs->x - 2 && j >= 2 && j <= pRegs->y - 2) {
                    // outer coastline. Moderate chance of ocean becoming land, lower chance of land becoming ocean.
                    // Chance is proportional to neighbors that are different.
                    if (rng::get_random(100) < (different * (reg->type != R_NUM ? 3 : 2))) {
                        reg->wages = -2;
                    }
                }
            }
        }
        // Apply the changes
        for (int i = 0; i < pRegs->x; i++) {
            for (int j = 0; j < pRegs->y; j++) {
                ARegion *reg = pRegs->GetRegion(i, j);
                if (!reg) continue;
                // If the region changed type, swap it to the new type and clear the flag.
                if (reg->wages == -2) reg->type = (reg->type == R_OCEAN ? R_NUM : R_OCEAN);
                reg->wages = -1;
            }
        }
    }

    logger::write("Adding the barrens");
    center->type = R_BARREN;
    for (int d = 0; d < NDIRS; d++) {
        ARegion *newreg = center->neighbors[d];
        if (!newreg) continue;
        newreg->type = R_BARREN;
    }

    logger::write("");
}

void ARegionList::MakeCentralLand(ARegionArray *pRegs)
{
    for (int i = 0; i < pRegs->x; i++) {
        for (int j = 0; j < pRegs->y; j++) {
            ARegion *reg = pRegs->GetRegion(i, j);
            if (!reg) continue;
            // Initialize region to ocean.
            reg->type = R_OCEAN;
            // If the region is close to the edges, it stays ocean
            if (i < 8 || i >= pRegs->x - 8 || j < 8 || j >= pRegs->y - 8)
                continue;
            // If the region is within 10 of the edges, it has a 50%
            // chance of staying ocean.
            if (i < 10 || i >= pRegs->x - 10 || j < 10 || j >= pRegs->y - 10) {
                if (rng::get_random(100) > 50) continue;
            }

            // Otherwise, set the region to land.
            reg->type = R_NUM;
        }
    }
}

void ARegionList::MakeIslands(ARegionArray *pArr, int nPlayers)
{
    // First, make the islands along the top.
    int i;
    int nRow = (nPlayers + 3) / 4;
    for (i = 0; i < nRow; i++)
        MakeOneIsland(pArr, 10 + i * 6, 2);
    // Next, along the left.
    nRow = (nPlayers + 2) / 4;
    for (i = 0; i < nRow; i++)
        MakeOneIsland(pArr, 2, 10 + i * 6);
    // The islands along the bottom.
    nRow = (nPlayers + 1) / 4;
    for (i = 0; i < nRow; i++)
        MakeOneIsland(pArr, 10 + i * 6, pArr->y - 6);
    // And the islands on the right.
    nRow = nPlayers / 4;
    for (i = 0; i < nRow; i++)
        MakeOneIsland(pArr, pArr->x - 6, 10 + i * 6);
}

void ARegionList::MakeOneIsland(ARegionArray *pRegs, int xx, int yy)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            ARegion *pReg = pRegs->GetRegion(i + xx, j + yy);
            if (!pReg) continue;
            pReg->type = R_NUM;
        }
    }
}

void ARegionList::CleanUpWater(ARegionArray *pRegs)
{
    logger::write("Converting Scattered Water");
    int dotter = 0;
    for (int ctr = 0; ctr < Globals->SEA_LIMIT+1; ctr++) {
        for (int i = 0; i < pRegs->x; i++) {
            for (int j = 0; j < pRegs->y; j++) {
                ARegion *reg = pRegs->GetRegion(i, j);
                int remainocean = 0;
                if ((!reg) || (reg->type != R_OCEAN)) continue;
                for (int d = 0; d < NDIRS; d++) {
                    remainocean += reg->CheckSea(d, Globals->SEA_LIMIT, remainocean);
                }
                if (dotter++%2000 == 0) logger::dot();
                if (remainocean > 0) continue;
                reg->wages = 0;
                if (rng::get_random(100) < Globals->LAKES) {
                        reg->type = R_LAKE;
                } else reg->type = R_NUM;
            }
        }
    }
    logger::write("");
}

void ARegionList::RemoveCoastalLakes(ARegionArray *pRegs)
{
    logger::write("Removing coastal 'lakes'");
    for (int c = 0; c < 2; c++) {
        for (int i = 0; i < pRegs->x; i++) {
            for (int j = 0; j < pRegs->y; j++) {
                ARegion *reg = pRegs->GetRegion(i, j);
                if ((!reg) || (reg->type != R_LAKE)) continue;
                if (reg->IsCoastal() > 0) {
                    reg->type = R_OCEAN;
                    reg->wages = -1;
                    logger::dot();
                } else if (reg->wages <= 0) { // name the Lake
                    int wage1 = 0;
                    int count1 = 0;
                    int wage2 = 0;
                    int count2 = 0;
                    int temp = 0;
                    for (int d = 0; d < NDIRS; d++) {
                        ARegion *newregion = reg->neighbors[d];
                        if (!newregion) continue;
                        // name after neighboring lake regions preferrentially
                        if ((newregion->wages > 0) &&
                                (newregion->type == R_LAKE)) {
                            count1 = 1;
                            wage1 = newregion->wages;
                            break;
                        }
                        if ((TerrainDefs[newregion->type].similar_type !=
                                    R_OCEAN) && (newregion->wages > -1)) {
                            if (newregion->wages == wage1) count1++;
                            else if (newregion->wages == wage2) count2++;
                            else if (count2 == 0) {
                                wage2 = newregion->wages;
                                count2++;
                            }
                            if (count2 > count1) {
                                temp = wage1;
                                wage1 = wage2;
                                wage2 = temp;
                                int tmpin = count1;
                                count1 = count2;
                                count2 = tmpin;
                            }
                        }
                    }
                    if (count1 > 0) reg->wages = wage1;
                }
            }
        }
    }
    logger::write("");
}

void ARegionList::SeverLandBridges(ARegionArray *pRegs)
{
    logger::write("Severing land bridges");
    // mark land hexes to delete
    for (int i = 0; i < pRegs->x; i++) {
        for (int j = 0; j < pRegs->y; j++) {
            ARegion *reg = pRegs->GetRegion(i, j);
            if ((!reg) || (TerrainDefs[reg->type].similar_type == R_OCEAN))
                continue;
            if (reg->IsCoastal() != 4) continue;
            int tidych = Globals->SEVER_LAND_BRIDGES;
            for (int d = 0; d < NDIRS; d++) {
                ARegion *newregion = reg->neighbors[d];
                if ((!newregion) ||
                        (TerrainDefs[newregion->type].similar_type == R_OCEAN))
                    continue;
                if (newregion->IsCoastal() == 4) tidych = tidych * 2;
            }
            if (rng::get_random(100) < (tidych)) reg->wages = -2;
        }
    }
    // now change to ocean
    for (int i = 0; i < pRegs->x; i++) {
        for (int j = 0; j < pRegs->y; j++) {
            ARegion *reg = pRegs->GetRegion(i, j);
            if ((!reg) || (reg->wages > -2)) continue;
            reg->type = R_OCEAN;
            reg->wages = -1;
            logger::dot();
        }
    }
    logger::write("");
}

void ARegionList::SetRegTypes(ARegionArray *pRegs, int newType)
{
    for (int i = 0; i < pRegs->x; i++) {
        for (int j = 0; j < pRegs->y; j++) {
            ARegion *reg = pRegs->GetRegion(i, j);
            if (!reg) continue;
            if (reg->type == -1) reg->type = newType;
        }
    }
}

void ARegionList::SetupAnchors(ARegionArray *ta)
{
    // Now, setup the anchors
    logger::write("Setting up the anchors");
    int skip = 250;
    int f = 2;
    if (Globals->TERRAIN_GRANULARITY) {
        skip = Globals->TERRAIN_GRANULARITY;
        while (skip > 5) {
            f++;
            skip -= 5;
            if (skip < 1) skip = 1;
        }
        skip = 100 * ((skip+3) * f + 2) / (skip + f - 2);
    }
    int dotter = 0;
    for (int x=0; x<(ta->x)/f; x++) {
        for (int y=0; y<(ta->y)/(f*2); y++) {
            if (rng::get_random(1000) > skip) continue;
            ARegion *reg = 0;
            for (int i=0; i<4; i++) {
                int tempx = x * f + rng::get_random(f);
                int tempy = y * f * 2 + rng::get_random(f)*2 + tempx%2;
                reg = ta->GetRegion(tempx, tempy);
                if (!reg)
                    continue;
                if (reg->type == R_NUM) {
                    reg->type = GetRegType(reg);
                    reg->population = 1;
                    if (TerrainDefs[reg->type].similar_type != R_OCEAN)
                        reg->wages = AGetName(0, reg);
                    break;
                }
            }
            if (dotter++%30 == 0) logger::dot();
        }
    }

    logger::write("");
}

void ARegionList::GrowTerrain(ARegionArray *pArr, int growOcean)
{
    logger::write("Growing Terrain...");
    for (int j=0; j<30; j++) {
        int x, y;
        for (x = 0; x < pArr->x; x++) {
            for (y = 0; y < pArr->y; y++) {
                ARegion *reg = pArr->GetRegion(x, y);
                if (!reg) continue;
                reg->population = 1;
            }
        }
        for (x = 0; x < pArr->x; x++) {
            for (y = 0; y < pArr->y; y++) {
                ARegion *reg = pArr->GetRegion(x, y);
                if (!reg) continue;
                if ((j > 0) && (j < 21) && (rng::get_random(3) < 2)) continue;
                if (reg->type == R_NUM) {

                    // Check for Lakes
                    if (Globals->LAKES &&
                        (rng::get_random(100) < (Globals->LAKES/10 + 1))) {
                            reg->type = R_LAKE;
                            break;
                    }
                    // Check for Odd Terrain
                    if (rng::get_random(1000) < Globals->ODD_TERRAIN) {
                        reg->type = GetRegType(reg);
                        if (TerrainDefs[reg->type].similar_type != R_OCEAN)
                            reg->wages = AGetName(0, reg);
                        break;
                    }


                    int init = rng::get_random(6);
                    for (int i=0; i<NDIRS; i++) {
                        ARegion *t = reg->neighbors[(i+init) % NDIRS];
                        if (t) {
                            if (t->population < 1) continue;
                            if (t->type != R_NUM && t->type != R_BARREN &&
                                (TerrainDefs[t->type].similar_type!=R_OCEAN ||
                                 (growOcean && (t->type != R_LAKE)))) {
                                if (j==0) t->population = 0;
                                reg->population = 0;
                                reg->race = t->type;
                                reg->wages = t->wages;
                                break;
                            }
                        }
                    }
                }
            }
        }

        for (x = 0; x < pArr->x; x++) {
            for (y = 0; y < pArr->y; y++) {
                ARegion *reg = pArr->GetRegion(x, y);
                if (!reg) continue;
                if (reg->type == R_NUM && reg->race != -1)
                    reg->type = reg->race;
            }
        }
    }
}

void ARegionList::RandomTerrain(ARegionArray *pArr)
{
    int x, y;
    for (x = 0; x < pArr->x; x++) {
        for (y = 0; y < pArr->y; y++) {
            ARegion *reg = pArr->GetRegion(x, y);
            if (!reg) continue;

            if (reg->type == R_NUM) {
                int adjtype = 0;
                int adjname = -1;
                for (int d = 0; d < NDIRS; d++) {
                    ARegion *newregion = reg->neighbors[d];
                    if (!newregion) continue;
                    // R_NUM is the terrain count, and doubles as the "unassigned" sentinel
                    // during generation, so it has to be excluded before indexing TerrainDefs.
                    if ((newregion->type != R_NUM) &&
                            (TerrainDefs[newregion->type].similar_type != R_OCEAN) &&
                            (newregion->wages > 0)) {
                        adjtype = newregion->type;
                        adjname = newregion->wages;
                    }
                }
                if (adjtype && !Globals->CONQUEST_GAME) {
                    reg->type = adjtype;
                    reg->wages = adjname;
                } else {
                    reg->type = GetRegType(reg);
                    reg->wages = AGetName(0, reg);
                }
            }
        }
    }
}

void ARegionList::MakeUWMaze(ARegionArray *pArr)
{
    int x, y;

    for (x = 0; x < pArr->x; x++) {
        for (y = 0; y < pArr->y; y++) {
            ARegion *reg = pArr->GetRegion(x, y);
            if (!reg) continue;
            if (reg->type == R_BARREN) continue;

            for (int i=D_NORTH; i != NDIRS; ++i) {
                int count = 0;
                for (int j=D_NORTH; j != NDIRS; ++j)
                    if (reg->neighbors[j]) count++;
                if (count <= 1) break;

                ARegion *n = reg->neighbors[i];
                if (n) {
                    if (n->type == R_BARREN) continue;
                    if (n->xloc < x || (n->xloc == x && n->yloc < y))
                        continue;
                    if (!CheckRegionExit(reg, n)) {
                        count = 0;
                        for (int k = D_NORTH; k != NDIRS; ++k) {
                            if (n->neighbors[k]) count++;
                        }
                        if (count <= 1) break;
                        n->neighbors[reg->GetRealDirComp(i)] = 0;
                        reg->neighbors[i] = 0;
                    }
                }
            }
        }
    }
}

void ARegionList::AssignTypes(ARegionArray *pArr)
{
    // RandomTerrain() will set all of the un-set region types and names.
    RandomTerrain(pArr);
}

void ARegionList::UnsetRace(ARegionArray *pArr)
{
    int x, y;
    for (x = 0; x < pArr->x; x++) {
        for (y = 0; y < pArr->y; y++) {
            ARegion *reg = pArr->GetRegion(x, y);
            if (!reg) continue;
            reg->race = - 1;
        }
    }
}

void ARegionList::RaceAnchors(ARegionArray *pArr)
{
    logger::write("Setting Race Anchors");
    UnsetRace(pArr);
    int x, y;
    int wigout = 0;
    for (x = 0; x < pArr->x; x++) {
        for (y = 0; y < pArr->y; y++) {
            // Anchor distribution: depends on GROW_RACES value
            int jiggle = 4 + 2 * Globals->GROW_RACES;
            if ((y + ((x % 2) * jiggle/2)) % jiggle > 1) continue;
            int xoff = x + 2 - rng::get_random(3) - rng::get_random(3);
            ARegion *reg = pArr->GetRegion(xoff, y);
            if (!reg) continue;

            if ((reg->type == R_LAKE) && (!Globals->LAKESIDE_IS_COASTAL))
                continue;
            if (TerrainDefs[reg->type].flags & TerrainType::BARREN) continue;

            reg->race = -1;
            wigout = 0; // reset sanity

            if (TerrainDefs[reg->type].similar_type == R_OCEAN) {
                // setup near coastal race here
                int d = rng::get_random(NDIRS);
                int ctr = 0;
                ARegion *nreg = reg->neighbors[d];
                if (!nreg) continue;
                while((ctr++ < 20) && (reg->race == -1)) {
                    if (TerrainDefs[nreg->type].similar_type != R_OCEAN) {
                        int rnum = sizeof(TerrainDefs[nreg->type].coastal_races) /
                            sizeof(TerrainDefs[nreg->type].coastal_races[0]);

                        while (reg->race == -1 || (ItemDefs[reg->race].flags & ItemType::DISABLED)) {
                            reg->race = TerrainDefs[nreg->type].coastal_races[rng::get_random(rnum)];
                            if (++wigout > 100) break;
                        }
                    } else {
                        int dir = rng::get_random(NDIRS);
                        if (d == nreg->GetRealDirComp(dir)) continue;
                        if (!(nreg->neighbors[dir])) continue;
                        nreg = nreg->neighbors[dir];
                    }
                }
            } else {
                // setup noncoastal race here
                int rnum = sizeof(TerrainDefs[reg->type].races)/sizeof(TerrainDefs[reg->type].races[0]);

                while ( reg->race == -1 || (ItemDefs[reg->race].flags & ItemType::DISABLED)) {
                    reg->race = TerrainDefs[reg->type].races[rng::get_random(rnum)];
                    if (++wigout > 100) break;
                }
            }

            /* leave out this sort of check for the moment
            if (wigout > 100) {
                // do something!
                logger::write("There is a problem with the races in the ");
                logger::write(TerrainDefs[reg->type].name);
                logger::write(" region type");
            }
            */

            if (reg->race == -1) {
                logger::write(
                    "Hey! No race anchor got assigned to the " + TerrainDefs[reg->type].name
                    + " at " + std::to_string(x) + "," + std::to_string(y)
                );
            }
        }
    }
}

void ARegionList::GrowRaces(ARegionArray *pArr)
{
    logger::write("Growing Races");
    RaceAnchors(pArr);
    int a, x, y;
    for (a = 0; a < 25; a++) {
        for (x = 0; x < pArr->x; x++) {
            for (y = 0; y < pArr->y; y++) {
                ARegion *reg = pArr->GetRegion(x, y);
                if ((!reg) || (reg->race == -1)) continue;

                for (int dir = 0; dir < NDIRS; dir++) {
                    ARegion *nreg = reg->neighbors[dir];
                    if ((!nreg) || (nreg->race != -1)) continue;
                    int iscoastal = 0;
                    int cnum = sizeof(TerrainDefs[reg->type].coastal_races) /
                        sizeof(TerrainDefs[reg->type].coastal_races[0]);
                    for (int i=0; i<cnum; i++) {
                        if (reg->race == TerrainDefs[reg->type].coastal_races[i])
                            iscoastal = 1;
                    }
                    // Only coastal races may pass from sea to land
                    if ((TerrainDefs[nreg->type].similar_type == R_OCEAN) && (!iscoastal)) continue;

                    int ch = rng::get_random(5);
                    if (iscoastal) {
                        if (TerrainDefs[nreg->type].similar_type == R_OCEAN)
                            ch += 2;
                    } else {
                        auto mt = find_race(ItemDefs[reg->race].abr)->get();
                        if (mt.terrain==TerrainDefs[nreg->type].similar_type) ch += 2;
                        int rnum = sizeof(TerrainDefs[nreg->type].races) / sizeof(TerrainDefs[nreg->type].races[0]);
                        for (int i=0; i<rnum; i++) {
                            if (TerrainDefs[nreg->type].races[i] == reg->race) ch++;
                        }
                    }
                    if (ch > 3) nreg->race = reg->race;
                }
            }
        }
    }
}

void ARegionList::FinalSetup(ARegionArray *pArr)
{
    int x, y;
    for (x = 0; x < pArr->x; x++) {
        for (y = 0; y < pArr->y; y++) {
            ARegion *reg = pArr->GetRegion(x, y);
            if (!reg) continue;

            if ((TerrainDefs[reg->type].similar_type == R_OCEAN) && (reg->type != R_LAKE)) {
                if (pArr->levelType == ARegionArray::LEVEL_UNDERWORLD) {
                    reg->set_name("The Undersea");
                }
                else if (pArr->levelType == ARegionArray::LEVEL_UNDERDEEP) {
                    reg->set_name("The Deep Undersea");
                }
                else {
                    std::string ocean_name = Globals->WORLD_NAME;
                    ocean_name += " Ocean";
                    reg->set_name(ocean_name);
                }
            } else if (TerrainDefs[reg->type].similar_type == R_BARREN) {
                reg->set_name("The Barrens");
            } else {
                if (reg->wages == -1)
                    reg->set_name("The Void");
                else if (reg->wages != -2)
                    reg->set_name(AGetNameString(reg->wages));
                else
                    reg->wages = -1;
            }

            reg->Setup();
        }
    }
}

void ARegionList::MakeShaft(ARegion *reg, ARegionArray *pFrom, ARegionArray *pTo)
{
    if (TerrainDefs[reg->type].similar_type == R_OCEAN) return;
    if (reg->type == R_BARREN) return;

    int tempx = reg->xloc * pTo->x / pFrom->x + rng::get_random(pTo->x / pFrom->x);
    int tempy = reg->yloc * pTo->y / pFrom->y + rng::get_random(pTo->y / pFrom->y);
    //
    // Make sure we get a valid region.
    //
    tempy += (tempx + tempy) % 2;

    ARegion *temp = pTo->GetRegion(tempx, tempy);
    if (!temp) return;
    if (TerrainDefs[temp->type].similar_type == R_OCEAN) return;
    if (temp->type == R_BARREN) return;

    Object *o = new Object(reg);
    o->num = reg->buildingseq++;
    o->set_name("Shaft");
    o->type = O_SHAFT;
    o->incomplete = 0;
    o->inner = temp->num;
    reg->objects.push_back(o);

    o = new Object(temp);
    o->num = temp->buildingseq++;
    o->set_name("Shaft");
    o->type = O_SHAFT;
    o->incomplete = 0;
    o->inner = reg->num;
    temp->objects.push_back(o);
}

void ARegionList::MakeShaftLinks(int levelFrom, int levelTo, int odds)
{
    ARegionArray *pFrom = pRegionArrays[levelFrom];
    ARegionArray *pTo = pRegionArrays[levelTo];

    int x, y;
    for (x = 0; x < pFrom->x; x++) {
        for (y = 0; y < pFrom->y; y++) {
            ARegion *reg = pFrom->GetRegion(x, y);
            if (!reg) continue;

            if (rng::get_random(odds) != 0) continue;

            MakeShaft(reg, pFrom, pTo);
        }
    }
}

void ARegionList::SetACNeighbors(int levelSrc, int levelTo, int maxX, int maxY)
{
    ARegionArray *ar = GetRegionArray(levelSrc);

    for (int x = 0; x < ar->x; x++) {
        for (int y = 0; y < ar->y; y++) {
            ARegion *AC = ar->GetRegion(x, y);
            if (!AC) continue;
            if (Globals->START_CITIES_EXIST) {
                for (int i=0; i<NDIRS; i++) {
                    if (AC->neighbors[i]) continue;
                    ARegion *pReg = GetStartingCity(AC, i, levelTo, maxX, maxY);
                    if (!pReg) continue;
                    AC->neighbors[i] = pReg;
                    pReg->MakeStartingCity();
                    if (Globals->GATES_EXIST) {
                        numberofgates++;
                    }
                }
            }
            else
            {
                // Rimefall keys its gateways on LATITUDE BAND, not on terrain. NewOrigins builds
                // one gateway per terrain type; this builds one per band, and behind each band a
                // set of start slots sized by the density curve in rimefall.h.
                //
                // The curation is inherited rather than reinvented: get_starting_region_candidates
                // already requires wood, iron, stone, grain or livestock, and horses or camels
                // within two hexes on a landmass of at least ten hexes. Candidates of every
                // terrain are pooled and then bucketed by band, so a band's slots may sit on any
                // terrain it happens to have — which matters most in the far south, where there is
                // too little land to insist on a particular one.
                //
                // The spacing pass below is NewOrigins' own, applied within a band instead of
                // across terrains: up to fifty attempts maximising the minimum distance to slots
                // already chosen, stopping once it clears twenty.
                ARegionArray *to = GetRegionArray(levelTo);
                std::vector<std::vector<ARegion *>> by_band(RIMEFALL_BANDS);
                for (int type = R_PLAIN; type <= R_TUNDRA; type++) {
                    for (ARegion *cand : to->get_starting_region_candidates(type)) {
                        by_band[rimefall_band_of_row(to, cand->yloc)].push_back(cand);
                    }
                }

                //
                // THE CURVE IS WRITTEN FOR A 64x64 WORLD, so scale it by the land this one has
                // (0024). Twenty starts on ninety land hexes is not a tighter game, it is a
                // different one: every band becomes the crush zone and the middle band stops
                // having a role. A world with more land than the reference does not get more
                // slots — the curve is the design, and this only keeps a small world from being
                // asked to carry it.
                //
                int land = 0;
                for (int lx = 0; lx < to->x; lx++) {
                    for (int ly = 0; ly < to->y; ly++) {
                        ARegion *reg = to->GetRegion(lx, ly);
                        if (!reg) continue;
                        if (TerrainDefs[reg->type].similar_type == R_OCEAN) continue;
                        land++;
                    }
                }

                int curve_total = 0;
                for (int band = 0; band < RIMEFALL_BANDS; band++) curve_total += rimefall_starts_per_band[band];

                int target = (land + RIMEFALL_LAND_PER_START / 2) / RIMEFALL_LAND_PER_START;
                if (target > curve_total) target = curve_total;
                if (target < 1) target = 1;

                //
                // Largest remainder, so the scaled quotas add up to the target exactly and the
                // rounding loss lands where the curve is thinnest rather than always in the same
                // band. A tie goes to the band the curve weights more heavily, and then to the
                // northern one, which keeps the apportionment reproducible.
                //
                std::vector<int> quota(RIMEFALL_BANDS, 0);
                std::vector<int> remainder(RIMEFALL_BANDS, 0);
                int apportioned = 0;
                for (int band = 0; band < RIMEFALL_BANDS; band++) {
                    int exact = rimefall_starts_per_band[band] * target;
                    quota[band] = exact / curve_total;
                    remainder[band] = exact % curve_total;
                    apportioned += quota[band];
                }
                while (apportioned < target) {
                    int pick = -1;
                    for (int band = 0; band < RIMEFALL_BANDS; band++) {
                        if (remainder[band] < 0) continue;
                        if (pick < 0 || remainder[band] > remainder[pick] ||
                            (remainder[band] == remainder[pick] &&
                             rimefall_starts_per_band[band] > rimefall_starts_per_band[pick])) {
                            pick = band;
                        }
                    }
                    if (pick < 0) break;
                    quota[pick]++;
                    remainder[pick] = -1;
                    apportioned++;
                }

                logger::write(
                    "The surface holds " + std::to_string(land) + " land hexes, so the density "
                    "curve is scaled from " + std::to_string(curve_total) + " start slots to " +
                    std::to_string(target)
                );

                //
                // A SETTLED HEX FIRST (0023). get_starting_region_candidates weighs resources and
                // says nothing about towns, and the engine's occupancy cascade cannot repair that
                // here: its first three match levels are gated on the candidate having a town, and
                // Game::filter_gateway_destinations hands it one hex. So a start without a market
                // would be chosen at world creation and nothing downstream would notice.
                //
                // The market is the whole difference. Recruiting belongs to the region rather than
                // the town (0016), so a townless start still raises men — but it can sell nothing
                // where it stands. What is left townless after this gets a village founded on it,
                // in Game::CreateWorld once the slots are placed.
                //
                std::vector<std::vector<ARegion *>> settled(RIMEFALL_BANDS), wild(RIMEFALL_BANDS);
                for (int band = 0; band < RIMEFALL_BANDS; band++) {
                    for (ARegion *cand : by_band[band]) {
                        if (cand->town) settled[band].push_back(cand);
                        else wild[band].push_back(cand);
                    }
                    logger::write(
                        "Band " + std::to_string(band) + " (" + rimefall_band_name(band) + "): " +
                        std::to_string(by_band[band].size()) + " candidates for " +
                        std::to_string(quota[band]) + " start slots, " +
                        std::to_string(settled[band].size()) + " of them settled"
                    );
                }

                std::vector<ARegion *> chosen;
                std::vector<int> chosen_band;

                //
                // FARTHEST POINT, NOT FIFTY GUESSES (0024). Take the candidate whose nearest
                // already-placed start is furthest away, every time. The sampler this replaces
                // drew fifty candidates at random and kept the best of them, which is a good
                // approximation on a large pool and no approximation at all on a small one, where
                // it kept drawing the same handful of hexes.
                //
                // Only the first slot in the world is drawn at random, because there is nothing
                // for it to be far from yet, and without it every world would start its layout in
                // the same corner of the map. Everything after it is determined by the map.
                //
                auto take_from = [&](int band) -> ARegion * {
                    std::vector<ARegion *>& pool = settled[band].empty() ? wild[band] : settled[band];
                    if (pool.empty()) return nullptr;

                    size_t index = 0;
                    if (chosen.empty()) {
                        index = rng::get_random(pool.size());
                    } else {
                        int best = -1;
                        for (size_t i = 0; i < pool.size(); i++) {
                            int nearest = 1000;
                            for (ARegion *other : chosen) {
                                int d = GetPlanarDistance(pool[i], other, 0);
                                if (d < nearest) nearest = d;
                            }
                            if (nearest > best) { best = nearest; index = i; }
                        }
                    }

                    ARegion *picked = pool[index];
                    pool.erase(pool.begin() + index);
                    chosen.push_back(picked);
                    chosen_band.push_back(band);
                    return picked;
                };

                std::vector<int> placed(RIMEFALL_BANDS, 0);
                for (int band = 0; band < RIMEFALL_BANDS; band++) {
                    for (int n = 0; n < quota[band]; n++) {
                        if (!take_from(band)) break;
                        placed[band]++;
                    }
                }

                //
                // A BAND THAT CANNOT FILL ITS QUOTA HANDS IT ON (0024), to the nearest band that
                // still has candidates. A band comes up empty when its own land grows nothing the
                // candidate test accepts — the far south rolls jungle, its only wood, at 4 in 64,
                // and `get_starting_region_candidates` requires wood outright. Rare, and it used
                // to cost the world a start location outright: one fewer faction could ever join,
                // for a reason no player could see.
                //
                // The band still offers nothing in such a world. What moves is the slot, not the
                // shortage, and the log says where it went.
                //
                for (int band = 0; band < RIMEFALL_BANDS; band++) {
                    int missing = quota[band] - placed[band];
                    if (missing <= 0) continue;
                    logger::write(
                        "  " + rimefall_band_name(band) + " could not fill " +
                        std::to_string(missing) + " of its " + std::to_string(quota[band]) +
                        " slots"
                    );
                    while (missing > 0) {
                        int from = -1;
                        int from_distance = RIMEFALL_BANDS + 1;
                        int from_left = -1;
                        for (int other = 0; other < RIMEFALL_BANDS; other++) {
                            if (other == band) continue;
                            int left = (int) settled[other].size() + (int) wild[other].size();
                            if (left < 1) continue;
                            int d = other > band ? other - band : band - other;
                            if (d < from_distance || (d == from_distance && left > from_left)) {
                                from = other;
                                from_distance = d;
                                from_left = left;
                            }
                        }
                        if (from < 0) {
                            logger::write("  nowhere left in the world to place them");
                            break;
                        }
                        if (!take_from(from)) break;
                        placed[from]++;
                        missing--;
                        logger::write("  one of them goes to " + rimefall_band_name(from));
                    }
                }

                //
                // What is left townless gets a village in Game::CreateWorld (0023). Reported here
                // because this is where the count is known.
                //
                int townless = 0;
                for (ARegion *reg : chosen) if (!reg->town) townless++;
                if (townless > 0) {
                    logger::write(
                        std::to_string(townless) + " of the placed slots have no settlement and "
                        "will have a village founded for them"
                    );
                }

                // ONE GATEWAY OBJECT PER START SLOT, and this is what makes the slot registry
                // work at all.
                //
                // The slots have to survive a save cycle, and there is nowhere else to put them:
                // rulesetSpecificData is not persisted (0011), and ARegion::Writeout has no field
                // for "this hex is a start slot" — adding one would change the game.in format,
                // which is a published interface. Objects ARE persisted, so the set of gateway
                // objects in the nexus IS the slot registry, recovered by reading them back.
                //
                // A gateway's BAND IS DERIVED FROM ITS DESTINATION'S LATITUDE, never parsed back
                // out of the name. The name is for players and may be reworded freely.
                //
                // Created once here and never destroyed or recreated. 0010's consequences call
                // that out: these carry numbers from buildingseq, and churning them would move
                // object numbers around in reports and in the JSON. CheckVictory renames a slot
                // that has been taken instead of removing it, which also shows players the world
                // filling up rather than silently shrinking the list.
                for (size_t i = 0; i < chosen.size(); i++) {
                    Object *o = new Object(AC);
                    o->num = AC->buildingseq++;
                    // Set the type first: set_name() refuses to rename an O_DUMMY object,
                    // which is what the constructor leaves it as.
                    o->type = O_GATEWAY;
                    // A comma, not brackets. filter::legal_characters drops '(' and ')' outright,
                    // and filter::strip_number cuts everything from the last '(' when a save file
                    // is read back — so a parenthesised terrain would survive generation and then
                    // be truncated on the next load.
                    o->set_name(
                        "Gateway to " + rimefall_band_name(chosen_band[i]) +
                        ", " + std::string(TerrainDefs[chosen[i]->type].name)
                    );
                    o->incomplete = 0;
                    o->inner = chosen[i]->num;
                    AC->objects.push_back(o);
                }
                logger::write("Placed " + std::to_string(chosen.size()) + " start slots in total");
            }
        }
    }
}

void ARegionList::InitSetupGates(int level)
{

    if (!Globals->GATES_EXIST) return;

    ARegionArray *pArr = pRegionArrays[level];

    int i, j, k;
    for (i=0; i<pArr->x / 8; i++) {
        for (j=0; j<pArr->y / 16; j++) {
            for (k=0; k<5; k++) {
                int tempx = i*8 + rng::get_random(8);
                int tempy = j*16 + rng::get_random(8)*2 + tempx%2;
                ARegion *temp = pArr->GetRegion(tempx, tempy);
                if (temp && TerrainDefs[temp->type].similar_type != R_OCEAN &&
                        temp->type != R_VOLCANO && temp->type != R_BARREN &&
                        temp->gate != -1) {
                    numberofgates++;
                    temp->gate = -1;
                    break;
                }
            }
        }
    }
}

void ARegionList::FixUnconnectedRegions()
{
    ARegion *head, *tail, *neighbors[NDIRS], *n;
    int attempts, max, i, j, count, offset, x, y, xscale, yscale;
    Object *o;

    for(const auto r : regions) {
        r->distance = -1;
        r->next = 0;
    }

    // Build a list of all the regions we know we can get to:
    // The nexus and anywhere that has a gate
    head = 0;
    tail = 0;
    for(const auto r : regions) {
        if (r->zloc == ARegionArray::LEVEL_NEXUS || r->gate == -1) {
            r->distance = 0;
            r->next = head;
            head = r;
            if (!tail)
                tail = r;
        }
    }
    while (head) {
        tail = FindConnectedRegions(head, tail, 1);
        head = head->next;
    }
    attempts = 0;
    do {
        max = 0;
        count = 0;
        for(const auto r : regions) {
            if (r->distance == -1) {
                count++;
            }
            if (r->distance > max)
                max = r->distance;
        }
        if (count > 0) {
            ARegion *target = nullptr;
            i = rng::get_random(count);
            for(const auto r : regions) {
                if (r->distance == -1) {
                    if (!i) {
                        target = r;
                        break;
                    }
                    i--;
                }
            }
            // Found an unconnected region
            // Try to link it in
            n = 0;
            // first, see if we can knock down a wall
            // sadly we can only knock down all the walls at once
            for (i = 0; i < NDIRS; i++)
                neighbors[i] = target->neighbors[i];
            if (Globals->ICOSAHEDRAL_WORLD) {
                IcosahedralNeighSetup(target, pRegionArrays[target->zloc]);
            } else {
                NeighSetup(target, pRegionArrays[target->zloc]);
            }
            offset = rng::get_random(NDIRS);
            for (i = 0; i < NDIRS; i++) {
                if (target->neighbors[(i + offset) % NDIRS] &&
                        target->neighbors[(i + offset) % NDIRS]->distance != -1) {
                    break;
                }
            }
            for (j = 0; j < NDIRS; j++) {
                // restore all the walls other than the one
                // we meant to break
                if (i != j)
                    target->neighbors[(j + offset) % NDIRS] = neighbors[(j + offset) % NDIRS];
            }
            if (i < NDIRS) {
                // also restore the link on the other side
                n = target->neighbors[(i + offset) % NDIRS];
                for (j = 0; j < NDIRS; j++)
                    neighbors[j] = n->neighbors[j];
                if (Globals->ICOSAHEDRAL_WORLD) {
                    IcosahedralNeighSetup(n, pRegionArrays[target->zloc]);
                } else {
                    NeighSetup(n, pRegionArrays[n->zloc]);
                }
                for (j = 0; j < NDIRS; j++)
                    if (n->neighbors[j] != target)
                        n->neighbors[j] = neighbors[j];
            } else if (TerrainDefs[target->type].similar_type != R_OCEAN) {
                // couldn't break a wall
                // so try to put in a shaft
                if (target->zloc > ARegionArray::LEVEL_SURFACE) {
                    x = target->xloc * GetLevelXScale(target->zloc) / GetLevelXScale(target->zloc - 1);
                    y = target->yloc * GetLevelYScale(target->zloc) / GetLevelYScale(target->zloc - 1);
                    xscale = GetLevelXScale(target->zloc) / GetLevelXScale(target->zloc - 1);
                    yscale = 2 * GetLevelYScale(target->zloc) / GetLevelYScale(target->zloc - 1);
                    for (i = 0; !n && i < xscale; i++)
                        for (j = 0; !n && j < yscale; j++) {
                            n = pRegionArrays[target->zloc - 1]->GetRegion(x + i, y + j);
                            if (n && TerrainDefs[n->type].similar_type == R_OCEAN)
                                n = 0;
                        }
                    if (n) {
                        o = new Object(n);
                        o->num = n->buildingseq++;
                        o->set_name("Shaft");
                        o->type = O_SHAFT;
                        o->incomplete = 0;
                        o->inner = target->num;
                        n->objects.push_back(o);

                        o = new Object(target);
                        o->num = target->buildingseq++;
                        o->set_name("Shaft");
                        o->type = O_SHAFT;
                        o->incomplete = 0;
                        o->inner = n->num;
                        target->objects.push_back(o);
                    }
                }
                if (!n) {
                    // None of that worked
                    // can we put in a gate?
                    if (Globals->GATES_EXIST &&
                            !rng::get_random(10)) {
                        target->gate = -1;
                        target->distance = 0;
                        n = target;
                    }
                }
            }
            if (n) {
                head = n;
                head->next = 0;
                tail = head;
                while (head) {
                    tail = FindConnectedRegions(head, tail, 1);
                    head = head->next;
                }
                attempts = 0;
            }
        }
        attempts++;
    } while (count > 0 && attempts < 1000);

    if (count > 0) {
        printf("Unable to link up %d hexes!\n", count);
    }
    printf("Maximum distance from the Nexus: %d.\n", max);
}

void ARegionList::FinalSetupGates()
{
    int ngates, log10, *used, i;

    if (!Globals->GATES_EXIST) return;

    ngates = numberofgates;

    if (Globals->DISPERSE_GATE_NUMBERS) {
        log10 = 0;
        while (ngates > 0) {
            ngates /= 10;
            log10++;
        }
        ngates = 10;
        while (log10 > 0) {
            ngates *= 10;
            log10--;
        }
    }

    used = new int[ngates];

    for (i = 0; i < ngates; i++)
        used[i] = 0;

    for(const auto r : regions) {
        if (r->gate == -1) {
            int index = rng::get_random(ngates);
            while (used[index]) {
                if (Globals->DISPERSE_GATE_NUMBERS) {
                    index = rng::get_random(ngates);
                } else {
                    index++;
                    index = index % ngates;
                }
            }
            r->gate = index+1;
            used[index] = 1;
            // setting up gatemonth
            r->gatemonth = rng::get_random(12);
        }
    }
    delete[] used;
}
