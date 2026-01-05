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

typedef struct Set
{
    uint8_t* occupancy;
    void* data;
    uint32_t itemSize;
    uint32_t count;
    uint32_t capacity;
    uint32_t maxProbes;
} Set;

Set SetCreate(uint32_t itemSize, uint32_t capacity)
{
    Set set;
    if (capacity < 16) capacity = 16;

    // allocate occupancy bit array
    uint32_t occupancyRemainder = capacity & 7;
    uint32_t occupancyBytes = capacity >> 3;
    if (occupancyRemainder != 0) occupancyBytes += 1;
    set.occupancy = (uint8_t*)calloc(occupancyBytes, 1); 

    // allocate space for open address space
    set.data = malloc(itemSize * capacity);

    // init tracking variables
    set.itemSize = itemSize;
    set.count = 0;
    set.capacity = capacity;
    set.maxProbes = 1;
    return set;
}

// FNV algorithm https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp
static uint32_t SetHash(void* value, uint32_t len)
{
    uint32_t hash = 2166136261u;
    uint8_t* p = (uint8_t*)value;
    for (uint32_t i=0; i<len; i++) {
        hash ^= p[i];
        hash *= 16777619u;
    }
    return hash;
}

inline uint8_t SetSlotOccupied(Set* set, uint32_t i)
{
    return set->occupancy[i >> 3] & (1u << (i & 7));
}

inline void SetMarkSlot(Set* set, uint32_t i)
{
    set->occupancy[i >> 3] |= (uint8_t)(1 << (i & 7));
}

inline void SetFreeSlot(Set* set, uint32_t i)
{
    set->occupancy[i >> 3] &= ~(1u << (i & 7));
}

void SetFree(Set* set)
{
    if (set->occupancy) free(set->occupancy);
    if (set->data) free(set->data);
    set->itemSize = 0;
    set->count = 0;
    set->capacity = 0;
    set->maxProbes = 1;
}

void SetResize(Set* set)
{
    uint32_t oldCapacity = set->capacity;
    uint8_t* oldOccupancy = set->occupancy;
    void* oldData = set->data;

    // create larger buffers
    set->capacity *= 2;
    uint32_t occupancyRemainder = set->capacity & 7;
    uint32_t occupancyBytes = set->capacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    set->occupancy = calloc(occupancyBytes, 1);
    set->data = malloc(set->capacity * set->itemSize);
    set->count = 0;
    set->maxProbes = 1;

    // re-insert items
    for (uint32_t i=0; i<oldCapacity; i++) {
        uint8_t present = oldOccupancy[i >> 3] & (1u << (i & 7));
        if (present)
        {
            char* item = (char*)oldData + i * set->itemSize;

            // add item ======================================= //
            uint32_t probes = 0;
            uint32_t hash = SetHash(item, set->itemSize);
            while (probes < set->capacity) {
                uint32_t i = (hash + probes) % set->capacity;

                // found free slot -> copy item into slot
                if (!SetSlotOccupied(set, i)) {
                    char* dst = (char*)set->data + i * set->itemSize;
                    memcpy(dst, item, set->itemSize);
                    SetMarkSlot(set, i);
                    set->count++;
                    break;
                }
                probes++;
            }
            if (probes + 1 > set->maxProbes) set->maxProbes = probes + 1;
            // add item ======================================= //
        }
    }

    free(oldOccupancy);
    free(oldData);
}

void SetInsert(Set* set, void* item)
{
    // resize if surpased acceptable load factor
    if (set->count > (set->capacity * 0.6)) {
        SetResize(set);
    }

    uint32_t probes = 0;
    uint32_t hash = SetHash(item, set->itemSize);
    while(probes < set->capacity) {
        uint32_t i = (hash + probes) % set->capacity;

        if (SetSlotOccupied(set, i)) {
            char* dst = (char*)set->data + i * set->itemSize;

            // item already in set? update it
            if (memcmp(dst, item, set->itemSize) == 0) {
                memcpy(dst, item, set->itemSize);
            }
        }
        else {
            SetMarkSlot(set, i);

            char* dst = (char*)set->data + i * set->itemSize;
            memcpy(dst, item, set->itemSize);
            set->count++;
            break;
        }
        probes++;
    }
    if (probes + 1 > set->maxProbes) set->maxProbes = probes + 1;
}

void SetRemove(Set* set, void* item)
{
    uint32_t probes = 0;
    uint32_t hash = SetHash(item, set->itemSize);

    int holeIndex = -1;
    while(probes < set->maxProbes) {
        uint32_t i = (hash + probes) % set->capacity;

        if (SetSlotOccupied(set, i)) {
            void* checkItem = (char*)set->data + i * set->itemSize;
            if (memcmp(checkItem, item, set->itemSize) == 0) {
                holeIndex = (int)i;
                SetFreeSlot(set, i);
                break;
            }
        }
        else break;
        probes++;
    }

    if (holeIndex == -1) return; // item not in set

    uint32_t i = (holeIndex + 1) % set->capacity;
    while(SetSlotOccupied(set, i)) {
        void* candidateItem = (char*)set->data + i * set->itemSize;
        uint32_t candidateHash = SetHash(candidateItem, set->itemSize);
        uint32_t candidateHome = candidateHash % set->capacity;
        
        // can the candidate move into the hole?
        int canMoveCandidate;
        if (holeIndex <= i) {
            canMoveCandidate = (candidateHome <= holeIndex || candidateHome > i);
        }
        else {
            canMoveCandidate = (candidateHome <= holeIndex && candidateHome > i);
        }
        if (!canMoveCandidate) {
            i = (i + 1) % set->capacity;
            continue;
        }

        // move candidate into hole
        char* dst = (char*)set->data + holeIndex * set->itemSize;
        memcpy(dst, candidateItem, set->itemSize);
        SetFreeSlot(set, i);
        SetMarkSlot(set, holeIndex);

        // increment i
        holeIndex = i;
        i = (i + 1) % set->capacity;
    }
    set->count--;
}

int SetContains(Set* set, void* item)
{
    uint32_t probes = 0;
    uint32_t hash = SetHash(item, set->itemSize);
    while (probes < set->maxProbes) {
        uint32_t i = (hash + probes) % set->capacity;

        if (SetSlotOccupied(set, i)) {
            char* dst = (char*)set->data + i * set->itemSize;
            if (memcmp(dst, item, set->itemSize) == 0) {
                return 1;
            }
        }
        else return 0;
        probes++;
    }
    return 0;
}

void SetClear(Set* set)
{
    uint32_t occupancyRemainder = set->capacity & 7;
    uint32_t occupancyBytes = set->capacity >> 3;
    occupancyBytes += 1 * (occupancyRemainder != 0);
    memset(set->occupancy, 0, occupancyBytes);
    set->count = 0;
    set->maxProbes = 0;
}