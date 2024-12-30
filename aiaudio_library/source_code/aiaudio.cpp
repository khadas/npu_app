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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#define USE_DMA_BUFFER

#include <math.h>
#include <float.h>
#include <unistd.h>
using namespace std;

static unsigned char temp[112*112*3];
static unsigned char *inbuf_det = NULL;
static unsigned char *inbuf_reg = NULL;
static unsigned char *outbuf = NULL;

#include "nn_sdk.h"
#include "nn_demo.h"
#include "nn_detect.h"
#include "nn_detect_utils.h"
#include "detect_log.h"
extern "C"

typedef unsigned char   uint8_t;
int g_detect_number = DETECT_NUM;

#define _SET_STATUS_(status, stat, lbl) do {\
    status = stat; \
    goto lbl; \
}while(0)

#define _CHECK_STATUS_(status, lbl) do {\
    if (NULL == status) {\
        goto lbl; \
    } \
}while(0)

typedef enum {
    NETWORK_UNINIT,
    NETWORK_INIT,
    NETWORK_PREPARING,
    NETWORK_PROCESSING,
} network_status;

typedef struct {
    dev_type type;
    char* value;
    char* info;
}chip_info;

typedef void* (*aml_module_create)(aml_config* config);
typedef void (*aml_module_input_set)(void* context,nn_input *pInput);
typedef void* (*aml_module_output_get)(void* context,aml_output_config_t outconfig);
typedef void (*aml_module_destroy)(void* context);
typedef void (*aml_util_getHardwareStatus)(int* customID,int *powerStatus,int *ddk_version);
typedef void* (*post_process_all_module)(aml_module_t type,nn_output *pOut);

typedef void *(*aml_util_mallocAlignedBuffer)(int mem_size);
typedef void (*aml_util_freeAlignedBuffer)(unsigned char *addr);
typedef void (*aml_util_swapInputBuffer)(void *context,void *newBuffer,unsigned int inputId);
typedef void (*aml_util_flushTensorHandle)(void* context,aml_flush_type_t type);

typedef struct function_process {
    aml_module_create              module_create;
    aml_module_input_set           module_input_set;
    aml_module_output_get          module_output_get;
    aml_module_destroy             module_destroy;
    aml_util_getHardwareStatus     getHardwareStatus;
    post_process_all_module        post_process;
    aml_util_mallocAlignedBuffer   mallocAlignedBuffer;
    aml_util_freeAlignedBuffer     freeAlignedBuffer;
    aml_util_swapInputBuffer       swapInputBuffer;
    aml_util_flushTensorHandle     flushTensorHandle;
}network_process;

typedef struct detect_network {
    det_model_type      mtype;
    int                 input_size;
    uint8_t             *input_ptr;
    network_status      status;
    void                *context;
    network_process     process;
    void *              handle_id_user;
    void *              handle_id_demo;
}det_network_t, *p_det_network_t;

dev_type g_dev_type = DEV_REVA;
static det_network_t network[DET_BUTT];

const char* data_file_path = "/etc/nn_data";
const char * file_name[DET_BUTT]= {
    "deepspeech2",
    "wakeup_cnn",
};

char model_path[48];

int get_input_size(det_model_type mtype);

static det_status_t check_input_param(input_image_t imageData, det_model_type modelType)
{
    int ret = DET_STATUS_OK;
    int size=0, imagesize=0;

    if (NULL == imageData.data) {
        LOGE("Data buffer is NULL");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    if (imageData.pixel_format != PIX_FMT_NV21 && imageData.pixel_format != PIX_FMT_RGB888) {
        LOGE("Current only support RGB888 and NV21");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    size = get_input_size(modelType);
    if (size == 0) {
        LOGE("Get_model_size fail!");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    if (imageData.pixel_format == PIX_FMT_RGB888) {
        imagesize = imageData.width * imageData.height * imageData.channel;
        if (size != imagesize) {
            LOGE("Inputsize not match! model size:%d vs input size:%d\n", size, imagesize);
            _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
        }
    }

exit:
    return ret;
}

static void check_and_set_dev_type(det_model_type modelType)
{
    int customID,powerStatus,ddk_version;
    p_det_network_t net = &network[modelType];

    net->process.getHardwareStatus(&customID,&powerStatus,&ddk_version);

    switch (customID) {
        case 125:
            LOGI("set_dev_type DEV_REVA and setenv 0");
            g_dev_type = DEV_REVA;
            setenv("DEV_TYPE", "0", 0);
            break;
        case 136:
            LOGI("set_dev_type DEV_REVB and setenv 1");
            g_dev_type = DEV_REVB;
            setenv("DEV_TYPE", "1", 0);
            break;
        case 153:
            LOGI("set_dev_type DEV_MS1 and setenv 2");
            g_dev_type = DEV_MS1;
            setenv("DEV_TYPE", "2", 0);
            break;
        case 161:
            LOGI("set_dev_type DEV_A1 and setenv 3");
            g_dev_type = DEV_A1;
            setenv("DEV_TYPE", "3", 0);
            break;
        case 185:
            LOGI("set_dev_type DEV_TM2 and setenv 4");
            g_dev_type = DEV_TM2;
            setenv("DEV_TYPE", "4", 0);
            break;
        case 190:
            LOGI("set_dev_type DEV_C2 and setenv 5");
            g_dev_type = DEV_C2;
            setenv("DEV_TYPE", "5", 0);
            break;
        case 30:
            LOGI("set_dev_type DEV_A5 and setenv 6");
            g_dev_type = DEV_A5;
            setenv("DEV_TYPE", "5", 0);
            break;  //need check a5 chip info (ysq)
        default:
            LOGE("set_dev_type fail, please check the type ");
            break;
    }

    LOGD("Read customID:%x\n", customID);
}

det_status_t check_and_set_function(det_model_type modelType)
{
    LOGP("Enter, dlopen so: libnnsdk.so");

    int ret = DET_STATUS_ERROR;
    p_det_network_t net = &network[modelType];

    net->handle_id_user =  dlopen("libnnsdk.so", RTLD_NOW);
    if (NULL == net->handle_id_user) {
        LOGE("dlopen libnnsdk.so failed!,%s",dlerror());
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    net->handle_id_demo =  dlopen("libnndemo.so", RTLD_NOW);
    if (NULL == net->handle_id_demo) {
        LOGE("dlopen libnndemo.so failed!,%s",dlerror());
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    net->process.module_create = (aml_module_create)dlsym(net->handle_id_user, "aml_module_create");
    _CHECK_STATUS_(net->process.module_create, exit);

    net->process.module_input_set = (aml_module_input_set)dlsym(net->handle_id_user, "aml_module_input_set");
    _CHECK_STATUS_(net->process.module_input_set, exit);

    net->process.module_output_get = (aml_module_output_get)dlsym(net->handle_id_user, "aml_module_output_get");
    _CHECK_STATUS_(net->process.module_output_get, exit);

    net->process.module_destroy = (aml_module_destroy)dlsym(net->handle_id_user, "aml_module_destroy");
    _CHECK_STATUS_(net->process.module_destroy, exit);

    net->process.getHardwareStatus = (aml_util_getHardwareStatus)dlsym(net->handle_id_user, "aml_util_getHardwareStatus");
    _CHECK_STATUS_(net->process.getHardwareStatus, exit);

    net->process.mallocAlignedBuffer = (aml_util_mallocAlignedBuffer)dlsym(net->handle_id_user, "aml_util_mallocAlignedBuffer");
    _CHECK_STATUS_(net->process.mallocAlignedBuffer, exit);

    net->process.freeAlignedBuffer = (aml_util_freeAlignedBuffer)dlsym(net->handle_id_user, "aml_util_freeAlignedBuffer");
    _CHECK_STATUS_(net->process.freeAlignedBuffer, exit);

    net->process.swapInputBuffer = (aml_util_swapInputBuffer)dlsym(net->handle_id_user, "aml_util_swapInputBuffer");
    _CHECK_STATUS_(net->process.swapInputBuffer, exit);

    net->process.flushTensorHandle = (aml_util_flushTensorHandle)dlsym(net->handle_id_user, "aml_util_flushTensorHandle");
    _CHECK_STATUS_(net->process.flushTensorHandle, exit);

    net->process.post_process = (post_process_all_module)dlsym(net->handle_id_demo, "post_process_all_module");
    _CHECK_STATUS_(net->process.post_process, exit);

    ret = DET_STATUS_OK;
exit:
    LOGP("Leave, dlopen so: libnnsdk.so, ret=%d", ret);
    return ret;
}

amlnn_model_type get_model_type(det_model_type mtype)
{
    amlnn_model_type type_val = TENSORFLOW;
    switch (mtype) {
    case DET_AML_DEEPSPEECH2:
        type_val = TENSORFLOW;
        break;
    case DET_AML_WAKE_UP_CNN:
        type_val = TENSORFLOW;
        break;
    default:
        break;
    }
    return type_val;
}

int get_input_size(det_model_type mtype)
{
    int input_size = 0;
    switch (mtype) {
    case DET_AML_DEEPSPEECH2:
        input_size = 756*161*1;
        break;
    case DET_AML_WAKE_UP_CNN:
        input_size = 160*160*1;
        break;
    default:
        break;
    }
    return input_size;
}

det_status_t det_get_model_name(const char * data_file_path, dev_type type,det_model_type mtype)
{
    int ret = DET_STATUS_OK;
    switch (type) {
    case DEV_REVA:
        sprintf(model_path, "%s/%s_7d.nb", data_file_path,file_name[mtype]);
        break;
    case DEV_REVB:
        sprintf(model_path, "%s/%s_88.nb", data_file_path,file_name[mtype]);
        break;
    case DEV_MS1:
        sprintf(model_path, "%s/%s_99.nb", data_file_path, file_name[mtype]);
        break;
    case DEV_A1:
        sprintf(model_path, "%s/%s_a1.nb", data_file_path, file_name[mtype]);
        break;
    case DEV_TM2:
        sprintf(model_path, "%s/%s_b9.nb", data_file_path, file_name[mtype]);
        break;
    case DEV_C2:
        sprintf(model_path, "%s/%s_be.nb", data_file_path, file_name[mtype]);
        break;
    case DEV_A5:
        sprintf(model_path, "%s/%s_1e.nb", data_file_path, file_name[mtype]);
        break;
    default:
        break;
    }
    return ret;
}

det_status_t det_set_model(det_model_type modelType)
{
    aml_config config;
    int ret = DET_STATUS_OK;
    int size = 0;
    p_det_network_t net = &network[modelType];

    LOGP("Enter, modeltype:%d", modelType);
    if (modelType >= DET_BUTT) {
        LOGE("Det_set_model fail, modelType >= BUTT");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    ret = check_and_set_function(modelType);
    check_and_set_dev_type(modelType);
    if (ret) {
        LOGE("ModelType so open failed or Not support now!!");
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    memset(&config,0,sizeof(aml_config));
    LOGI("Start create Model, data_file_path=%s",data_file_path);
    det_get_model_name(data_file_path,g_dev_type,modelType);

    size = get_input_size(modelType);

    config.path = (const char *)model_path;
    config.modelType = get_model_type(modelType);
    config.nbgType = NN_NBG_FILE;
    config.typeSize = sizeof(aml_config);

    net->context = net->process.module_create(&config);

    if (net->context == NULL) {
        LOGE("Model_create fail, file_path=%s, dev_type=%d", data_file_path, g_dev_type);
        _SET_STATUS_(ret, DET_STATUS_CREATE_NETWORK_FAIL, exit);
    }

    net->input_ptr = (uint8_t*) malloc( size* sizeof(uint8_t));

    if (net->input_ptr == NULL) {
        LOGE("det_set_model malloc input buffer fail");
        _SET_STATUS_(ret, DET_STATUS_MEMORY_ALLOC_FAIL, exit);
    }
    net->mtype = modelType;
    net->input_size = size * sizeof(uint8_t);
    LOGI("input_ptr size=%d, addr=%x", size, net->input_ptr);
    net->status = NETWORK_INIT;
exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_get_model_size(det_model_type modelType, int *width, int *height, int *channel)
{
    LOGP("Enter, modeltype:%d", modelType);

    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];
    if (!net->status) {
        LOGE("Model has not created! modeltype:%d", modelType);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    switch (modelType) {
    case DET_AML_DEEPSPEECH2:
        *width = 756;
        *height = 161;
        *channel = 1;
        break;
    case DET_AML_WAKE_UP_CNN:
        *width = 160;
        *height = 160;
        *channel = 1;
        break;
    default:
        break;
    }
exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_set_input(input_image_t imageData, det_model_type modelType)
{
    nn_input inData;
    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];
    int i,j,tmpdata,nn_width, nn_height, channels,offset;

    LOGP("Enter, modeltype:%d", modelType);
    if (!net->status) {
        LOGE("Model has not created! modeltype:%d", modelType);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }

    ret = check_input_param(imageData, modelType);
    if (ret) {
        LOGE("Check_input_param fail.");
        _SET_STATUS_(ret, DET_STATUS_PARAM_ERROR, exit);
    }

    if (net->input_ptr != inbuf_det && net->input_ptr != inbuf_reg) {
        switch (modelType) {
            case DET_AML_DEEPSPEECH2:
                det_get_model_size(modelType,&nn_width, &nn_height, &channels);
                for (i = 0; i < channels; i++) {
                    offset = nn_width * nn_height *( channels -1 - i);
                    for (j = 0; j < nn_width * nn_height; j++) {
                        tmpdata = (imageData.data[j * channels + i]>>1);
                        inbuf_det[j + offset] = (uint8_t)((tmpdata >  127) ? 127 : (tmpdata < -128) ? -128 : tmpdata);
                    }
                }
                break;
            case DET_AML_WAKE_UP_CNN:
                for (i = 0;i<net->input_size;i++) {
                    tmpdata = (imageData.data[i]>>1);
                    inbuf_reg[i] = tmpdata;
                }
                break;
            default:
                break;
        }
        net->process.flushTensorHandle(net->context, AML_INPUT_TENSOR);
    }

    if (ret) {
        LOGE("aml_module_input_set fail.");
        _SET_STATUS_(ret, DET_STATUS_SET_INPUT_ERROR, exit);
    }
    net->status = NETWORK_PREPARING;

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

aml_module_t get_sdk_modeltype(det_model_type modelType)
{
    aml_module_t type = AML_DEEPSPEECH2;
    switch (modelType) {
    case DET_AML_DEEPSPEECH2:
        type = AML_DEEPSPEECH2;
        break;
    case DET_AML_WAKE_UP_CNN:
        type = AML_WAKE_UP_CNN;
        break;
    default:
        break;
    }
    return type;
}

void post_process(det_model_type modelType,void* out,pDetResult resultData)
{
    LOGP("Enter, post_process modelType:%d", modelType);

    deepspeeck2_out_t              *deepspeeck2_out            = NULL;
    wake_up_cnn_out_t              *wake_up_cnn_out            = NULL;

    uint8_t *buffer = NULL;
    float left = 0, right = 0, top = 0, bot=0;
    unsigned int i = 0, j = 0;

    switch (modelType) {
    case DET_AML_DEEPSPEECH2:
        deepspeeck2_out = (deepspeeck2_out_t *)out;
        break;
    case DET_AML_WAKE_UP_CNN:
        wake_up_cnn_out = (wake_up_cnn_out_t *)out;
        break;
    default:
        break;
    }
    LOGP("Leave, post_process modelType:%d", modelType);
    return;
}

det_status_t det_get_result(pDetResult resultData, det_model_type modelType)
{
    aml_output_config_t outconfig;
    aml_module_t modelType_sdk;
    int ret = DET_STATUS_OK;

    p_det_network_t net = &network[modelType];
    void *out;
    nn_output *nn_out;
    LOGP("Enter, modeltype:%d", modelType);
    if (NETWORK_PREPARING != net->status) {
        LOGE("Model not create or not prepared! status=%d", net->status);
        _SET_STATUS_(ret, DET_STATUS_ERROR, exit);
    }
    outconfig.typeSize = sizeof(aml_output_config_t);
    modelType_sdk = get_sdk_modeltype(modelType);

    outconfig.perfMode = AML_NO_PERF;
    nn_out = (nn_output*)net->process.module_output_get(net->context,outconfig);
    out = (void*)net->process.post_process(modelType_sdk,nn_out);

    if (out == NULL) {
        LOGE("Process Net work fail");
        _SET_STATUS_(ret, DET_STATUS_PROCESS_NETWORK_FAIL, exit);
    }

    post_process(modelType,out,resultData);
    net->status = NETWORK_PROCESSING;

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}


det_status_t det_release_model(det_model_type modelType)
{
    int ret = DET_STATUS_OK;
    p_det_network_t net = &network[modelType];

    LOGP("Enter, modeltype:%d", modelType);
    if (!net->status) {
        LOGW("Model has benn released!");
        _SET_STATUS_(ret, DET_STATUS_OK, exit);
    }

    if (net->context != NULL) {
        net->process.module_destroy(network[modelType].context);
        net->context = NULL;
    }

    if (net->input_ptr) {
        free(net->input_ptr);
        net->input_ptr = NULL;
    }

    dlclose(net->handle_id_user);
    net->handle_id_user = NULL;
    dlclose(net->handle_id_demo);
    net->handle_id_demo = NULL;
    net->status = NETWORK_UNINIT;

exit:
    LOGP("Leave, modeltype:%d", modelType);
    return ret;
}

det_status_t det_set_log_config(det_debug_level_t level,det_log_format_t output_format)
{
    LOGP("Enter, level:%d", level);
    det_set_log_level(level, output_format);
    LOGP("Leave, level:%d", level);
    return 0;
}

