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



// DATA LAYOUT
// ASCII/UTF8 [length: 3 bytes][encoding: 1 byte][data][\n]
// UTF32      [header: 3 bytes][encoding: 1 byte][data]

#pragma once
#include <stdint.h>
#include <string.h>


typedef char* String;
#define ASCII  0x01  // 00000001
#define UTF8   0x02  // 00000010
#define UTF32  0x03  // 00000011

String StringCreate(char* str)
{
    uint32_t length = strlen(str);
    String result = malloc(length + 4 + 1);
    if (!result) return NULL;
    memcpy(result + 4, str, length);
    result[0] = (length >> 16) & 0xFF;
    result[1] = (length >> 8) & 0xFF;
    result[2] = length & 0xFF;  
    result[3] = ASCII;
    result[length + 4] = '\0';
    return result;
}

String StringCreateUTF8(char* str)
{
    uint32_t length = strlen(str);
    String result = malloc(length + 4 + 1);
    if (!result) return NULL;
    memcpy(result + 4, str, length);
    result[0] = (length >> 16) & 0xFF;
    result[1] = (length >> 8) & 0xFF;
    result[2] = length & 0xFF;  
    result[3] = UTF8;
    result[length + 4] = '\0';
    return result;
}

inline void StringFree(String* str)
{
    if (str && *str) {
        free(*str);
        *str = NULL;
    }
}

inline char* StringCstr(String str)
{
    return str + 4;
}

inline uint32_t StringLen(String string)
{
    return ((uint32_t)(unsigned char)string[0] << 16) |
           ((uint32_t)(unsigned char)string[1] << 8) |
           ((uint32_t)(unsigned char)string[2]);
}

uint32_t GetNextUTF8Codepoint(String string, int* byteOffset)
{
    char* cstr = StringCstr(string);
    uint32_t stringLen = StringLen(string);
    uint32_t codepoint = 0;
    unsigned char firstByte = (unsigned char)cstr[*byteOffset];

    if ((firstByte & 0x80) == 0) {
        codepoint = firstByte;
        *byteOffset += 1;
    }
    else if (*byteOffset + 1 < stringLen && (firstByte & 0xE0) == 0xC0) {
        codepoint = (firstByte & 0x1F) << 6;
        codepoint |= (unsigned char)(cstr[*byteOffset + 1] & 0x3F);
        *byteOffset += 2;
    }
    else if (*byteOffset + 2 < stringLen && (firstByte & 0xF0) == 0xE0) {
        codepoint = (firstByte & 0x0F) << 12;
        codepoint |= (unsigned char)(cstr[*byteOffset + 1] & 0x3F) << 6;
        codepoint |= (unsigned char)(cstr[*byteOffset + 2] & 0x3F);
        *byteOffset += 3;
    }
    else if (*byteOffset + 3 < stringLen && (firstByte & 0xF8) == 0xF0) {
        codepoint = (firstByte & 0x07) << 18;
        codepoint |= (unsigned char)(cstr[*byteOffset + 1] & 0x3F) << 12;
        codepoint |= (unsigned char)(cstr[*byteOffset + 2] & 0x3F) << 6;
        codepoint |= (unsigned char)(cstr[*byteOffset + 3] & 0x3F);
        *byteOffset += 4;
    }
    return codepoint;
} 

inline uint32_t GetUTF32Codepoint(String string, int index) 
{
    uint32_t codepoint;
    memcpy(&codepoint, StringCstr(string) + index * 4, 4);
    return codepoint;
}

String StringConcat(String a, String b)
{
    // validate strings and get metadata
    if (a == NULL || b == NULL) return NULL;
    uint32_t aLen = StringLen(a);
    uint32_t bLen = StringLen(b);
    char aEncoding = a[3];
    char bEncoding = b[3];
    if (aEncoding != bEncoding || aLen == 0 || bLen == 0) return NULL;

    // allocate space for return string
    String output = NULL;
    output = malloc(aLen + bLen + 4 + 1);
    if (output == NULL) return NULL;

    // copy data from string a and b
    memcpy(output + 4, a + 4, aLen);
    memcpy(output + 4 + aLen, b + 4, bLen);

    // set metadata
    uint32_t outputLen = aLen + bLen;
    output[0] = (outputLen >> 16) & 0xFF;
    output[1] = (outputLen >> 8) & 0xFF;
    output[2] = outputLen & 0xFF;  
    output[3] = aEncoding;
    output[outputLen + 4] = '\0';
    return output;
}

int StringContains(String string, String search)
{
    // validate strings and get metadata
    if (string == NULL || search == NULL) return 0;
    uint32_t stringLen = StringLen(string);
    uint32_t searchLen = StringLen(search);
    char stringEncoding = string[3];
    char searchEncoding = search[3];
    if (stringEncoding != searchEncoding || stringLen == 0 || searchLen == 0) {
        return 0;
    }
    char* stringCstr = StringCstr(string);
    char* searchCstr = StringCstr(search);

    if (stringEncoding == ASCII || stringEncoding == UTF8)
    {
        // longest prefix-suffix
        uint16_t* lps = (uint16_t*)calloc(searchLen, sizeof(uint16_t));
        int j = 0;
        int i = 1;
        while (i < searchLen) {
            if (stringCstr[i] == searchCstr[j]) {
                j += 1; lps[i] = j; i += 1;
            }
            else {
                if (j != 0) { j = lps[j - 1]; }
                else {
                    lps[i] = 0; i += 1;
                }
            }
        }

        // knuth-morris-pratt
        i = 0;
        j = 0;
        while (i < stringLen) {
            if (stringCstr[i] == searchCstr[j]) {
                i++; j++;
            }
            if (j == searchLen) {
                free(lps);
                return 1;
            }
            else if (i < stringLen && stringCstr[i] != searchCstr[j]) {
                if (j != 0) { j = lps[j-1]; } 
                else { i++; }
            }
        }
        free(lps);
    }
    else if (stringEncoding == UTF32)
    {
        uint32_t stringCount = stringLen / 4;
        uint32_t searchCount = searchLen / 4;

        // longest prefix-suffix
        uint32_t* lps = (uint32_t*)calloc(searchCount, sizeof(uint32_t));
        int j = 0;
        int i = 1;
        while (i < searchCount) {
            if (GetUTF32Codepoint(search, i) == GetUTF32Codepoint(search, j)) {
                j += 1; lps[i] = j; i += 1;
            }
            else {
                if (j != 0) { j = lps[j - 1]; }
                else {
                    lps[i] = 0; i += 1;
                }
            }
        }

        // knuth-morris-pratt
        i = 0;
        j = 0;
        while (i < stringCount) {
            if (GetUTF32Codepoint(string, i) == GetUTF32Codepoint(search, j)) {
                i++; j++;
            }
            if (j == searchCount) {
                free(lps);
                return 1;
            }
            else if (i < stringCount && GetUTF32Codepoint(string, i) != GetUTF32Codepoint(search, j)) {
                if (j != 0) { j = lps[j-1]; } 
                else { i++; }
            }
        }
        free(lps);
    }
    return 0;
}

int StringEquals(String a, String b)
{
    if (!a || !b) return 0;
    uint32_t aLen = *(uint32_t*)a;
    return memcmp(a, b, aLen + 4) == 0;
}

void StringEncodeUTF8(String* string)
{
    // validate string and get metadata
    if (string == NULL || *string == NULL) return;
    uint32_t stringLen = StringLen(*string);
    char stringEncoding = (*string)[3];
    if (stringLen == 0 || stringEncoding == UTF8) return;

    if (stringEncoding == ASCII)
    {
        (*string)[3] = UTF8;
        return;
    }
    else if (stringEncoding == UTF32)
    {
        // precompute buffer size to be
        char* cstr = StringCstr(*string);
        uint32_t bufferSize = 0;
        for (uint32_t i=0; i<stringLen; i+=4)
        {
            uint32_t codepoint = *((uint32_t*)(cstr + i));
            if (codepoint < 0x80) bufferSize += 1;
            else if (codepoint < 0x800) bufferSize += 2;
            else if (codepoint < 0x10000) bufferSize += 3;
            else bufferSize += 4;
        }

        // allocate space for encoded result
        String encoded = malloc(bufferSize + 6);
        char* encodedCstr = StringCstr(encoded);
        if (encoded == NULL) return;

        // set metadata
        encoded[0] = (bufferSize >> 16) & 0xFF;
        encoded[1] = (bufferSize >> 8) & 0xFF;
        encoded[2] = bufferSize & 0xFF;  
        encoded[3] = UTF8;
        encodedCstr[bufferSize] = '\0';

        // encode
        uint32_t utf8Len = 0;
        for (uint32_t i=0; i<stringLen; i+=4)
        {
           uint32_t codepoint = *((uint32_t*)(cstr + i));
            
            if (codepoint < 0x80) { // 1 byte codepoint
                encodedCstr[utf8Len++] = (unsigned char)(codepoint);
            } 
            else if (codepoint < 0x800) { // 2 byte codepoint
                encodedCstr[utf8Len++] = (unsigned char)(0xC0 | (codepoint >> 6));
                encodedCstr[utf8Len++] = (unsigned char)(0x80 | (codepoint & 0x3F));
            }
            else if (codepoint < 0x10000) { // 3 byte codepoint
                encodedCstr[utf8Len++] = (unsigned char)(0xE0 | (codepoint >> 12));
                encodedCstr[utf8Len++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
                encodedCstr[utf8Len++] = (unsigned char)(0x80 | (codepoint & 0x3F));
            }
            else { // 4 byte codepoint
                encodedCstr[utf8Len++] = (unsigned char)(0xF0 | (codepoint >> 18));
                encodedCstr[utf8Len++] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3F));
                encodedCstr[utf8Len++] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3F));
                encodedCstr[utf8Len++] = (unsigned char)(0x80 | (codepoint & 0x3F));
            }
        }

        // update string
        free(*string);
        *string = encoded;
    }
}

void StringEncodeUTF32(String* string)
{
    // validate string and get metadata
    if (string == NULL) return;
    uint32_t stringLen = StringLen(*string);
    char stringEncoding = (*string)[3];
    if (stringLen == 0 || stringEncoding == UTF32) return;
    char* cstr = StringCstr(*string);

    if (stringEncoding == ASCII)
    {
        // allocate space for encoded result
        uint32_t encodedLen = stringLen * 4;
        String encoded = malloc(encodedLen + 4);
        char* encodedCstr = StringCstr(encoded);
        if (encoded == NULL) return;

        // set encoded metadata
        encoded[0] = (encodedLen >> 16) & 0xFF;
        encoded[1] = (encodedLen >> 8) & 0xFF;
        encoded[2] = encodedLen & 0xFF;  
        encoded[3] = UTF32;
        
        // encode
        for (uint32_t i=0; i<stringLen; i++)
        {
            uint32_t codepoint = (unsigned char)cstr[i];
            memcpy(encodedCstr + i * 4, &codepoint, 4);
        }

        // update string
        free(*string);
        *string = encoded;
        return;
    }

    if (stringEncoding == UTF8)
    {
        // over-allocate space for encoded result
        String encoded = malloc(stringLen * 4 + 4);
        if (encoded == NULL) return;

        // encode
        uint32_t utf32Len = 0;
        int i = 0;
        while (i < stringLen) {
            uint32_t codepoint = GetNextUTF8Codepoint(*string, &i);
            memcpy(encoded + 4 + utf32Len, &codepoint, 4);
            utf32Len += 4;
        }

        // shrink over-allocated buffer
        if (utf32Len > stringLen * 4) {
            encoded = realloc(encoded, utf32Len + 4);
        }

        // set metadata
        encoded[0] = (utf32Len >> 16) & 0xFF;
        encoded[1] = (utf32Len >> 8) & 0xFF;
        encoded[2] = utf32Len & 0xFF;  
        encoded[3] = UTF32;

        // update string
        free(*string);
        *string = encoded;
        return;
    }
}
