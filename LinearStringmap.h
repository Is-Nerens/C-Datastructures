#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct LinearStringmapChunk
{
    char* buffer;
    uint32_t capacity;
    uint32_t used;
} LinearStringmapChunk;

typedef struct LinearStringmap
{
    // chunk array
    LinearStringmapChunk* chunkArray;
    uint32_t chunkArraySize;
    uint32_t chunkArrayCapacity;
    uint32_t allocSearchStart;
    uint32_t totalChunkCapacity;
    
    // hashmap
    uint8_t* occupancy;
    void* map;
    uint32_t mapCapacity;
    uint32_t itemSize;
    uint32_t itemCount;
    uint32_t maxProbes;
} LinearStringmap;

void LinearStringmapFree(LinearStringmap* map)
{
    if (!map) return;
    if (map->chunkArray) free(map->chunkArray);
    if (map->occupancy) free(map->occupancy);
    if (map->map) free(map->map);
    map->chunkArray = NULL;
    map->occupancy = NULL;
    map->map = NULL;
    map->chunkArraySize = 0;
    map->chunkArrayCapacity = 0;
    map->allocSearchStart = 0;
    map->totalChunkCapacity = 0;
    map->mapCapacity = 0;
    map->itemSize = 0;
    map->itemCount = 0;
    map->maxProbes = 0;
}

int LinearStringmapInit(LinearStringmap* map, uint32_t itemSize, uint32_t mapCapacity, uint32_t chunkCapacity)
{
    // create chunk buffer array
    if (chunkCapacity < 512) chunkCapacity = 512;
    map->chunkArrayCapacity = 8;
    map->chunkArray = (LinearStringmapChunk*)malloc(sizeof(LinearStringmapChunk) * map->chunkArrayCapacity);
    if (map->chunkArray == NULL) return 0;
    map->chunkArraySize = 1;
    map->allocSearchStart = 0;
    map->totalChunkCapacity = chunkCapacity;

    // create first chunk
    map->chunkArray[0].buffer = (char*)malloc(chunkCapacity);
    if (map->chunkArray[0].buffer == NULL) {
        LinearStringmapFree(map); return 0;
    }
    map->chunkArray[0].capacity = chunkCapacity;
    map->chunkArray[0].used = 0;

    // create hashmap
    map->mapCapacity = mapCapacity;
    uint32_t occupancyRemainder = map->mapCapacity & 7;
    uint32_t occupancyBytes = map->mapCapacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    map->occupancy = (uint8_t*)calloc(occupancyBytes, 1); 
    if (map->occupancy == NULL) {
        LinearStringmapFree(map); return 0;
    }
    map->map = malloc((sizeof(char*) + itemSize) * mapCapacity);
    map->itemSize = itemSize;
    map->itemCount = 0;
    map->maxProbes = 1;

    // success
    return 1;
}

uint32_t LinearStringmapHash(char* string) {
    uint32_t hash = 2166136261u;
    for (uint8_t* p = (uint8_t*)string; *p; p++) {
        hash ^= *p;
        hash *= 16777619u;
    }
    return hash;
}

static inline uint8_t LinearStringmapSlotPresent(LinearStringmap* map, uint32_t i)
{
    return map->occupancy[i >> 3] & (1u << (i & 7));
}

static inline void LinearStringmapMarkSlot(LinearStringmap* map, uint32_t i)
{
    map->occupancy[i >> 3] |= (uint8_t)(1 << (i & 7));
}

static inline void LinearStringmapClearSlot(LinearStringmap* map, uint32_t i)
{
    map->occupancy[i >> 3] &= ~(1u << (i & 7));
}

int LinearStringmapGrowRehash(LinearStringmap* map)
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
            uint32_t hash = LinearStringmapHash(key);
            while(probes < map->mapCapacity) {
                uint32_t i = (hash + probes) % map->mapCapacity;
                if (!LinearStringmapSlotPresent(map, i)) {
                    char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
                    memcpy(base, &key, sizeof(char*));
                    memcpy(base + sizeof(char*), value, map->itemSize);
                    LinearStringmapMarkSlot(map, i);
                    break;
                }
                probes++;
            }
            if (probes + 1 > map->maxProbes) map->maxProbes = probes + 1;
        }
    }
    return 1;
}

char* LinearStringmapAddKey(LinearStringmap* map, char* key)
{
    char* storedKey;
    uint32_t keyLen = strlen(key);
    
    // search chunks for space
    for (uint32_t i=map->allocSearchStart; i<map->chunkArraySize; i++) {
        LinearStringmapChunk* chunk = &map->chunkArray[i];
        if (chunk->capacity - chunk->used >= keyLen + 1) {
            storedKey = (char*)chunk->buffer + chunk->used;
            memcpy(storedKey, key, keyLen);
            storedKey[keyLen] = '\0';
            chunk->used += keyLen + 1;
            return storedKey;
        }
        else if (chunk->capacity - chunk->used < 20)
        {
            map->allocSearchStart = i + 1;
        }
    }

    // no space? -> add new chunk
    if (map->chunkArrayCapacity == map->chunkArraySize) {
        map->chunkArrayCapacity *= 2;
        map->chunkArray = (LinearStringmapChunk*)realloc(map->chunkArray, sizeof(LinearStringmapChunk) * map->chunkArrayCapacity);
        if (map->chunkArray == NULL) return NULL;
    }
    map->chunkArraySize++;
    uint32_t newChunkCap = map->totalChunkCapacity;
    if (newChunkCap < keyLen * 2) newChunkCap = keyLen * 2;
    map->totalChunkCapacity += newChunkCap;
    LinearStringmapChunk* newChunk = &map->chunkArray[map->chunkArraySize - 1];
    newChunk->capacity = newChunkCap;
    newChunk->buffer = (char*)malloc(newChunkCap);
    if (newChunk->buffer == NULL) return NULL;
    newChunk->used = keyLen + 1;

    // copy key into new chunk
    storedKey = (char*)newChunk->buffer;
    memcpy(storedKey, key, keyLen);
    storedKey[keyLen] = '\0';
    return storedKey;
}

int LinearStringmapSet(LinearStringmap* map, char* key, void* value)
{
    // resize if surpassed max load factor
    if (map->itemCount * 10 > map->mapCapacity * 7) {
        if (!LinearStringmapGrowRehash(map)) {
            return 0;
        }
    }

    uint32_t probes = 0;
    uint32_t hash = LinearStringmapHash(key);
    while(probes < map->mapCapacity) {
        uint32_t i = (hash + probes) % map->mapCapacity;

        // if key exists -> update value
        if (LinearStringmapSlotPresent(map, i)) {
            char* base = (char*)map->map + i * (sizeof(char*) + map->itemSize);
            char* storedKey = *(char**)base;
            if (strcmp(key, storedKey) == 0) {
                memcpy(base + sizeof(char*), value, map->itemSize);
                return 1;
            }
        }
        else
        {
            LinearStringmapMarkSlot(map, i);
            char* storedKey = LinearStringmapAddKey(map, key);
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

void* LinearStringmapGet(LinearStringmap* map, char* key)
{
    uint32_t probes = 0;
    uint32_t hash = LinearStringmapHash(key);
    while(probes < map->mapCapacity) {
        uint32_t i = (hash + probes) % map->mapCapacity;
        if (LinearStringmapSlotPresent(map, i)) {
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

int LinearStringmapContains(LinearStringmap* map, char* key)
{
    uint32_t probes = 0;
    uint32_t hash = LinearStringmapHash(key);
    while(probes < map->maxProbes) {
        uint32_t i = (hash + probes) % map->mapCapacity;
        if (LinearStringmapSlotPresent(map, i)) {
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