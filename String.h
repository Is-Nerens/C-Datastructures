#pragma once
#include <stdint.h>
#include <string.h>

typedef char* String;
#define ASCII  0x01  // 00000001
#define UTF8   0x02  // 00000010
#define UTF32  0x03  // 00000011

inline char* StringCstr(String str)
{
    return str + 5;
}

String StringCreate(char* str)
{
    uint32_t length = strlen(str);
    String result = malloc(length + 6);
    if (!result) return NULL;
    memcpy(result + 5, str, length);
    memcpy(result, &length, 4);
    result[4] = ASCII;
    result[length + 5] = '\0';
    return result;
}

String StringCreateUTF8(char* str)
{
    uint32_t length = strlen(str);
    String result = malloc(length + 6);
    if (!result) return NULL;
    memcpy(result + 5, str, length);
    memcpy(result, &length, 4);
    result[4] = UTF8;
    result[length + 5] = '\0';
    return result;
}

String StringCreateUTF32(char* str)
{
    uint32_t length = strlen(str);
    String result = malloc(length + 6);
    if (!result) return NULL;
    memcpy(result + 5, str, length);
    memcpy(result, &length, 4);
    result[4] = UTF32;
    result[length + 5] = '\0';
    return result;
}

inline void StringFree(String* str)
{
    if (str && *str) {
        free(*str);
        *str = NULL;
    }
}

uint32_t GetNextUTF8Codepoint(String string, int* i)
{
    char* cstr = StringCstr(string);
    uint32_t stringLen = *(uint32_t*)string;
    uint32_t codepoint = 0;
    unsigned char firstByte = (unsigned char)cstr[*i];

    if ((firstByte & 0x80) == 0) {
        codepoint = firstByte;
        *i += 1;
    }
    else if (*i + 1 < stringLen && (firstByte & 0xE0) == 0xC0) {
        codepoint = (firstByte & 0x1F) << 6;
        codepoint |= (unsigned char)(cstr[*i + 1] & 0x3F);
        *i += 2;
    }
    else if (*i + 2 < stringLen && (firstByte & 0xF0) == 0xE0) {
        codepoint = (firstByte & 0x0F) << 12;
        codepoint |= (unsigned char)(cstr[*i + 1] & 0x3F) << 6;
        codepoint |= (unsigned char)(cstr[*i + 2] & 0x3F);
        *i += 3;
    }
    else if (*i + 3 < stringLen && (firstByte & 0xF8) == 0xF0) {
        codepoint = (firstByte & 0x07) << 18;
        codepoint |= (unsigned char)(cstr[*i + 1] & 0x3F) << 12;
        codepoint |= (unsigned char)(cstr[*i + 2] & 0x3F) << 6;
        codepoint |= (unsigned char)(cstr[*i + 3] & 0x3F);
        *i += 4;
    }
    return codepoint;
} 

String StringConcat(String a, String b)
{
    // validate strings and get metadata
    if (a == NULL || b == NULL) return NULL;
    uint32_t aLen = *(uint32_t*)a;
    uint32_t bLen = *(uint32_t*)b;
    char aEncoding = a[4];
    char bEncoding = b[4];
    if (aEncoding != bEncoding || aLen == 0 || bLen == 0) return NULL;

    // allocate space for return string
    String output = NULL;
    output = malloc(aLen + bLen + 6);
    if (output == NULL) return NULL;

    // copy data from string a and b
    memcpy(output + 5, a + 5, aLen);
    memcpy(output + 5 + aLen, b + 5, bLen);

    // set metadata
    uint32_t outputLen = aLen + bLen;
    memcpy(output, &outputLen, 4);
    output[4] = aEncoding;
    output[outputLen + 5] = '\0';
    return output;
}

int StringContains(String string, String search)
{
    // validate strings and get metadata
    if (string == NULL || search == NULL) return 0;
    uint32_t stringLen = *(uint32_t*)string;
    uint32_t searchLen = *(uint32_t*)search;
    char stringEncoding = string[4];
    char searchEncoding = search[4];
    if (stringEncoding != searchEncoding || stringLen == 0 || searchLen == 0) {
        return 0;
    }

    if (stringEncoding == ASCII)
    {
        // longest prefix-suffix
        uint8_t* lps = (uint8_t*)calloc(searchLen, 1);
        int j = 0;
        int i = 1;
        while (i < searchLen) {
            if (search[i + 5] == search[j + 5]) {
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
            if (string[i + 5] == search[j + 5]) {
                i++; j++;
            }
            if (j == searchLen) {
                free(lps);
                return 1;
            }
            else if (i < stringLen && string[i + 5] != search[j + 5]) {
                if (j != 0) { j = lps[j-1]; } 
                else { i++; }
            }
        }
        free(lps);
    }
    else if (stringEncoding == UTF8)
    {
        // longest prefix-suffix
        uint8_t* lps = (uint8_t*)calloc(searchLen, 1);
        int j = 0;
        int i = 1;
        while (i < searchLen) {
            if (search[i + 5] == search[j + 5]) {
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
            if (string[i + 5] == search[j + 5]) {
                i++; j++;
            }
            if (j == searchLen) {
                free(lps);
                return 1;
            }
            else if (i < stringLen && string[i + 5] != search[j + 5]) {
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
    return memcmp(a, b, aLen + 5) == 0;
}

void StringEncodeUTF8(String* string)
{
    // validate string and get metadata
    if (string == NULL || *string == NULL) return;
    uint32_t stringLen = *(uint32_t*)*string;
    char stringEncoding = (*string)[4];
    if (stringLen == 0 || stringEncoding == UTF8) return;

    if (stringEncoding == ASCII)
    {
        (*string)[4] = UTF8;
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
        memcpy(encoded, &bufferSize, 4);
        encoded[4] = UTF8;
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
    uint32_t stringLen = *(uint32_t*)*string;
    char stringEncoding = (*string)[4];
    if (stringLen == 0 || stringEncoding == UTF32) return;
    char* cstr = StringCstr(*string);

    if (stringEncoding == ASCII)
    {
        // allocate space for encoded result
        uint32_t encodedLen = stringLen * 4;
        String encoded = malloc(encodedLen + 6);
        char* encodedCstr = StringCstr(encoded);
        if (encoded == NULL) return;

        // set encoded metadata
        memcpy(encoded, &encodedLen, 4);
        encoded[4] = UTF32;
        encodedCstr[stringLen * 4] = '\0';
        
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
        String encoded = malloc(stringLen * 4 + 6);
        if (encoded == NULL) return;

        // encode
        uint32_t utf32Len = 0;
        int i = 0;
        while (i < stringLen) {
            uint32_t codepoint = GetNextUTF8Codepoint(*string, &i);
            memcpy(encoded + 5 + utf32Len, &codepoint, 4);
            utf32Len += 4;
        }

        // shrink over-allocated buffer
        if (utf32Len > stringLen * 4) {
            encoded = realloc(encoded, utf32Len + 6);
        }

        // set metadata
        memcpy(encoded, &utf32Len, 4);
        encoded[4] = UTF32;
        encoded[utf32Len + 5] = '\0';

        // update string
        free(*string);
        *string = encoded;
        return;
    }
}
