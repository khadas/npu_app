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
#define _CRT_SECURE_NO_WARNINGS
#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include "hmath.h"
#include "mfcc.h"
#include"sigProcess.h"
#include"WAVE.h"


float* get_wav_decode(char* path)
{
    double sampleRate = 16000;
    double samplePeriod;
    double hipassfre = 4000;
    double lowpassfre = 20;
    int wlen =640;
    int inc = 320 ;
    int bankNum = 40;
    int MFCCNum = 40;
    int delwin = 9;
    int energyFlag =0;
    int zeroCrossingFlag = 0;
    int brightFlag = 0;
    int subBandEFlag = 0;
    int regreOrder = 2;
    int MFCC0thFlag = 0;
    int fftLength = 0;

    int sampleNum;
    int channels =0;
    double preemphasise = 0.97;
    int vecNum = 1;
    int fbankFla = 0;
    FILE *f ;

    Vector d2=NULL , d3=NULL ,fbank=NULL,subBankEnergy=NULL,hamWin=NULL;
    Vector* d1=NULL;
    FBankInfo info;
    Vector dpostProc;
    int otherFeatureNum;
    int curr_pos ;
    int vSize,rowNum,step;
    double zc,te,te2,brightness;
    int i,j,i0,j0,k;
    static float mfcc[490];

    samplePeriod = (double)1e7 / (double)sampleRate;

    otherFeatureNum = MFCC0thFlag + energyFlag + zeroCrossingFlag + brightFlag + subBandEFlag + fftLength;//计算其他参量的个数

    subBankEnergy = CreateVector(subBandEFlag);
    d2 = CreateVector(wlen);
    fbank = CreateVector(bankNum);
    d3 = CreateVector(MFCCNum);

    info = InitFBank(wlen, samplePeriod, bankNum, lowpassfre, hipassfre, 0, 1, 0, 1.0, 0, 0);
    hamWin=GenHamWindow(wlen);
    curr_pos = 0;
    f = fopen(path, "rb");

    WAVE_t wavfile = initWAVE_t();
    loadWAVEFile(&wavfile, f);
    print_WAVE(wavfile);
    sampleNum=wavfile.WAVEParams.numSamples;
    channels = wavfile.WAVEParams.numChannels;


    d1 = CreateMatrix(wavfile.WAVEParams.numChannels, wavfile.WAVEParams.numSamples);
    for (i0 = 1; i0 <= wavfile.WAVEParams.numChannels; i0++) {
        for (i = 1; i <= wavfile.WAVEParams.numSamples; i++) {
            d1[i0][i] = (double)wavfile.DATA.data[i0][i];
        }
    }

    if (channels == 2) for (i = 1; i <= wavfile.WAVEParams.numSamples; i++) {
        d1[3][i] = 0.5*(d1[1][i] + d1[2][i]);
        d1[4][i] = d1[1][i] - d1[2][i];
    }
    for (i0 = 1; i0 <= NumRows(d1); i0++) {
        PreEmphasise(d1[i0],preemphasise);
    }

    dpostProc = CreateVector(((MFCCNum + otherFeatureNum) * vecNum *(int)((sampleNum - (wlen - inc)) / inc)));

    for (j = 0; j <= (sampleNum - wlen); j += inc) {
        for (i0 = 1; i0 <= vecNum; i0++) {
            for (i = 1; i <= wlen; i++) {
                d2[i] = (double)wavfile.DATA.data[i0][i+j];
            };

            Ham(d2,hamWin,wlen);
            Wave2FBank(d2, fbank, &te, &te2, info);
            FBank2MFCC(fbank, d3, MFCCNum);
            for (j0 = 1; j0 <= MFCCNum / 4; j0++, curr_pos++) {
                dpostProc[curr_pos] = d3[j0];
            }
        }

    }

    fclose(f);
    free_WAVE(&wavfile);
    FreeVector(dpostProc);
    FreeMatrix(d1);

    FreeVector(d2);
    FreeVector(fbank);
    FreeVector(d3);
    FreeVector(hamWin);

    return (float*)mfcc;
}
