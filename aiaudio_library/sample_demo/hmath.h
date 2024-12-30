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
#ifndef _HMATH_H_
#define _HMATH_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include<stdio.h>
#include<math.h>
typedef double logdouble;
typedef void* Ptr;
typedef double* Vector;
typedef double** Matrix;
typedef int* IntVec;
typedef int* ShortVec;
typedef int** IntMat;
//#define pi 3.1415926
#define pi 3.1415926535897932384626433832795028841971

typedef Vector SVector;
typedef Matrix STriMat;
typedef Matrix SMatrix;

typedef struct {
    Vector real;
    Vector imag;
    int N;
}XFFT;

Vector CreateVector(int n);
int VectorSize(Vector v);
void ZeroVector(Vector v);
void ShowVector(Vector v);
void ShowVectorE(Vector v);
void FreeVector(Vector v);
double FindMax(Vector v);
int FindMaxIndex(Vector v);
void CopyVector(Vector v1, Vector v2);
void CopyVector2(Vector v1, IntVec ind1, Vector v2, IntVec ind2);
void WriteVectorE(FILE* f, Vector v);
void WriteVector(FILE* f, Vector v);
void LoadVector(FILE * f, Vector v);
void LoadVectorE(FILE * f, Vector v);


IntVec CreateIntVec(int n);
void FreeIntVec(IntVec v);
void ShowIntVec(IntVec v);
void WriteIntVec(FILE* f, IntVec v);
void ZeroIntVec(IntVec v);
void CopyIntVec(IntVec v1, IntVec v2);

SVector CreateSVector(int n);
void FreeSVector(SVector v);


void InitXFFT(XFFT* xfftP, int N);
void ShowXFFT(XFFT xf);
void ShowXFFTE(XFFT xf);
void FreeXFFT(XFFT* xfftP);
int XFFTSize(XFFT x);
void XFFTToVector(XFFT xf, Vector* vp, int power2Flag);
void VectorToXFFT(XFFT* xfp, Vector v);


IntMat CreateIntMat(int nrows, int ncols);
void FreeIntMat(IntMat m);
void ZeroIntMat(IntMat m);
void WriteIntMat(FILE* f, IntMat m);
void ShowIntMat(IntMat m);

Matrix CreateMatrix(int nrows, int ncols);
int NumRows(Matrix m);
int NumCols(Matrix m);
void ShowMatrix(Matrix m);
void FreeMatrix(Matrix m);
void ZeroMatrix(Matrix m);
void CopyMatrix(Matrix m1, Matrix m2);
void CopyMatToTri(Matrix m1, STriMat m2);
void WriteMatrix(FILE* f, Matrix m);
void LoadMatrix(FILE* f, Matrix m);


SMatrix CreateSMatrix(int nrows, int ncols);
void FreeSMatrix(SMatrix m);

Matrix CreateSTriMat(int size);
int STriMatSize(STriMat m);
void ShowSTriMat(STriMat m);
void FreeSTriMat(STriMat m);
void ZeroSTriMat(STriMat m);
void CopySTriMat(STriMat m1, STriMat m2);
void WriteSTriMat(FILE* f, STriMat m);
void LoadStriMat(FILE*f, STriMat m);

Ptr GetHook(Ptr m);
void SetHook(Ptr m, Ptr ptr);
void SetUse(Ptr m, int n);

int Choleski(STriMat A, Matrix L);
void MSolve(Matrix L, int i, Vector x, Vector y);
logdouble CovInvert(STriMat c, STriMat invc);
logdouble CovDet(STriMat c);

int mod(int a, int b);
void reshape(Matrix* mp, Vector v, int r, int c,int dim);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
