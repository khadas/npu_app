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
#include "mfcc.h"
#include "hmath.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

double Mel(int k , double fres)
{
    return 1127 * log(1 + ( k - 1 ) * fres);
}

double WarpFreq(double fcl, double fcu, double freq, double minFreq, double maxFreq, double alpha)
{
    if (alpha == 1.0)
        return freq;
    else {
        double scale = 1.0 / alpha;
        double cu = fcu * 2 / (1 + scale);
        double cl = fcl * 2 / (1 + scale);

        double au = (maxFreq - cu * scale) / (maxFreq - cu);
        double al = (cl * scale - minFreq) / (cl - minFreq);

        if (freq > cu)
            return  au * (freq - cu) + scale * cu;
        else if (freq < cl)
            return al * (freq - minFreq) + minFreq;
        else
            return scale * freq;
    }
}

FBankInfo InitFBank(int frameSize, double sampPeriod, int numChans,
    double lopass, double hipass, int usePower, int takeLogs,
    int doubleFFT,
    double alpha, double warpLowCut, double warpUpCut)
{
    FBankInfo fb;
    double mlo, mhi, ms, melk;
    int k, chan, maxChan, Nby2;

    fb.frameSize = frameSize; fb.numChans = numChans;
    fb.sampPeriod = sampPeriod;
    fb.usePower = usePower; fb.takeLogs = takeLogs;

    fb.fftN = 1024;
    while (frameSize>fb.fftN) fb.fftN *= 2;

    Nby2 = fb.fftN / 2;
    fb.fres = 1.0E7 / (sampPeriod * fb.fftN * 700.0);
    maxChan = numChans + 1;

    fb.klo = 2; fb.khi = Nby2;
    mlo = 0; mhi = Mel(Nby2 + 1, fb.fres);
    if (lopass >= 0.0) {
        mlo = 1127 * log(1 + lopass / 700.0);
        fb.klo = (int)((lopass * sampPeriod * 1.0e-7 * fb.fftN) + 2.5);
        if (fb.klo<2) fb.klo = 2;
    }
    if (hipass >= 0.0) {
        mhi = 1127 * log(1 + hipass / 700.0);

        fb.khi = (int)((hipass * sampPeriod * 1.0e-7 * fb.fftN) + 0.5);
        if (fb.khi>Nby2) fb.khi = Nby2;
    }

    fb.cf = CreateVector(maxChan);
    ms = mhi - mlo;
    for (chan = 1; chan <= maxChan; chan++) {
        if (alpha == 1.0) {
            fb.cf[chan] = ((double)chan / (double)maxChan)*ms + mlo;
        }
        else {

            double minFreq = 700.0 * (exp(mlo / 1127.0) - 1.0);
            double maxFreq = 700.0 * (exp(mhi / 1127.0) - 1.0);
            double cf = ((double)chan / (double)maxChan) * ms + mlo;

            cf = 700 * (exp(cf / 1127.0) - 1.0);

            fb.cf[chan] = 1127.0 * log(1.0 + WarpFreq(warpLowCut, warpUpCut, cf, minFreq, maxFreq, alpha) / 700.0);
        }
    }


    fb.loChan = CreateIntVec(Nby2);
    for (k = 1, chan = 1; k <= Nby2; k++) {
        melk = Mel(k, fb.fres);
        if (k<fb.klo || k>fb.khi) fb.loChan[k] = -1;
        else {
            while (fb.cf[chan] < melk  && chan <= maxChan) ++chan;
            fb.loChan[k] = chan - 1;
        }
    }

    fb.loWt = CreateVector(Nby2);
    for (k = 1; k <= Nby2; k++) {
        chan = fb.loChan[k];
        if (k<fb.klo || k>fb.khi) fb.loWt[k] = 0.0;
        else {
            if (chan>0)
                fb.loWt[k] = ((fb.cf[chan + 1] - Mel(k, fb.fres)) /
                (fb.cf[chan + 1] - fb.cf[chan]));
            else
                fb.loWt[k] = (fb.cf[1] - Mel(k, fb.fres)) / (fb.cf[1] - mlo);
        }
    }

    fb.x = CreateVector(fb.fftN);
    return fb;
}

void Wave2FBank(Vector s, Vector fbank, double *te,double* te2, FBankInfo info)
{
    const double melfloor = 1.0;
    int k, bin;
    double t1, t2;
    double ek;

    if (info.frameSize != VectorSize(s))
        printf("Wave2FBank: frame size mismatch");
    if (info.numChans != VectorSize(fbank))
        printf("Wave2FBank: num channels mismatch");
    if (te != NULL) {
        *te = 0.0;
        for (k = 1; k <= info.frameSize; k++)
            *te += (s[k] * s[k]);
    }
    for (k = 1; k <= info.frameSize; k++)
        info.x[k] = s[k];
    for (k = info.frameSize + 1; k <= info.fftN; k++)
        info.x[k] = 0.0;
    Realft(info.x);
    ZeroVector(fbank);
    for (k = info.klo; k <= info.khi; k++) {
        t1 = info.x[2 * k - 1]; t2 = info.x[2 * k];
        if (info.usePower)
            ek = t1*t1 + t2*t2;
        else
            ek = sqrt(t1*t1 + t2*t2);

        bin = info.loChan[k];
        t1 = info.loWt[k] * ek;
        if (bin>0) fbank[bin] += t1;
        if (bin<info.numChans) fbank[bin + 1] += ek - t1;
    }
    *te2 = 0.0;
    for (k = 1; k <= info.fftN/2; k++) {
        *te2 += info.x[2 * k - 1] * info.x[2 * k - 1] + info.x[2 * k] * info.x[2 * k];
    }

    if (info.takeLogs) {
        for (bin = 1; bin <= info.numChans; bin++) {
            t1 = fbank[bin];
            if (t1<melfloor) t1 = melfloor;
            fbank[bin] = log(t1);
        }
    }

}

void FBank2MFCC(Vector fbank, Vector c, int n)
{
    int j, k, numChan;
    double mfnorm, pi_factor, x;

    numChan = VectorSize(fbank);
    mfnorm = sqrt(2.0 / (double)numChan);
    pi_factor = pi / (double)numChan;
    for (j = 1; j <= n; j++) {
        c[j] = 0.0;
        x = (double)j * pi_factor;
        for (k = 1; k <= numChan; k++)
            c[j] += fbank[k] * cos(x*(k - 0.5));
        c[j] *= mfnorm;
        c[j] *= 1 +11*sin(pi*(j-1)/22);
    }
}


double FBank2C0(Vector fbank)
{
    int k, numChan;
    double mfnorm, sum;

    numChan = VectorSize(fbank);
    mfnorm = sqrt(2.0 / (double)numChan);
    sum = 0.0;
    for (k = 1; k <= numChan; k++)
        sum += fbank[k];
    return sum * mfnorm;
}

void FFT(Vector s, int invert)
{
    int ii, jj, n, nn, limit, m, j, inc, i;
    double wx, wr, wpr, wpi, wi, theta;
    double xre, xri, x;

    n = VectorSize(s);
    nn = n / 2 + 1;
    j = 1;
    for (ii = 1; ii <= nn; ii++) {
        i = 2 * ii - 1;
        if (j>i) {
            xre = s[j];
            xri = s[j + 1];
            s[j] = s[i];
            s[j + 1] = s[i + 1];
            s[i] = xre;
            s[i + 1] = xri;
        }
        m = n / 2;
        while (m >= 2 && j > m) {
            j -= m;
            m /= 2;
        }
        j += m;
    };
    limit = 2;
    while (limit < n) {
        inc = 2 * limit;
        theta = 2*pi / limit;
        if (invert) theta = -theta;
        x = sin(0.5 * theta);
        wpr = -2.0 * x * x;
        wpi = sin(theta);
        wr = 1.0;
        wi = 0.0;
        for (ii = 1; ii <= limit / 2 ; ii++) {
            m = 2 * ii - 1;
            for (jj = 0; jj <= (n - m) / inc; jj++) {
                i = m + jj * inc;
                j = i + limit;
                xre = wr * s[j] - wi * s[j + 1];
                xri = wr * s[j + 1] + wi * s[j];
                s[j] = s[i] - xre;
                s[j + 1] = s[i + 1] - xri;
                s[i] = s[i] + xre;
                s[i + 1] = s[i + 1] + xri;
            }
            wx = wr;
            wr = wr * wpr - wi * wpi + wr;
            wi = wi * wpr + wx * wpi + wi;
        }
        limit = inc;
    }
    if (invert) {
        for (i = 1; i <= n; i++)
            s[i] = s[i] / nn;
    }

}

void Realft(Vector s)
{
    int n, n2, i, i1, i2, i3, i4;
    double xr1, xi1, xr2, xi2, wrs, wis;
    double yr, yi, yr2, yi2, yr0, theta, x;

    n = VectorSize(s) / 2;
    n2 = n / 2 ;
    theta = pi / (n);
    FFT(s, 0);
    x = sin(0.5 * theta);
    yr2 = -2.0 * x * x;
    yi2 = sin(theta);
    yr = 1.0 + yr2;
    yi = yi2;
    for (i = 2; i <= n2; i++) {
        i1 = i + i - 1;
        i2 = i1 + 1;
        i3 = n + n + 3 - i2;
        i4 = i3 + 1;
        wrs = yr;
        wis = yi;
        xr1 = (s[i1] + s[i3]) / 2.0;
        xi1 = (s[i2] - s[i4]) / 2.0;
        xr2 = (s[i2] + s[i4]) / 2.0;
        xi2 = (s[i3] - s[i1]) / 2.0;
        s[i1] = xr1 + wrs * xr2 - wis * xi2;
        s[i2] = xi1 + wrs * xi2 + wis * xr2;
        s[i3] = xr1 - wrs * xr2 + wis * xi2;
        s[i4] = -xi1 + wrs * xi2 + wis * xr2;
        yr0 = yr;
        yr = yr * yr2 - yi  * yi2 + yr;
        yi = yi * yr2 + yr0 * yi2 + yi;
    }
    xr1 = s[1];
    s[1] = xr1 + s[2];
    s[2] = 0.0;
    s[1025] = ( (s[512] + s[1024]) / 2.0) - wrs * ((s[512] + s[1024])) - wis * ( (s[1024] - s[512]) / 2.0);
}