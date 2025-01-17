/****************************************************************************
*   Generated by ACUITY 5.0.0
*   Match ovxlib 1.1.1
*
*   Neural Network application project entry file
****************************************************************************/
/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <time.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#define _BASETSD_H

#include "vsi_nn_pub.h"

#include "vnn_global.h"
#include "vnn_pre_process.h"
#include "vnn_post_process.h"
#include "vnn_dncnn.h"

/*-------------------------------------------
        Macros and Variables
-------------------------------------------*/

/*-------------------------------------------
                  Functions
-------------------------------------------*/
static void vnn_ReleaseNeuralNetwork
    (
    vsi_nn_graph_t *graph
    )
{
    vnn_ReleaseDncnn( graph, TRUE );
    if (vnn_UseImagePreprocessNode())
    {
        vnn_ReleaseBufferImage();
    }
}

static vsi_status vnn_PostProcessNeuralNetwork
    (
    vsi_nn_graph_t *graph
    )
{
    return vnn_PostProcessDncnn( graph );
}

#define BILLION                                 1000000000
static vx_uint64 get_perf_count()
{
#if defined(__linux__) || defined(__ANDROID__) || defined(__QNX__) || defined(__CYGWIN__)
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (vx_uint64)((vx_uint64)ts.tv_nsec + (vx_uint64)ts.tv_sec * BILLION);
#elif defined(_WIN32) || defined(UNDER_CE)
    LARGE_INTEGER ln;

    QueryPerformanceCounter(&ln);

    return (vx_uint64)ln.QuadPart;
#endif
}

static vsi_status vnn_VerifyGraph
    (
    vsi_nn_graph_t *graph
    )
{
    vsi_status status = VSI_FAILURE;
    vx_uint64 tmsStart, tmsEnd, msVal, usVal;

    /* Verify graph */
    printf("Verify...\n");
    tmsStart = get_perf_count();
    status = vsi_nn_VerifyGraph( graph );
    TEST_CHECK_STATUS( status, final );
    tmsEnd = get_perf_count();
    msVal = (tmsEnd - tmsStart)/1000000;
    usVal = (tmsEnd - tmsStart)/1000;
    printf("Verify Graph: %ldms or %ldus\n", (long)msVal, (long)usVal);

final:
    return status;
}

static vsi_status vnn_ProcessGraph
    (
    vsi_nn_graph_t *graph
    )
{
    vsi_status status = VSI_FAILURE;
    int32_t i,loop,not_print_time;
    char *loop_s;
    char *print_time;
    vx_uint64 tmsStart, tmsEnd, sigStart, sigEnd, msVal, usVal;

    status = VSI_FAILURE;
    loop = 1; /* default loop time is 1 */
    loop_s = getenv("VNN_LOOP_TIME");
    print_time = getenv("VNN_NOT_PRINT_TIME");
    if (loop_s)
    {
        loop = atoi(loop_s);
    }
    if (print_time)
    {
        not_print_time = atoi(print_time);
    }

    /* Run graph */
    tmsStart = get_perf_count();
    printf("Start run graph [%d] times...\n", loop);
    for(i = 0; i < loop; i++)
    {
        sigStart = get_perf_count();
        status = vsi_nn_RunGraph(graph);
        if(status != VSI_SUCCESS)
        {
            printf("Run graph the %d time fail\n", i);
        }
        TEST_CHECK_STATUS( status, final );

        sigEnd = get_perf_count();
        msVal = (sigEnd - sigStart)/1000000;
        usVal = (sigEnd - sigStart)/1000;
        if (not_print_time == 0)
        {
            printf("Run the %u time: %lldms or %lldus\n", (i+1), msVal, usVal);
        }
        if (msVal > 1000)
        {
            printf("\nTest Fail,Execution Time Too Long\n\n");
            exit(1);
        }
    /* Post process output data */
    status = vnn_PostProcessNeuralNetwork( graph );
    TEST_CHECK_STATUS( status, final );
    }
    tmsEnd = get_perf_count();
    msVal = (tmsEnd - tmsStart)/1000000;
    usVal = (tmsEnd - tmsStart)/1000;
    printf("Execution time:\n");
    printf("Total   %lldms, ", msVal);
    printf("Average %.2fms \n", ((float)usVal)/1000/loop);

final:
    return status;
}

static vsi_status vnn_PreProcessNeuralNetwork
    (
    vsi_nn_graph_t *graph,
    int argc,
    char **argv
    )
{
    /*
     * argv0:   execute file
     * argv1:   data file
     * argv2~n: inputs n file
     */
    const char **inputs = (const char **)argv + 2;
    uint32_t input_num = argc - 2;

    if(vnn_UseImagePreprocessNode())
    {
        return vnn_PreProcessDncnn_ImageProcess(graph, inputs, input_num);
    }
    return vnn_PreProcessDncnn( graph, inputs, input_num );
}

static vsi_nn_graph_t *vnn_CreateNeuralNetwork
    (
    const char *data_file_name
    )
{
    vsi_nn_graph_t *graph = NULL;
    vx_uint64 tmsStart, tmsEnd, msVal, usVal;

    tmsStart = get_perf_count();
    graph = vnn_CreateDncnn(data_file_name, NULL);
    TEST_CHECK_PTR(graph, final);
    tmsEnd = get_perf_count();
    msVal = (tmsEnd - tmsStart)/1000000;
    usVal = (tmsEnd - tmsStart)/1000;
    printf("Create Neural Network: %lldms or %lldus\n", msVal, usVal);

final:
    return graph;
}

/*-------------------------------------------
                  Main Functions
-------------------------------------------*/
int main
    (
    int argc,
    char **argv
    )
{
    vsi_status status = VSI_FAILURE;
    vsi_nn_graph_t *graph;
    const char *data_name = NULL;

    if(argc < 3)
    {
        printf("Usage: %s data_file inputs...\n", argv[0]);
        return -1;
    }

    data_name = (const char *)argv[1];

    /* Create the neural network */
    graph = vnn_CreateNeuralNetwork( data_name );
    TEST_CHECK_PTR(graph, final);

    /* Pre process the image data */
    status = vnn_PreProcessNeuralNetwork( graph, argc, argv );
    TEST_CHECK_STATUS( status, final );

    /* Verify graph */
    status = vnn_VerifyGraph(graph);
    TEST_CHECK_STATUS(status, final);

    /* Process graph */
    status = vnn_ProcessGraph( graph );
    TEST_CHECK_STATUS( status, final );

    if(VNN_APP_DEBUG)
    {
        /* Dump all node outputs */
        vsi_nn_DumpGraphNodeOutputs(graph, "./network_dump", NULL, 0, TRUE, 0);
    }

    /* Post process output data */
//    status = vnn_PostProcessNeuralNetwork( graph );
//    TEST_CHECK_STATUS( status, final );
    printf("\nTest Pass.\n\n");
    return status;
final:
    printf("\nTest Fail: Error status %d\n\n",status);
    vnn_ReleaseNeuralNetwork( graph );
    return status;
}

