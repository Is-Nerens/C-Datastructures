#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct DynamicArray
{
    void* data;  
    uint32_t elementSize;
    uint32_t size;     
    uint32_t capacity; 
} DynamicArray; 

void DynamicArrayInit (DynamicArray* arr, uint32_t elementSize, uint32_t initialCapacity) 
{
    arr->data = malloc(initialCapacity * elementSize);
    arr->elementSize = elementSize;
    arr->size = 0;
    arr->capacity = initialCapacity;
}

void DynamicArrayPush(DynamicArray* array, void* value) 
{
    if (array->size >= array->capacity) {
        array->capacity *= 2;
        array->data = realloc(array->data, array->capacity * array->elementSize);
        if (!array->data) {
            return;
        }
    }
    memcpy((char*)array->data + (array->size * array->elementSize), value, array->elementSize);
    array->size++;
}

void* DynamicArrayPushEmpty(DynamicArray* array)
{
    if (array->size >= array->capacity) {
        array->capacity *= 2;
        array->data = realloc(array->data, array->capacity * array->elementSize);
        if (!array->data) {
            return NULL;
        }
    }
    char* dst = (char*)array->data + (array->size * array->elementSize);
    array->size++;
    return dst;
}

void DynamicArrayDeleteBackfill(DynamicArray* array, uint32_t index)
{
    if (index >= array->size) {
        return;
    }
    if (index == array->size - 1) {
        array->size--;
        return;
    }
    void* target = (char*)array->data + (index * array->elementSize);
    void* last   = (char*)array->data + ((array->size - 1) * array->elementSize);
    memcpy(target, last, array->elementSize);
    array->size--;
}

inline void* DynamicArrayGet(DynamicArray* array, uint32_t index) 
{
    if (index >= array->size) {
        return NULL;
    }
    return (char*)array->data + (index * array->elementSize);
}

inline void DynamicArrayClear(DynamicArray* array)
{
    array->size = 0;
}

inline void DynamicArrayFree(DynamicArray* array) 
{
    free(array->data);
    array->size = array->capacity = 0;
    array->data = NULL;
}