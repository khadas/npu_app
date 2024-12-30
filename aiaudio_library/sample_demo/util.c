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
#include <math.h>
#include "nn_sdk.h"
#include "nn_util.h"
#include "nn_demo.h"


float* get_tensor_Data(const char * path, int length)
{
        FILE* fp = fopen(path,"rb");
        float fval = 0.0;
        float* data = (float*)malloc(length*sizeof(float));

        printf("\n*****input value(read from tensor file)*****\n");
        for (int i=0; i<length; i++) {
            if (fscanf( fp, "%f ", &fval ) != 1)
            {
                printf("Read tensor file fail.\n");
                printf("Please check file lines or if the file contains illegal characters\n");
                return NULL;
            }
            data[i]=fval;
            if (i < 8)
                printf("data[%d]=%f\n",i,fval);

        }
        printf("*****input value*****\n\n");
        fclose(fp);
        return data;
}


float Float16ToFloat32(const signed short* src , float* dst ,int lenth)
{
    signed int t1;
    signed int t2;
    signed int t3;
    float out;
    int i;
    for (i = 0 ;i < lenth ;i++)
    {
        t1 = src[i] & 0x7fff;                       // Non-sign bits
        t2 = src[i] & 0x8000;                       // Sign bit
        t3 = src[i] & 0x7c00;                       // Exponent

        t1 <<= 13;                              // Align mantissa on MSB
        t2 <<= 16;                              // Shift sign bit into position

        t1 += 0x38000000;                       // Adjust bias

        t1 = (t3 == 0 ? 0 : t1);                // Denormals-as-zero

        t1 |= t2;
        *((unsigned int*)&out) = t1;                 // Re-insert sign bit
        dst[i] = out;

    }
    return out;
}

float *dtype_To_F32(nn_output * outdata ,int sz)
{
    int stride, fl, i, zeropoint;
    float scale;
    unsigned char *buffer_u8 = NULL;
    signed char *buffer_int8 = NULL;
    signed short *buffer_int16 = NULL;
    float *buffer_f32 = NULL;

    buffer_f32 = (float *)malloc(sizeof(float) * sz );

    if (outdata->out[0].param->data_format == NN_BUFFER_FORMAT_UINT8)
    {
        stride = (outdata->out[0].size)/sz;
        scale = outdata->out[0].param->quant_data.affine.scale;
        zeropoint =  outdata->out[0].param->quant_data.affine.zeroPoint;

        buffer_u8 = (unsigned char*)outdata->out[0].buf;
        for (i = 0; i < sz; i++)
        {
            buffer_f32[i] = (float)(buffer_u8[stride * i] - zeropoint) * scale;
        }
    }

    else if (outdata->out[0].param->data_format == NN_BUFFER_FORMAT_INT8)
    {
        buffer_int8 = (signed char*)outdata->out[0].buf;
        if (outdata->out[0].param->quant_data.dfp.fixed_point_pos >= 0)
        {
            fl = 1 << (outdata->out[0].param->quant_data.dfp.fixed_point_pos);
            for (i = 0; i < sz; i++)
            {
                buffer_f32[i] = (float)buffer_int8[i] * (1.0/(float)fl);
            }
        }
        else
        {
            fl = 1 << (-outdata->out[0].param->quant_data.dfp.fixed_point_pos);
            for (i = 0; i < sz; i++)
                buffer_f32[i] = (float)buffer_int8[i] * ((float)fl);
        }
    }

    else if (outdata->out[0].param->data_format == NN_BUFFER_FORMAT_INT16)
    {
        buffer_int16 =  (signed short*)outdata->out[0].buf;
        if (outdata->out[0].param->quant_data.dfp.fixed_point_pos >= 0)
        {
            fl = 1 << (outdata->out[0].param->quant_data.dfp.fixed_point_pos);
            for (i = 0; i < sz; i++)
            {
                buffer_f32[i] = (float)((buffer_int16[i]) * (1.0/(float)fl));
            }
        }
        else
        {
            fl = 1 << (-outdata->out[0].param->quant_data.dfp.fixed_point_pos);
            for (i = 0; i < sz; i++)
                buffer_f32[i] = (float)((buffer_int16[i]) * ((float)fl));
        }
    }
    else if (outdata->out[0].param->data_format == NN_BUFFER_FORMAT_FP16 )
    {
        buffer_int16 = (signed short*)outdata->out[0].buf;

        Float16ToFloat32(buffer_int16 ,buffer_f32 ,sz);
    }

    else if (outdata->out[0].param->data_format == NN_BUFFER_FORMAT_FP32)
    {
        memcpy(buffer_f32, outdata->out[0].buf, sz);
    }
    else
    {
        printf("Error: currently not support type, type = %d\n", outdata->out[0].param->data_format);
    }
    return buffer_f32;
}
