// MIT License
// Copyright (c) 2026 Arran Stevens

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#pragma once
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ------------------------------------------------------
// --- Datastructure For Mapping (generic -> generic) ---
// ------------------------------------------------------
typedef struct Hashmap
{
    uint8_t* occupancy;
    void* data;
    uint32_t keySize;
    uint32_t itemSize;
    uint32_t itemCount;
    uint32_t capacity;
    uint32_t maxProbes;
    uint32_t iterateIndex;
} Hashmap;

void HashmapInit(Hashmap* hmap, uint32_t keySize, uint32_t itemSize, uint32_t capacity)
{
    if (capacity < 10) capacity = 10; // ensure capacity for at least 10 elements

    // allocate space for occupancy bit array
    uint32_t occupancyRemainder = capacity & 7;
    uint32_t occupancyBytes = capacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    hmap->occupancy = (uint8_t*)calloc(occupancyBytes, 1); 

    // allocate space for sparse data array
    hmap->data = malloc((keySize + itemSize) * capacity);

    // initialise tracking variables
    hmap->keySize = keySize;
    hmap->itemSize = itemSize;
    hmap->itemCount = 0;
    hmap->capacity = capacity;
    hmap->maxProbes = 1;
}

// FNV algorithm https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
static uint32_t HashGeneric(void* key, uint32_t len)
{
    uint32_t hash = 2166136261u;
    uint8_t* p = (uint8_t*)key;
    for (uint32_t i=0; i<len; i++) {
        hash ^= p[i];
        hash *= 16777619u;
    }
    return hash;
}

inline uint8_t HashmapSlotPresent(Hashmap* hmap, uint32_t i)
{
    return hmap->occupancy[i >> 3] & (1u << (i & 7));
}

inline void HashmapMarkSlot(Hashmap* hmap, uint32_t i)
{
    hmap->occupancy[i >> 3] |= (uint8_t)(1 << (i & 7));
}

inline void HashmapClearSlot(Hashmap* hmap, uint32_t i)
{
    hmap->occupancy[i >> 3] &= ~(1u << (i & 7));
}

void HashmapResizeAdd(Hashmap* hmap, void* key, void* value)
{
    uint32_t probes = 0;
    uint32_t hash = HashGeneric(key, hmap->keySize);
    while (probes < hmap->capacity) {
        uint32_t i = (hash + probes) % hmap->capacity;

        if (!HashmapSlotPresent(hmap, i)) { // Found empty slot

            // set key and data
            char* base = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
            memcpy(base, key, hmap->keySize);
            memcpy(base + hmap->keySize, value, hmap->itemSize);

            // mark slot and increase item count
            HashmapMarkSlot(hmap, i);
            hmap->itemCount++;
            break;
        }
        probes++;
    }
    if (probes + 1 > hmap->maxProbes) hmap->maxProbes = probes + 1;
}

void HashmapResize(Hashmap* hmap)
{
    uint32_t oldCapacity = hmap->capacity;
    uint8_t* oldOccupancy = hmap->occupancy;
    void* oldData = hmap->data;

    // Resize
    hmap->capacity *= 2;
    uint32_t occupancyRemainder = hmap->capacity & 7;
    uint32_t occupancyBytes = hmap->capacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    hmap->occupancy = calloc(occupancyBytes, 1);
    hmap->data = malloc(hmap->capacity * (hmap->keySize + hmap->itemSize));
    hmap->maxProbes = 0;
    hmap->itemCount = 0;

    // Re-insert all old items
    for (uint32_t i=0; i<oldCapacity; i++) {

        uint8_t isPresent = oldOccupancy[i >> 3] & (1u << (i & 7));
        if (isPresent) // Found item
        { 
            char* base = (char*)oldData + i * (hmap->keySize + hmap->itemSize);
            void* key = base;
            void* value = base + hmap->keySize;
            HashmapResizeAdd(hmap, key, value); // Re-add item 
        }
    }

    free(oldOccupancy);
    free(oldData);
}

int HashmapContains(Hashmap* hmap, void* key)
{
    uint32_t probes = 0;
    uint32_t hash = HashGeneric(key, hmap->keySize);
    while (probes < hmap->maxProbes) {
        uint32_t i = (hash + probes) % hmap->capacity;
        if (Hashmap_SlotPresent(hmap, i)) { // Found item
            
            // Check if key matches
            char* base = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
            void* checkKey = base;
            if (memcmp(checkKey, key, hmap->keySize) == 0) {
                return 1;
            }
        } else {
            return 0;
        }
        probes++;
    }

    return 0;
}

void* HashmapGet(Hashmap* hmap, void* key)
{
    uint32_t probes = 0;
    uint32_t hash = HashGeneric(key, hmap->keySize);
    while (probes < hmap->maxProbes) {
        uint32_t i = (hash + probes) % hmap->capacity;
        if (HashmapSlotPresent(hmap, i)) { // Found item
            
            // Check if key matches
            char* base = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
            void* checkKey = base;
            if (memcmp(checkKey, key, hmap->keySize) == 0) {
                return base + hmap->keySize;
            }
        }
        else {
            return NULL;
        }
        probes++;
    }

    return NULL;
}

void HashmapSet(Hashmap* hmap, void* key, void* value)
{
    // Resize if surpassed max load factor 
    if ((float)hmap->itemCount / (float)hmap->capacity > 0.5f) {
        HashmapResize(hmap);
    }

    uint32_t probes = 0;
    uint32_t hash = HashGeneric(key, hmap->keySize);
    while (probes < hmap->capacity) {
        uint32_t i = (hash + probes) % hmap->capacity;

        if (HashmapSlotPresent(hmap, i)) {
            char* base = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
            if (memcmp(base, key, hmap->keySize) == 0) {
                memcpy(base + hmap->keySize, value, hmap->itemSize);
                return;
            }
        } 
        else 
        {
            HashmapMarkSlot(hmap, i);
            
            // set key and value
            char* base = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
            memcpy(base, key, hmap->keySize);
            memcpy(base + hmap->keySize, value, hmap->itemSize);

            hmap->itemCount++;
            break;
        }
        probes++;
    }
    if (probes + 1 > hmap->maxProbes) hmap->maxProbes = probes + 1;
}

void HashmapDelete(Hashmap* hmap, void* key)
{
    uint32_t probes = 0;
    uint32_t hash = HashGeneric(key, hmap->keySize);

    int holeIndex = -1;
    while(probes < hmap->maxProbes) 
    {
        uint32_t i = (hash + probes) % hmap->capacity;

        if (HashmapSlotPresent(hmap, i)) {
            void* checkKey = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
            if (memcmp(checkKey, key, hmap->keySize) == 0) {
                holeIndex = (int)i;
                HashmapClearSlot(hmap, i);
                break;
            }
        }
        else break;

        probes++;
    }

    if (holeIndex == -1) return; // key not found

    uint32_t i = (holeIndex + 1) % hmap->capacity;
    while (HashmapSlotPresent(hmap, i)) 
    {
        void* candidateKey = (char*)hmap->data + i * (hmap->keySize + hmap->itemSize);
        uint32_t candidateHash = HashGeneric(candidateKey, hmap->keySize);
        uint32_t candidateHome = candidateHash % hmap->capacity;

        // Can the candidate move into the hole?
        bool canMoveCandidate;
        if (holeIndex <= i)
            canMoveCandidate = (candidateHome <= holeIndex || candidateHome > i);
        else
            canMoveCandidate = (candidateHome <= holeIndex && candidateHome > i);

        if (!canMoveCandidate) {
            i = (i + 1) % hmap->capacity;
            continue;
        }

        // Move candidate into hole
        memcpy(
            (char*)hmap->data + holeIndex * (hmap->keySize + hmap->itemSize),
            candidateKey,
            hmap->keySize + hmap->itemSize
        );

        HashmapClearSlot(hmap, i);
        HashmapMarkSlot(hmap, holeIndex);

        holeIndex = i;
        i = (i + 1) % hmap->capacity;
    }

    hmap->itemCount--;
}

inline void HashmapIterateBegin(Hashmap* hmap)
{
    hmap->iterateIndex = 0;
}

inline int HashmapIterateContinue(Hashmap* hmap)
{   
    return hmap->iterateIndex < hmap->capacity;
}

void HashmapIterateGet(Hashmap* hmap, void** returnKey, void** returnVal)
{
    if (hmap->itemCount == 0) 
    {
        *returnKey = NULL;
        *returnVal = NULL;
    }

    // Probe until next item is found
    while(hmap->iterateIndex < hmap->capacity)
    {

        // Found item -> return pointer
        if (HashmapSlotPresent(hmap, hmap->iterateIndex)) { 
            char* base = (char*)hmap->data + hmap->iterateIndex * (hmap->keySize + hmap->itemSize);
            hmap->iterateIndex++;
            *returnKey = base;
            *returnVal = base + hmap->keySize;
            return;
        }

        hmap->iterateIndex++;
    }

    // Error
    *returnKey = NULL;
    *returnVal = NULL;
    return;
}

void HashmapClear(Hashmap* hmap)
{
    if (hmap->capacity == 0) return;
    uint32_t occupancyRemainder = hmap->capacity & 7;
    uint32_t occupancyBytes = hmap->capacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    memset(hmap->occupancy, 0, occupancyBytes);
    hmap->itemCount = 0;
    hmap->maxProbes = 0;
}

void HashmapFree(Hashmap* hmap)
{
    free(hmap->occupancy);
    free(hmap->data);
    hmap->occupancy = NULL;
    hmap->data = NULL;
    hmap->capacity = 0;
    hmap->itemCount = 0;
}
