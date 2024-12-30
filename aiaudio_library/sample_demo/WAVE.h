/****************************************************************************
*
*    Copyright (c) 2022 amlogic Corporation
*
*    Permission is hereby granted, free of charge, to any person obtaining a
*    copy of this software and associated documentation files (the "Software"),
*    to deal in the Software without restriction, including without limitation
*    the rights to use, copy, modify, merge, publish, distribute, sublicense,
*    and/or sell copies of the Software, and to permit persons to whom the
*    Software is furnished to do so, subject to the following conditions:
*
*    The above copyright notice and this permission notice shall be included in
*    all copies or substantial portions of the Software.
*
*    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
*    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
*    DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/
#pragma once
#ifndef _WAVE_H_
#define _WAVE_H_

#ifdef __cplusplus
extern "C" {
#endif // _cplusplus

#ifdef _MSC_VER
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int16 uint16_t;
#else
#include <stdint.h>
#endif


#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include "hmath.h"

/*RIFF chunk*/
typedef struct {
    char ckID[4];
    uint32_t cksize;
    char WAVEID[4];
}RIFF_t;

/*fmt chunk*/
typedef struct {
    char ckID[4];
    uint32_t cksize;
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
}fmt_t;

typedef struct {
    char ckID[4];
    uint32_t cksize;
    int** data;
}DATA_t;

typedef struct {
    int numChannels;
    int numSamples;
    int sampleRate;
    int sampleLengthInByte;
    int containerLengthInByte;
}WAVEParams_t;

typedef struct {
    RIFF_t RIFF;
    fmt_t fmt;
    DATA_t DATA;
    WAVEParams_t WAVEParams;
    int myEndian;
}WAVE_t;

WAVE_t initWAVE_t();

long int readWAVE(FILE * f, size_t sizeInByte, int EndianFlag);

void loadWAVEFile(WAVE_t* w, FILE* f);

int WAVEParamsCheck(WAVEParams_t w1, WAVEParams_t w2);

void print_WAVE(WAVE_t w);

void free_WAVE(WAVE_t* w);

void writeWaveFile(FILE* f, WAVEParams_t params, IntMat m);

#ifdef __cplusplus
}
#endif // _cplusplus

#endif // !_WAVE_H_
