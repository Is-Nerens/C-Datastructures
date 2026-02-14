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
// [header: 4 bytes][data][\n]

#pragma once
#include <stdint.h>
#include <string.h>


typedef unsigned char* String;

String StringCreate(const char* str)
{
    uint32_t length = (uint32_t)strlen(str);
    String result = malloc(4 + length + 1);
    if (!result) return NULL;
    memcpy(result, &length, sizeof(uint32_t));
    memcpy(result + 4, str, length);
    result[4 + length] = '\0';
    return result;
}

String StringCreateBuffer(uint32_t bytes)
{
    String result = malloc(bytes + 4 + 1);
    if (!result) return NULL;
    memcpy(result, &bytes, sizeof(uint32_t));
    result[bytes + 4] = '\0';
    return result;
}

inline void StringFree(String str)
{
    free(str);
}

inline char* StringCstr(String str)
{
    return (char*)str + 4;
}

inline uint32_t StringLen(String string)
{
    return *(uint32_t*)string;
}

uint32_t NextUTF8Codepoint(String string, int* i)
{
    char* cStr = StringCstr(string);
    unsigned char* p = (unsigned char*)cStr + *i;
    uint32_t cp;

    if (*p == 0) return 0;  // end of string

    if ((*p & 0x80) == 0) {
        cp = *p++;
    }
    else if ((*p & 0xE0) == 0xC0 &&
            (p[1] & 0xC0) == 0x80) {
        cp = (*p & 0x1F) << 6 |
            (p[1] & 0x3F);
        p += 2;
    }
    else if ((*p & 0xF0) == 0xE0 &&
            (p[1] & 0xC0) == 0x80 &&
            (p[2] & 0xC0) == 0x80) {
        cp = (*p & 0x0F) << 12 |
            (p[1] & 0x3F) << 6 |
            (p[2] & 0x3F);
        p += 3;
    }
    else if ((*p & 0xF8) == 0xF0 &&
            (p[1] & 0xC0) == 0x80 &&
            (p[2] & 0xC0) == 0x80 &&
            (p[3] & 0xC0) == 0x80) {
        cp = (*p & 0x07) << 18  |
            (p[1] & 0x3F) << 12 |
            (p[2] & 0x3F) << 6  |
            (p[3] & 0x3F);
        p += 4;
    }
    else { // invalid byte
        cp = 0xFFFD;
        p += 1;
    }
    *i = (int)(p - (unsigned char*)cStr);
    return cp;
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

    // allocate space for return string
    String output = NULL;
    output = malloc(aLen + bLen + 4 + 1);
    if (output == NULL) return NULL;

    // copy data from string a and b
    memcpy(output + 4, a + 4, aLen);
    memcpy(output + 4 + aLen, b + 4, bLen);

    // set metadata
    uint32_t outputLen = aLen + bLen;
    memcpy(output, &outputLen, sizeof(uint32_t));
    output[outputLen + 4] = '\0';
    return output;
}

String StringConcatCstr(String a, char* b)
{
    if (a == NULL || b == NULL) return NULL;
    uint32_t aLen = StringLen(a);
    uint32_t bLen = strlen(b);

    // allocate space for return string
    String output = NULL;
    output = malloc(aLen + bLen + 4 + 1);
    if (output == NULL) return NULL;

    // copy data from string a and b
    memcpy(output + 4, a + 4, aLen);
    memcpy(output + 4 + aLen, b, bLen);

    // set metadata
    uint32_t outputLen = aLen + bLen;
    memcpy(output, &outputLen, sizeof(uint32_t));
    output[outputLen + 4] = '\0';
    return output;
}

String CstrConcatString(char* a, String b)
{
    if (a == NULL || b == NULL) return NULL;
    uint32_t aLen = strlen(a);
    uint32_t bLen = StringLen(b);
    
    // allocate space for return string
    String output = NULL;
    output = malloc(aLen + bLen + 4 + 1);
    if (output == NULL) return NULL;

    // copy data from string a and b
    memcpy(output + 4, a, aLen);
    memcpy(output + 4 + aLen, b + 4, bLen);

    // set metadata
    uint32_t outputLen = aLen + bLen;
    memcpy(output, &outputLen, sizeof(uint32_t));
    output[outputLen + 4] = '\0';
    return output;
}

inline String StringCopy(String string)
{
    uint32_t bytes = StringLen(string) + 4 + 1;
    String copy = malloc(bytes);
    memcpy(copy, string, bytes);
    return copy;
}

int StringContains(String string, String search)
{
    // validate strings and get metadata
    if (string == NULL || search == NULL) return -1;
    uint32_t stringLen = StringLen(string);
    uint32_t searchLen = StringLen(search);
    if (stringLen == 0 || searchLen == 0) {
        return -1;
    }
    char* stringCstr = StringCstr(string);
    char* searchCstr = StringCstr(search);

    // longest prefix-suffix
    uint16_t* lps = (uint16_t*)calloc(searchLen, sizeof(uint16_t));
    int j = 0;
    int i = 1;
    while (i < searchLen) {
        if (searchCstr[i] == searchCstr[j]) {
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
            return i - j;
        }
        else if (i < stringLen && stringCstr[i] != searchCstr[j]) {
            if (j != 0) { j = lps[j-1]; } 
            else { i++; }
        }
    }
    free(lps);
    return -1;
}

String StringReplaceFirst(String string, String target, String replacement)
{
    if (replacement == NULL) return NULL;
    int match = StringContains(string, target); // checks if string and target is NULL
    if (match == -1) return NULL;

    // construct result string
    uint32_t length = StringLen(string) - StringLen(target) + StringLen(replacement);
    String result = malloc(length + 4 + 1);
    if (!result) return NULL;
    memcpy(StringCstr(result), StringCstr(string), match);
    memcpy(StringCstr(result) + match, StringCstr(replacement), StringLen(replacement));
    memcpy(StringCstr(result) + match + StringLen(replacement), StringCstr(string) + match + StringLen(target), StringLen(string) - match - StringLen(target));
    memcpy(result, &length, sizeof(uint32_t));
    result[length + 4] = '\0';
    return result;
}

String StringRemoveSuffix(String string, String suffix)
{
    // critical errors
    if (string == NULL || 
        suffix == NULL || 
        StringLen(suffix) == 0 || 
        StringLen(suffix) > StringLen(string)) { 
        return NULL;
    }

    // check if string ends with suffix
    if (memcmp(StringCstr(string) + StringLen(string) - StringLen(suffix), StringCstr(suffix), StringLen(suffix)) != 0) {
        return StringCopy(string);
    }

    // construct new string without suffix
    uint32_t newLength = StringLen(string) - StringLen(suffix);
    String result = malloc(newLength + 4 + 1);
    if (!result) return NULL;
    memcpy(StringCstr(result), StringCstr(string), newLength);

    // set header
    memcpy(result, &newLength, sizeof(uint32_t));
    result[newLength + 4] = '\0';
    return result;
}

int StringEquals(String a, String b)
{
    if (!a || !b) return 0;
    uint32_t aLen = *(uint32_t*)a;
    return memcmp(a, b, aLen + 4) == 0;
}

String StringUpdate(String str, const char* new)
{
    char encoding = str[3];
    StringFree(str);
    return StringCreate(new);
}
