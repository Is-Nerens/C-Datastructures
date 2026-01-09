#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct Stringmap
{
    uint8_t* occupancy;
    void* map;
    uint32_t mapCapacity;
    uint32_t itemSize;
    uint32_t itemCount;
    uint32_t maxProbes;
} Stringmap;

void StringmapFree(Stringmap* map)
{
    if (!map) return;
    if (map->occupancy) free(map->occupancy);
    if (map->map) free(map->map);
    map->occupancy = NULL;
    map->map = NULL;
    map->mapCapacity = 0;
    map->itemSize = 0;
    map->itemCount = 0;
    map->maxProbes = 0;
}

int StringmapInit(Stringmap* map, uint32_t itemSize, uint32_t mapCapacity, uint32_t chunkCapacity)
{
    // create hashmap
    map->mapCapacity = mapCapacity;
    uint32_t occupancyRemainder = map->mapCapacity & 7;
    uint32_t occupancyBytes = map->mapCapacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    map->occupancy = (uint8_t*)calloc(occupancyBytes, 1); 
    if (map->occupancy == NULL) {
        StringmapFree(map); return 0;
    }
    map->map = malloc((sizeof(char*) + itemSize) * mapCapacity);
    map->itemSize = itemSize;
    map->itemCount = 0;
    map->maxProbes = 1;

    // success
    return 1;
}

uint32_t StringmapHash(char* string) {
    uint32_t hash = 2166136261u;
    for (uint8_t* p = (uint8_t*)string; *p; p++) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash;
}

static inline uint8_t StringmapSlotPresent(Stringmap* map, uint32_t i)
{
    return map->occupancy[i >> 3] & (1u << (i & 7));
}

static inline void StringmapMarkSlot(Stringmap* map, uint32_t i)
{
    map->occupancy[i >> 3] |= (uint8_t)(1 << (i & 7));
}

static inline void StringmapClearSlot(Stringmap* map, uint32_t i)
{
    map->occupancy[i >> 3] &= ~(1u << (i & 7));
}

int StringmapGrowRehash(Stringmap* map)
{
    uint32_t oldMapCapacity = map->mapCapacity;
    uint8_t* oldOccupancy = map->occupancy;
    void* oldMap = map->map;

    // reisze
    map->mapCapacity *= 2;
    uint32_t occupancyRemainder = map->mapCapacity & 7;
    uint32_t occupancyBytes = map->mapCapacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    map->occupancy = (uint8_t*)calloc(occupancyBytes, 1);
    if (map->occupancy == NULL) {
        map->mapCapacity = oldMapCapacity;
        map->occupancy = oldOccupancy;
        return 0;
    }
    map->map = malloc((sizeof(char*) + map->itemSize) * map->mapCapacity);
    if (map->map == NULL) {
        free(map->occupancy);
        map->mapCapacity = oldMapCapacity;
        map->occupancy = oldOccupancy;
        map->map = oldMap;
        return 0;
    }
    map->maxProbes = 1;
    
    // re-insert items
    for (uint32_t j=0; j<oldMapCapacity; j++) {
        if (oldOccupancy[j >> 3] & (1u << (j & 7)))
        {
            char* oldBase = (char*)oldMap + j * (sizeof(char*) + map->itemSize);
            char* key  = *(char**)oldBase;
            void* value = oldBase + sizeof(char*);

            // add item
            uint32_t probes = 0;
            uint32_t hash = StringmapHash(key);
            while(probes < map->mapCapacity) {
                uint32_t i = (hash + probes) % map->mapCapacity;
                if (!StringmapSlotPresent(map, i)) {
                    char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
                    memcpy(base, &key, sizeof(char*));
                    memcpy(base + sizeof(char*), value, map->itemSize);
                    StringmapMarkSlot(map, i);
                    break;
                }
                probes++;
            }
            if (probes + 1 > map->maxProbes) map->maxProbes = probes + 1;
        }
    }
    return 1;
}

int StringmapSet(Stringmap* map, char* key, void* value)
{
    // resize if surpassed max load factor
    if (map->itemCount * 10 > map->mapCapacity * 7) {
        if (!StringmapGrowRehash(map)) {
            return 0;
        }
    }

    uint32_t probes = 0;
    uint32_t hash = StringmapHash(key);
    while(probes < map->mapCapacity) {
        uint32_t i = (hash + probes) % map->mapCapacity;

        // if key exists -> update value
        if (StringmapSlotPresent(map, i)) {
            char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
            char* storedKey = *(char**)base;
            if (strcmp(key, storedKey) == 0) {
                memcpy(base + sizeof(char*), value, map->itemSize);
                return 1;
            }
        }
        else
        {
            StringmapMarkSlot(map, i);
            uint32_t len = strlen(key);
            char* storedKey = (char*)malloc(len + 1);
            memcpy(storedKey, key, len); storedKey[len] = '\0';
            if (storedKey == NULL) return 0;

            char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
            memcpy(base, &storedKey, sizeof(char*));
            memcpy(base + sizeof(char*), value, map->itemSize);
            map->itemCount++;
            break;
        }
        probes++;
    }
    if (probes + 1 > map->maxProbes) map->maxProbes = probes + 1;
    return 1;
}

void* StringmapGet(Stringmap* map, char* key)
{
    uint32_t probes = 0;
    uint32_t hash = StringmapHash(key);
    while(probes < map->mapCapacity) {
        uint32_t i = (hash + probes) % map->mapCapacity;
        if (StringmapSlotPresent(map, i)) {
            char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
            char* storedKey = *(char**)base;
            if (strcmp(key, storedKey) == 0) {
                return (char*)base + sizeof(char*);
            }
        }
        probes++;
    }
    return NULL;
}

int StringmapContains(Stringmap* map, char* key)
{
    uint32_t probes = 0;
    uint32_t hash = StringmapHash(key);
    while(probes < map->maxProbes) {
        uint32_t i = (hash + probes) % map->mapCapacity;
        if (StringmapSlotPresent(map, i)) {
            char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
            char* storedKey = *(char**)base;
            if (strcmp(key, storedKey) == 0) {
                return 1;
            }
        }
        probes++;
    }
    return 0;
}

void StringmapDelete(Stringmap* map, char* key)
{
    uint32_t probes = 0;
    uint32_t hash = StringmapHash(key);

    int holeIndex = -1;
    while(probes < map->mapCapacity) {
        uint32_t i = (hash + probes) % map->mapCapacity;
        if (StringmapSlotPresent(map, i)) {
            char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
            char* storedKey = *(char**)base;
            if (strcmp(key, storedKey) == 0) {
                holeIndex = (int)i;
                StringmapClearSlot(map, i);
                free(storedKey);
                break;
            }
        }
        else break;
        probes++;
    }
    
    if (holeIndex == -1) return; // key not found

    uint32_t i = (holeIndex + 1) % map->mapCapacity;
    while (StringmapSlotPresent(map, i))
    {
        char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
        char* candidateKey = *(char**)base;
        uint32_t candidateHash = StringmapHash(candidateKey);
        uint32_t candidateHome = candidateHash % map->mapCapacity;

        // can the candidate move into the hole?
        int canMoveCandidate;
        if (holeIndex <= i)
            canMoveCandidate = (candidateHome <= holeIndex || candidateHome > i);
        else
            canMoveCandidate = (candidateHome <= holeIndex && candidateHome > i);

        if (!canMoveCandidate) {
            i = (i + 1) % map->mapCapacity;
            continue;
        }

        // move candidate into the hole
        memcpy(
            (char*)map->map + holeIndex * (sizeof(char*) + map->itemSize),
            base,
            sizeof(char*) + map->itemSize
        );

        StringmapClearSlot(map, i);
        StringmapMarkSlot(map, holeIndex);

        holeIndex = i;
        i = (i + 1) % map->mapCapacity;
    }

    map->itemCount--;

}
