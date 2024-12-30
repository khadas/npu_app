/*
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 */

#pragma once

#include "adla.h"
#include <stdint.h>

#ifdef _MSC_VER
#ifdef ADLA_EXPORTS
#define ADLA_API __declspec(dllexport)
#else
#define ADLA_API __declspec(dllimport)
#endif
#else
#define ADLA_API __attribute__((visibility ("default")))
#endif

typedef void* ADLA_HANDLE;
typedef void* ADLA_MEMORY_HANDLE;
typedef uint64_t ADLA_EXTERNAL_BUFFER;
typedef int64_t ADLA_INVOKE_HANDLE;

typedef enum ADLA_DATA_TYPE
{
    ADLA_DATA_TYPE_INVALID = -1,
    ADLA_DATA_TYPE_UINT8 = 0,
    ADLA_DATA_TYPE_INT8 = 1,
    ADLA_DATA_TYPE_UINT16 = 2,
    ADLA_DATA_TYPE_INT16 = 3,
    ADLA_DATA_TYPE_UINT32 = 4,
    ADLA_DATA_TYPE_INT32 = 5,
    ADLA_DATA_TYPE_UINT64 = 6,
    ADLA_DATA_TYPE_INT64 = 7,
    ADLA_DATA_TYPE_FP16 = 8,
    ADLA_DATA_TYPE_FP32 = 9
} ADLA_DATA_TYPE;

typedef enum ADLA_DATA_LAYOUT
{
    ADLA_DATA_LAYOUT_NHWC = 0,
    ADLA_DATA_LAYOUT_NCHW = 1
} ADLA_DATA_LAYOUT;

typedef enum ADLA_EXTERNAL_BUFFER_TYPE
{
    ADLA_EXTERNAL_BUFFER_TYPE_ADLA_MEMORY = 0,
    ADLA_EXTERNAL_BUFFER_TYPE_PHYS_ADDR,
    ADLA_EXTERNAL_BUFFER_TYPE_DMA_BUF
} ADLA_EXTERNAL_BUFFER_TYPE;

typedef struct ADLA_INPUT_ARGS
{
    ADLA_MODEL_TYPE model_type;
    ADLA_MODEL_IN_OUT_TYPE input_type;

    union
    {
        // used when input_type is ADLA_MODEL_IN_OUT_TYPE_MEMORY
        struct
        {
            const void* model_data;
            int32_t model_size;
        };

        // used when input_type is ADLA_MODEL_IN_OUT_TYPE_FILE
        const char* model_path;
    };
} ADLA_INPUT_ARGS;

typedef struct ADLA_DEVICE_INFO
{
    int32_t working_frequency;      // in MHz
    int32_t axi_working_frequency;  // in MHz
    int32_t memory_size;            // memory size used by the device driver
    int32_t axi_sram_size;          // axi sram size allocated for the device
} ADLA_DEVICE_INFO;

typedef struct ADLA_CONTEXT_INFO
{
    int32_t memory_size;    // memory size used by the loaded model
    int32_t axi_sram_size;  // axi sram size required by the loaded model
    int32_t num_layers;
} ADLA_CONTEXT_INFO;

typedef struct ADLA_TENSOR_DESC
{
    int32_t index; // tensor index
    ADLA_DATA_TYPE type;
    ADLA_DATA_LAYOUT layout;
    int32_t batches;
    int32_t height;
    int32_t width;
    int32_t channels;
    int32_t size;
    float scale;
    int64_t zero_point;
} ADLA_TENSOR_DESC;

typedef struct ADLA_PROFILING_EXT_DATA
{
    uint64_t axi_freq_cur;   // current AXI working frequency
    uint64_t core_freq_cur;  // current ADLA working frequency
    uint64_t mem_alloced_base;
    uint64_t mem_alloced_umd;
    int64_t  mem_pool_size;  //-1:the limit base on the system
    uint64_t mem_pool_used;
    int32_t us_elapsed_in_hw_op;
    int32_t us_elapsed_in_sw_op;
} ADLA_PROFILING_EXT_DATA;

typedef struct ADLA_PROFILING_DATA
{
    uint64_t inference_time_us;
    uint64_t memory_usage_bytes;
    uint64_t dram_read_bytes;
    uint64_t dram_write_bytes;
    uint64_t sram_read_bytes;
    uint64_t sram_write_bytes;
    ADLA_PROFILING_EXT_DATA ext;
} ADLA_PROFILING_DATA;


typedef struct ADLA_BIND_BUFFER_REQUEST
{
    /****request info****/
    ADLA_EXTERNAL_BUFFER_TYPE type;
    ADLA_EXTERNAL_BUFFER buffer;/** set the physical address or the buffer handle **/
    int32_t size;/** set the space size of the buffer if the buffer type is PHYS_ADDR **/
    uint32_t mmap_enable;/**0:disable;other:enable**/
    /****return from driver****/
    void* virt_addr;    /** if mmap_enable is not 0, driver will return the virtual address **/
} ADLA_BIND_BUFFER_REQUEST;

#ifdef __cplusplus
extern "C" {
#endif

ADLA_API AdlaStatus AdlaCreateContext(ADLA_HANDLE* context, const ADLA_INPUT_ARGS* args);
ADLA_API AdlaStatus AdlaDestroyContext(ADLA_HANDLE context);
ADLA_API AdlaStatus AdlaGetDeviceInfo(ADLA_HANDLE context, ADLA_DEVICE_INFO* info);
ADLA_API AdlaStatus AdlaGetContextInfo(ADLA_HANDLE context, ADLA_CONTEXT_INFO* info);
ADLA_API AdlaStatus AdlaAllocateMemory(ADLA_HANDLE context, int32_t size, ADLA_MEMORY_HANDLE* memory, void** ptr);
ADLA_API AdlaStatus AdlaFreeMemory(ADLA_HANDLE context, ADLA_MEMORY_HANDLE memory);
ADLA_API AdlaStatus AdlaGetNumInputs(ADLA_HANDLE context, int32_t* num_inputs);
ADLA_API AdlaStatus AdlaGetNumOutputs(ADLA_HANDLE context, int32_t* num_outputs);
ADLA_API AdlaStatus AdlaGetInputDesc(ADLA_HANDLE context, int32_t index, ADLA_TENSOR_DESC* desc);
ADLA_API AdlaStatus AdlaGetOutputDesc(ADLA_HANDLE context, int32_t index, ADLA_TENSOR_DESC* desc);
ADLA_API AdlaStatus AdlaSetInput(ADLA_HANDLE context, int32_t index, const void* input_data);
ADLA_API AdlaStatus AdlaSetOutput(ADLA_HANDLE context, int32_t index, void* output_buffer);
ADLA_API AdlaStatus AdlaBindInput(ADLA_HANDLE context, int32_t index, ADLA_BIND_BUFFER_REQUEST* extern_buf_req);
ADLA_API AdlaStatus AdlaBindOutput(ADLA_HANDLE context, int32_t index, ADLA_BIND_BUFFER_REQUEST* extern_buf_req);
ADLA_API AdlaStatus AdlaInvoke(ADLA_HANDLE context);
ADLA_API AdlaStatus AdlaInvokeNoWait(ADLA_HANDLE context, ADLA_INVOKE_HANDLE* invoke_id, int32_t timeout_ms);
ADLA_API AdlaStatus AdlaWait(ADLA_HANDLE context);
ADLA_API AdlaStatus AdlaWaitWithId(ADLA_HANDLE context, ADLA_INVOKE_HANDLE invoke_id);
ADLA_API AdlaStatus AdlaGetWorkingFrequency(ADLA_HANDLE context, float* curr_freq, float* min_freq);
ADLA_API AdlaStatus AdlaSetWorkingFrequency(ADLA_HANDLE context, float freq);
ADLA_API AdlaStatus AdlaEnableProfiling(ADLA_HANDLE context);
ADLA_API AdlaStatus AdlaDisableProfiling(ADLA_HANDLE context);
ADLA_API AdlaStatus AdlaGetProfilingData(ADLA_HANDLE context, ADLA_PROFILING_DATA* profiling_data);

#ifdef __cplusplus
}
#endif
