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
/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include <pthread.h>
#include "nn_demo.h"
#include "mfcc.h"
#include "sigProcess.h"
#include "WAVE.h"

static void *context = NULL;
char * jpath = NULL;
static unsigned char *rawdata = NULL;
//////////////////for input/output dma////////////////////
static unsigned char *outbuf = NULL;
static unsigned char *inbuf = NULL;
static int use_dma = 0;
static amlnn_input_mode_t inmode;
static int input_width = 0, input_high = 0;
nn_input inData;

void Wave2Fbank(Vector s, Vector fbank, double *te,double* te2, FBankInfo info)
{
    const double melfloor = 1.0;
    int k, bin;
    double t1, t2;   /* real and imag parts */
    double ek;      /* energy of k'th fft channel */
                   /* Check that info record is compatible */
    if (info.frameSize != VectorSize(s))
        printf("Wave2FBank: frame size mismatch");
    if (info.numChans != VectorSize(fbank))
        printf("Wave2FBank: num channels mismatch");
    /* Compute frame energy if needed */
    if (te != NULL) {
        *te = 0.0;
        for (k = 1; k <= info.frameSize; k++) {
            *te += (s[k] * s[k]);
        }
    }
    ZeroVector(fbank);
    for (k = 1; k <= info.frameSize; k++)
        info.x[k] = s[k];    /* copy to workspace */
    for (k = info.frameSize + 1; k <= info.fftN + 2; k++)
        info.x[k] = 0.0;   /* pad with zeroes */
    Realft(info.x);                            /* take fft */
    for (k = 1; k <= (info.fftN + 2)/2; k++) {
        fbank[k] =  0.0009765625*sqrt(info.x[2*k-1]*info.x[2*k-1]+info.x[2*k]*info.x[2*k]) *sqrt(info.x[2*k-1] * info.x[2 * k - 1] + info.x[2 * k] * info.x[2 * k]);
    }
}

float* get_wav_decode_test(char* path)
{
    double sampleRate = 16000;
    double samplePeriod;
    double hipassfre = 4000;
    double lowpassfre = 20;
    int wlen= 640;
    int inc = 320 ;
    int bankNum = 40;
    int MFCCNum = 10;
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

    Vector d2=NULL , d3=NULL ,subBankEnergy=NULL,hamWin=NULL,fbank=NULL,fbank_i=NULL;
    Vector* d1=NULL;
    FBankInfo info;
    Vector dpostProc;

    int otherFeatureNum;
    int vSize,rowNum,step;
    double zc,te,te2,brightness;
    int i,j,i0,j0;
    int k,m,n;
    int nfilt = 40; // filterbank_channel_count
    int power_spe_l = 49;
    int power_spe_w = 513;
    int num_ceps = 10;                       // # dct_channel_count
    int numChan = nfilt;
    int cep_lifter = 22;

    static int bin[42] = {1,3,5,8,10,13,15,18,21,24,28,31,35,38,42,46,51,55,60,65,70,75,81,87,93,99,106,113, 121,129,137,145,154,163,173,183,194,205,217,229,242,256};
    static float fbank_f[40][513] = {0};
    static float fbank_T[513][40] = {0};
    static float filter_banks[49][40] = {0};
    static float power_spectrum[49][513] = {0};
    static float mfcc[49][10] = {0};
    static float mfcc_f[490] = {0};

    f = fopen(path, "rb");
    WAVE_t wavfile = initWAVE_t();
    loadWAVEFile(&wavfile, f);
    print_WAVE(wavfile);
    sampleNum=wavfile.WAVEParams.numSamples;
    channels = wavfile.WAVEParams.numChannels;

    d1 = CreateMatrix(wavfile.WAVEParams.numChannels, wavfile.WAVEParams.numSamples);
    for (i0 = 1; i0 <= wavfile.WAVEParams.numChannels; i0++) {
        for (i = 1; i <= wavfile.WAVEParams.numSamples; i++)
            d1[i0][i] = (double)wavfile.DATA.data[i0][i];
    }

    d2 = CreateVector(wlen);
    d3 = CreateVector(MFCCNum);
    fbank_i = CreateVector(bankNum);
    fbank = CreateVector(bankNum);
    info = InitFBank(wlen, samplePeriod, bankNum, lowpassfre, hipassfre, 0, 1, 0, 1.0, 0, 0);
    hamWin=GenHamWindow(wlen);
    m = 0;
    n=0;
    for (j = 0; j <= (sampleNum - wlen + 1); j += inc) {
        for (i0 = 1; i0 <= vecNum; i0++) {
            for (i = 1; i <= wlen; i++) {
                d2[i] = d1[i0][i+j];
            }
            Wave2Fbank(d2, fbank_i, &te, &te2, info);
            for (n = 1; n <= power_spe_w; n++) {
                power_spectrum[m][n-1] = fbank_i[n];
            }
            m++;
        }
    }

    for (m = 0; m < nfilt; m++) {
        for (k = bin[m]; k < bin[m+1]; k++) {
            fbank_f[m][k] = (float)(k - bin[m]) / (float)(bin[m + 1] - bin[m]);
        }
        for (k = bin[m+1]; k < bin[m+2]; k++) {
            fbank_f[m][k] = (float)(bin[m + 2] - k) / (float)(bin[m + 2] - bin[m + 1]);
        }
    }

    for (i = 0; i < power_spe_w; i++) {
        for (j=0; j<nfilt; j++)
            fbank_T[i][j] = fbank_f[j][i];
    }

    for (i = 0; i < power_spe_l; i++) {
        for (j = 0; j < nfilt; j++) {
            for (k = 0; k < power_spe_w; k++)
                filter_banks[i][j] += power_spectrum[i][k] * fbank_T[k][j];
        }
    }
    for (i = 0; i < power_spe_l; i++) {
        for (j = 0; j < nfilt; j++) {
            for (k = 0; k < power_spe_w; k++)
                if (filter_banks[i][j] <= 0)
                    filter_banks[i][j] = 0.000000000001;
                filter_banks[i][j] = log10(filter_banks[i][j]);
        }
    }

    float mfnorm = sqrt(2.0 / (double)numChan);
    float pi_factor = pi / (double)numChan;
    for (i = 0; i < power_spe_l; i++) {
        for (j = 0; j < num_ceps; j++) {
            mfcc[i][j] = 0.0;
            double x = (double)(j) * pi_factor;
            for (k = 1; k <= numChan; k++)
                mfcc[i][j] += (filter_banks[i][k-1]) * cos(x*(k - 0.5));
            mfcc[i][j] *= mfnorm;
        }
    }

    float lift[num_ceps] = {1,2.56546322 , 4.09905813  ,5.56956514 , 6.94704899 , 8.20346807,  9.31324532, 10.25378886 ,11.00595195 ,11.55442271} ;
    m = 0;
    for (i = 0; i < power_spe_l; i++) {
        for (j = 0; j < num_ceps; j++) {
            mfcc_f[m] = mfcc[i][j] * lift[j];
            m++;
        }
    }

    return (float*)mfcc_f;
}


int run_network(void *qcontext, unsigned char *qrawdata,int fbmode,unsigned char *fbbuf)
{
    img_classify_out_t *cls_out = NULL;
    obj_detect_out_t *obj_detect_out=NULL;
    nn_output *outdata = NULL;
    aml_module_t modelType;

    int sz=1;
    int j;
    unsigned int i=0;
    int ret = 0;
    float *buffer = NULL;
    FILE *fp,*File;
    aml_output_config_t outconfig;

    inData.input = qrawdata;
    ret = aml_module_input_set(qcontext,&inData);
    if (ret != 0)
        printf("aml_module_input_set error\n");

    outconfig.typeSize = sizeof(aml_output_config_t);
    modelType = IMAGE_CLASSIFY;
    outconfig.format = AML_OUTDATA_FLOAT32;
    outdata = (nn_output*)aml_module_output_get(qcontext,outconfig);

    cls_out = (img_classify_out_t*)post_process_all_module(modelType,outdata);

    for (i = 0; i < 5; i++)
        printf("top %d:score--%f,class--%d\n",i,cls_out->score[i],cls_out->topClass[i]);

    return ret;
}

void* init_network(int argc,char **argv)
{
    int size=0;
    aml_config config;
    void *qcontext = NULL;

    memset(&config,0,sizeof(aml_config));
    FILE *fp,*File;

    config.path = (const char *)argv[1];
    config.nbgType = NN_NBG_FILE;
    input_width = 64;
    input_high = 64;
    config.modelType = KERAS;
    qcontext = aml_module_create(&config);
    if (qcontext == NULL) {
        printf("amlnn_init is fail\n");
        return NULL;
    }
    inData.input_index = 0;
    inData.size = 490;
    inData.input_type = TENSOR_RAW_DATA;
    if (config.nbgType == NN_NBG_MEMORY && config.pdata != NULL)
        free((void*)config.pdata);

    return qcontext;
}

int destroy_network(void *qcontext)
{
    if (outbuf) aml_util_freeAlignedBuffer(NULL, outbuf);
    if (inbuf) aml_util_freeAlignedBuffer(NULL, inbuf);

    int ret = aml_module_destroy(qcontext);
    return ret;
}

void* net_thread_func(void *args)
{
    jpath = (char*)args;
    int ret = 0;
    FILE *fp;
    float *rawdata_f = (float*) malloc(sizeof(float) * 490);

    rawdata = (unsigned char*) get_wav_decode_test(jpath);

    printf("get wake up demo test results\n");

    ret = run_network(context, (unsigned char *)rawdata,AML_IN_PICTURE,NULL);
    ret = destroy_network(context);
    if (ret != 0)
        printf("aml_module_destroy error\n");
    return (void*)0;
}

/*-------------------------------------------
                  Main Functions
-------------------------------------------*/
int main(int argc,char **argv)
{
    int ret = 0,i=0;
    pthread_t tid[2];
    int customID,powerStatus;
    void *thread_args = NULL;


    context = init_network(argc,argv);
    thread_args = (void*)argv[2];

    if (0 != pthread_create(&tid[1],NULL,net_thread_func,thread_args)) {
        fprintf(stderr, "Couldn't create thread func\n");
        return -1;
    }

    pthread_join(tid[1], NULL);

    return ret;
}
