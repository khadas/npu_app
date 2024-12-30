/*
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 */

// #define USE_PATTERN

#include "adla_runtime.h"
#include "adla_io.h"


#include <iostream>
#include <fstream>

#include <cassert>
#include <sstream>
#include <string.h>
#include <unistd.h>

#if defined(__linux__)
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif


#define USE_SET_BUFFER_API          (1)
#define TIME_SLOTS (15)
#define TIM_SLOT_ID_LOAD_MODELFILE  (0)
#define TIM_SLOT_ID_CREATE_CONTEXT  (1)
#define TIM_SLOT_ID_LOAD_INPUTFILE  (2)
#define TIM_SLOT_ID_LOAD_PATTERN    (3)
#define TIM_SLOT_ID_SET_INPUT       (4)
#define TIM_SLOT_ID_INVOKE          (5)
#define TIM_SLOT_ID_TEST            (6)
#define TIM_SLOT_ID_TEST2           (7)


uint64_t timer_begin[TIME_SLOTS];
uint64_t timer_end[TIME_SLOTS];

#define BILLION 1000000000ULL

#define ADLAK_ALIGN(x, a)               ((x + (a - 1)) & ~(a - 1))

#define ADLAK_PAGE_ALIGN(addr)          ADLAK_ALIGN(addr, 0x1000)

static uint64_t get_sys_time() {
#if defined(__linux__)
    struct timespec ts;

#ifdef CLOCK_MONOTONIC
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    ts.tv_nsec = 0;
    ts.tv_sec  = 0;
#endif
    return (uint64_t)((uint64_t)ts.tv_nsec + (uint64_t)ts.tv_sec * BILLION);

#elif defined(_WIN32)
    LARGE_INTEGER ln;
    QueryPerformanceCounter(&ln);
    return (uint64_t)ln.QuadPart;

#endif
}

static void time_begin(int id) { timer_begin[id] = get_sys_time(); }
static void time_end(int id) { timer_end[id] = get_sys_time(); }
static uint64_t time_get(int id) { return timer_end[id] - timer_begin[id]; }


static void * read_file(const char *file_path,int *file_size)
{
    FILE *fp = NULL;
    int size = 0;
    void * buf = NULL;
    fp = fopen(file_path,"rb");
    if (NULL == fp)
    {
        printf("open %s file fail!\n",file_path);
        return 0;
    }

    fseek(fp,0,SEEK_END);
    size = ftell(fp);
    rewind(fp);

    buf = malloc(sizeof(unsigned char) * size);

    fread(buf, 1, size, fp);

    fclose(fp);

    *file_size = size;
    return buf;

}

static void save_file(void * buffer, const char *file_path, unsigned int file_size)
{
    FILE* fp  = NULL;
    size_t count;

    fp = fopen(file_path,"wb");

    count = fwrite(buffer,sizeof(unsigned char),file_size,fp);
    if (count != file_size)
        printf("fail to save input!\n");

    fclose(fp);
}

void get_subfix(char * input_name, char *subfix)
{
    const char *ptr;
    char sep = '.';
    int pos,n;
    char buff[32] = {0};

    printf("input name : %s\n",input_name);
    ptr = strrchr(input_name, sep);
    pos = ptr - input_name;
    n = strlen(input_name) - (pos + 1);
    strncpy(buff, input_name+(pos+1), n);
    memcpy(subfix,buff,n);
}

static float uint8_to_fp32(uint8_t val, int32_t zeroPoint, float scale) {
    float result = 0.0f;
    result       = (val - (uint8_t)zeroPoint) * scale;
    return result;
}

static bool get_top(
    float *pf_prob,
    float *pf_max_prob,
    unsigned int *max_class,
    unsigned int out_put_count,
    unsigned int top_num
    )
{
    unsigned int i, j;

    if (top_num > 10) return false;

    memset(pf_max_prob, 0, sizeof(float) * top_num);
    memset(max_class, 0xff, sizeof(float) * top_num);
    for (j = 0; j < top_num; j++) {
        for (i=0; i<out_put_count; i++) {
            if ((i == *(max_class+0)) || (i == *(max_class+1)) || (i == *(max_class+2)) ||
                (i == *(max_class+3)) || (i == *(max_class+4)))
                continue;
            if (pf_prob[i] > *(pf_max_prob+j)) {
                *(pf_max_prob+j) = pf_prob[i];
                *(max_class+j) = i;
            }
        }
    }

    return true;
}


struct inference_profile_info {
    std::string model_name;
    ADLA_PROFILING_DATA *profiling_data;
    unsigned long long           invoke_deta_us;
};

static void ProfilingDataDump(struct inference_profile_info *inference_profile_info) {
    ADLA_PROFILING_DATA *profiling_data = inference_profile_info->profiling_data;
    std::stringstream    ss;
    ss << "\nProfilling data:\n";
    ss << "  model_name: " << inference_profile_info->model_name << "\n";

    fprintf(stdout, "%s", ss.str().c_str());
    fprintf(stdout, "  -------------From system-----------------\n");
    fprintf(stdout, "  core_freq_cur        : %lu Hz\n", profiling_data->ext.core_freq_cur);
    fprintf(stdout, "  axi_freq_cur         : %lu Hz\n", profiling_data->ext.axi_freq_cur);
    fprintf(stdout, "  mem_pool_size        : 0x%08X Bytes\n", profiling_data->ext.mem_pool_size);
    fprintf(stdout, "  mem_pool_used        : 0x%08X Bytes\n", profiling_data->ext.mem_pool_used);
    fprintf(stdout, "  mem_alloced_base     : 0x%08X Bytes\n",
            profiling_data->ext.mem_alloced_base);
    fprintf(stdout, "  mem_alloced_umd      : 0x%08X Bytes\n", profiling_data->ext.mem_alloced_umd);
    int elapsed_hw_sw =
        (int)profiling_data->ext.us_elapsed_in_hw_op + (int)profiling_data->ext.us_elapsed_in_sw_op;

    fprintf(stdout, "inference_time         : %7.2f ms \n", (float)profiling_data->inference_time_us / 1000);
    fprintf(stdout, "elapsed_in_hw_op       : %7.2f ms \n", (float)profiling_data->ext.us_elapsed_in_hw_op / 1000);
    fprintf(stdout, "elapsed_in_sw_op       : %7.2f ms \n", (float)profiling_data->ext.us_elapsed_in_sw_op / 1000);
    fprintf(stdout, "elapsed (hw_op+sw_op)  : %7.2f ms \n", (float)elapsed_hw_sw/1000);
    fprintf(stdout, "invoke_time            : %7.2f ms \n", (float)inference_profile_info->invoke_deta_us / 1000);
    fprintf(stdout, "FPS(pm)                : %7.2f inf/s  \n",(double)1000000 / (double)profiling_data->inference_time_us);
    fprintf(stdout, "FPS(hw+sw)             : %7.2f inf/s  \n", (double)1000000 / (double)(elapsed_hw_sw));
    fprintf(stdout, "FPS(invoke)            : %7.2f inf/s \n",(double)1000000 / (double)(inference_profile_info->invoke_deta_us));
    fprintf(stdout, "dram_read_size         : %7.2f Mbytes \n",(float)profiling_data->dram_read_bytes / (1024 * 1024));
    fprintf(stdout, "dram_write_size        : %7.2f Mbytes \n",(float)profiling_data->dram_write_bytes / (1024 * 1024));
    fprintf(stdout, "BW(Ddr_data)           : %7.2f Mbytes \n",(double)(profiling_data->dram_read_bytes + profiling_data->dram_write_bytes) /
                (1024 * 1024));
    fprintf(stdout, "sram_read_bytes        : %7.2f Mbytes \n",(float)profiling_data->sram_read_bytes / (1024 * 1024));
    fprintf(stdout, "sram_write_bytes       : %7.2f Mbytes  \n",(float)profiling_data->sram_write_bytes / (1024 * 1024));
    fprintf(stdout, "BW(Sram_data)          : %7.2f Mbytes  \n",(double)(profiling_data->sram_read_bytes + profiling_data->sram_write_bytes) /
                (1024 * 1024));
    fprintf(stdout, "memory_usage_bytes     : %7.2f Mbytes \n",(float)profiling_data->memory_usage_bytes / (1024 * 1024));
    fprintf(stdout, "----------------------------print end---------------\n");
}

ADLA_HANDLE context_gloable;

static void sample_adla_handle_signal(int32_t signo) {
    if (SIGINT == signo || SIGTERM == signo) {
        // adla_context_destroy(ctx);
        printf("Ctrl c to kill app \n");
        AdlaDestroyContext(context_gloable);
    }
    exit(-1);
}


int main(int argc, char* argv[])
{
    int         loop_times      = 1;
    int         is_profile      = 1;
    int         is_internal_io  = 1;
    int         is_save_io      = -1;
    int         check_golden    = -1;


    char *      is_save_output_str;
    char *      check_golden_str;
    char *      is_profile_str;
    char *      is_internal_io_str;


    char *      model_path      = argv[1];
    char *      input_path      = argv[2];
    char *      golden_path     = argv[3];

    void *      input_data_ptr  = nullptr;
    void *      golden_data_ptr = nullptr;

    ADLA_HANDLE context;
    ADLA_INPUT_ARGS input_args;
    unsigned long long create_context_time_us = 0;

    int32_t     num_inputs      = 0;
    int32_t     num_outputs     = 0;


    if (argc < 5)
    {
        printf("Usage: ./runtime [xxx.adla] [input] [golden] [loop_times]\n");
        return -1;
    }


    is_save_output_str = getenv("NN_SAVE_OUTPUT");
    if (is_save_output_str)
    {
        is_save_io = atoi(is_save_output_str);
    }

    check_golden_str = getenv("NN_CHECK_OUTPUT");
    if (check_golden_str)
    {
        check_golden = atoi(check_golden_str);
    }

    is_profile_str = getenv("NN_PROFILING");
    if (is_profile_str)
    {
        is_profile = atoi(is_profile_str);
    }

    is_internal_io_str = getenv("NN_INTERNAL_IO");
    if (is_internal_io_str)
    {
        is_internal_io = atoi(is_internal_io_str);
    }

    signal(SIGINT, sample_adla_handle_signal);
    signal(SIGTERM, sample_adla_handle_signal);



    if (argv[4] == NULL)
    {
        loop_times = 1;
    }
    else
    {
        loop_times = atoi(argv[4]);
    }
    printf("loop times :%d\n",loop_times);


    if (is_internal_io != 1) {
        if (!model_path || !input_path)
        {
            fprintf(stderr, "fail to get model_path or input_path!\n");
            printf("Usage: ./runtime [xxx.adla] [input] [golden] [loop_times]\n");
            return -1;
        }
    }

    // load input data
    int32_t input_size = 0;
    int32_t golden_size = 0;

    struct inference_profile_info inference_profile_info;

    // load model
    input_args.model_type = ADLA_MODEL_TYPE_ADLA_LOADABLE;
    input_args.input_type = ADLA_MODEL_IN_OUT_TYPE_FILE;
    input_args.model_path = model_path;

    time_begin(TIM_SLOT_ID_CREATE_CONTEXT);
    if (AdlaCreateContext(&context, &input_args) != AdlaStatus_Success)
    {
        fprintf(stderr, "AdlaCreateContext failed!\n");
        return -1;
    }
    time_end(TIM_SLOT_ID_CREATE_CONTEXT);
    create_context_time_us = (unsigned long long)time_get(TIM_SLOT_ID_CREATE_CONTEXT)/1000;
    //printf("create context time :%lld us\n",create_context_time_us);
    //fprintf(stdout, "AdlaCreateContext succeeded.\n");
    context_gloable = context;

    /*     get input/output information   */
    AdlaGetNumInputs(context, &num_inputs);
    AdlaGetNumOutputs(context, &num_outputs);

    // setup input data
    assert(num_inputs == 1); // only accept one input currently

    /*  set input */


    ADLA_TENSOR_DESC desc = { 0 };

    AdlaGetInputDesc(context, 0, &desc);

    if (is_internal_io == 1) {
        input_data_ptr = inputdat;
    } else {
        input_data_ptr = read_file(input_path, &input_size);

    }

    if (is_save_io == 1)
    {
        char input_file_path[100] = {0};
        sprintf(input_file_path,"input_0.bin");
        save_file(input_data_ptr,input_file_path,desc.size);
   }

    AdlaSetInput(context, 0, input_data_ptr);

    /* setup output buffers*/
    #define MAX_OUTPUT_BUFFER_CNT     100

    int32_t     zeroPoint[MAX_OUTPUT_BUFFER_CNT]    = {0};
    float       scale[MAX_OUTPUT_BUFFER_CNT]        = {0};
    int         out_cnt[MAX_OUTPUT_BUFFER_CNT]      = {0};

    unsigned char* output_buffer[MAX_OUTPUT_BUFFER_CNT];


    for (auto i = 0; i < num_outputs; ++i)
    {
        ADLA_TENSOR_DESC desc = { 0 };

        AdlaGetOutputDesc(context, i, &desc);

        zeroPoint[i] = desc.zero_point;
        scale[i] = desc.scale;
        out_cnt[i] = desc.size;
        //printf("output desc.height :%d, desc.width:%d, desc.channels:%d, size :%d\n",desc.height,desc.width,desc.channels,desc.size);


        output_buffer[i] = (unsigned char *)malloc(out_cnt[i] * sizeof(unsigned char));
        memset(output_buffer[i],0,out_cnt[i]);

        AdlaSetOutput(context, i, output_buffer[i]);

    }

    ADLA_PROFILING_DATA profiling_data;
    if (is_profile == 1) {
        AdlaEnableProfiling(context);
   }

    float* f_outbuf = (float *)malloc(out_cnt[0] * sizeof(float));

    int             top_num         = 5;
    unsigned int    max_class[5];
    float           f_max_prob[5];

    for (int index = 0;index < loop_times;index++) {
         AdlaInvoke(context);
         AdlaWait(context);
    }

    if (check_golden == 1)
    {
        if (is_internal_io == 1) {
            golden_data_ptr = golden;
        } else {
            golden_data_ptr = read_file(golden_path, &golden_size);
        }

        if (memcmp(output_buffer[0],golden_data_ptr,out_cnt[0]) == 0) {
            printf(" output_buffer golden_data_ptr memory compare pass!\n");
        } else {
            printf(" output_buffer golden_data_ptr memory compare fail!\n");
        }

        if (is_internal_io != 1) {
            if (golden_data_ptr)
            {
                free(golden_data_ptr);
            }
        }
         golden_data_ptr = NULL;
     }


    if (is_profile == 1)
    {

        AdlaGetProfilingData(context, &profiling_data);

        inference_profile_info.invoke_deta_us =
                                        (unsigned long long)time_get(TIM_SLOT_ID_INVOKE) / (1000*loop_times);
        inference_profile_info.model_name = model_path;
        inference_profile_info.profiling_data = &profiling_data;
        inference_profile_info.profiling_data->ext.us_elapsed_in_sw_op = (int32_t)(inference_profile_info.profiling_data->ext.us_elapsed_in_sw_op);
        inference_profile_info.profiling_data->ext.us_elapsed_in_hw_op = (int32_t)(inference_profile_info.profiling_data->ext.us_elapsed_in_hw_op/loop_times);

        ProfilingDataDump(&inference_profile_info);
        AdlaDisableProfiling(context);
    }

/*
    unsigned long long invoke_deta_us = (unsigned long long)time_get(TIM_SLOT_ID_INVOKE) / (1000*(loop_times));
     printf("invoke time :%7.2f ms\n",(float) invoke_deta_us/1000);
*/
    if (is_save_io == 1)
    {
        char output_file_path[100] = {0};

        for (int i = 0; i < num_outputs; i++)
        {
            sprintf(output_file_path,"output_%d.bin",i);

            save_file(output_buffer[i],output_file_path,out_cnt[i]);
        }
    }

    if (is_profile == 1)
    {
        for (int i = 0; i < out_cnt[0]; i++) {
            f_outbuf[i] = uint8_to_fp32(output_buffer[0][i], zeroPoint[0], scale[0]);
        }

        get_top(f_outbuf,f_max_prob, max_class, out_cnt[0], top_num);
        printf("-----------------top 5----------------\n");
        for (int i = 0; i < top_num; i++) {
            printf(" %d : %f; ",max_class[i], f_max_prob[i]);
        }
        printf("\n------------------------------------\n");
    }

    for (int i = 0; i < num_outputs; i++)
    {
        if (output_buffer[i])
        {
            free(output_buffer[i]);
            output_buffer[i] = NULL;
        }
    }

    if (f_outbuf)
    {
        free(f_outbuf);
        f_outbuf = NULL;
    }

    if (is_internal_io != 1) {
        if (input_data_ptr)
        {
            free(input_data_ptr);
            input_data_ptr = NULL;
        }
    }
    printf("-------------- finished -------------\n");
    AdlaDestroyContext(context);
    return 0;
}
