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
#define _CRT_SECURE_NO_WARNINGS
#ifndef _SIGPROCESS_H_
#define _SIGPROCESS_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include<math.h>
#include<stdio.h>
#include<stdlib.h>
#include"hmath.h"


void circshift(Vector v, int shift);
int find(Vector v, double thre, int FrontOrEndFlag);
void pad_signal(Vector* yP, Vector x, int Npad);
void unpad_signal(Vector* yP, Vector x, int res, int target_sz );
Matrix frameRawSignal(IntVec v, int wlen, int inc,double preEmphasiseCoefft,int enableHamWindow);

void ZeroMean(IntVec data);
void PreEmphasise(Vector s, double k);

double calBrightness(Vector fftx);

void calSubBankE(Vector fftx, Vector subBankEnergy);

double zeroCrossingRate(Vector s, int frameSize);

void Regress(double* data, int vSize, int n, int step, int offset, int delwin, int head, int tail, int simpleDiffs);

void RegressMat(Matrix* m,int delwin, int regressOrder);

void NormaliseLogEnergy(double *data, int n, int step, double silFloor, double escale);

void ZNormalize(double *data, int vSize, int n, int step);

Vector GenHamWindow(int frameSize);

void Ham(Vector s, Vector hamWin, int hamWinSize);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !_SIGPROCESS_H_
