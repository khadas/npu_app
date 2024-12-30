/****************************************************************************
*   Generated by ACUITY 5.0.0
*   Match ovxlib 1.1.1
*
*   Neural Network appliction post-process source file
****************************************************************************/
/*-------------------------------------------
                Includes
-------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsi_nn_pub.h"

#include "vnn_global.h"
#include "vnn_post_process.h"

#define _BASETSD_H

/*-------------------------------------------
                  Functions
-------------------------------------------*/
#if 0
static void save_output_data(vsi_nn_graph_t *graph)
{
    uint32_t i;
#define _DUMP_FILE_LENGTH 1028
#define _DUMP_SHAPE_LENGTH 128
    char filename[_DUMP_FILE_LENGTH] = {0}, shape[_DUMP_SHAPE_LENGTH] = {0};
    vsi_nn_tensor_t *tensor;

    for(i = 0; i < graph->output.num; i++)
    {
        tensor = vsi_nn_GetTensor(graph, graph->output.tensors[i]);
        vsi_nn_ShapeToString( tensor->attr.size, tensor->attr.dim_num,
            shape, _DUMP_SHAPE_LENGTH, FALSE );
        snprintf(filename, _DUMP_FILE_LENGTH, "output%u_%s.txt", i, shape);
        vsi_nn_SaveTensorToTextByFp32( graph, tensor, filename, NULL );

    }
}

static vsi_bool get_top
    (
    float *pfProb,
    float *pfMaxProb,
    uint32_t *pMaxClass,
    uint32_t outputCount,
    uint32_t topNum
    )
{
    uint32_t i, j;

    #define MAX_TOP_NUM 20
    if (topNum > MAX_TOP_NUM) return FALSE;

    memset(pfMaxProb, 0, sizeof(float) * topNum);
    memset(pMaxClass, 0xff, sizeof(float) * topNum);

    for (j = 0; j < topNum; j++)
    {
        for (i=0; i<outputCount; i++)
        {
            if ((i == *(pMaxClass+0)) || (i == *(pMaxClass+1)) || (i == *(pMaxClass+2)) ||
                (i == *(pMaxClass+3)) || (i == *(pMaxClass+4)))
            {
                continue;
            }

            if (pfProb[i] > *(pfMaxProb+j))
            {
                *(pfMaxProb+j) = pfProb[i];
                *(pMaxClass+j) = i;
            }
        }
    }

    return TRUE;
}
#endif
static vsi_status show_top5
    (
    vsi_nn_graph_t *graph,
    vsi_nn_tensor_t *tensor
    )
{
    vsi_status status = VSI_FAILURE;
    uint32_t i,sz,stride;
    uint8_t *buffer = NULL;
    uint8_t *tensor_data = NULL;
#if 0
    uint32_t MaxClass[5];
    float fMaxProb[5];
#endif
    sz = 1;
    for(i = 0; i < tensor->attr.dim_num; i++)
    {
        sz *= tensor->attr.size[i];
    }

    stride = vsi_nn_TypeGetBytes(tensor->attr.dtype.vx_type);
    tensor_data = (uint8_t *)vsi_nn_ConvertTensorToData(graph, tensor);
    buffer = (uint8_t *)malloc(sizeof(uint8_t) * sz);
#if 0//if 1 时生成bin文件，这时候运算的结果肯定是不匹配的；if 0 的时候是用bin作为输入
	FILE *fp = fopen("output.bin","wb");
	fwrite(tensor_data,sz * sizeof(uint8_t),1,fp);
	fclose(fp);
#else
	FILE *fp = fopen("output.bin","rb");
	if (!fp)
	{
		printf("The output.bin file is required in the current folder!\n");
		exit(1);
	}
	fread(buffer,sz * sizeof(uint8_t),1,fp);
	fclose(fp);
#endif
    for(i = 0; i < sz; i++)
    {
//        status = vsi_nn_DtypeToFloat32(&tensor_data[stride * i], &buffer[i], &tensor->attr.dtype);
		if(buffer[i] != tensor_data[i])
		{
//			printf("result not match %d %d  %d\n",buffer[i], tensor_data[i],i);
			printf("\nTest Fail.Wrong Result\n\n");
			exit(1);
		}
    }
#if 0
    if (!get_top(buffer, fMaxProb, MaxClass, sz, 5))
    {
        printf("Fail to show result.\n");
        goto final;
    }

    printf(" --- Top5 ---\n");
    for(i=0; i<5; i++)
    {
        printf("%3d: %8.6f\n", MaxClass[i], fMaxProb[i]);
    }

#endif
    status = VSI_SUCCESS;
//final:
    if(tensor_data)vsi_nn_Free(tensor_data);
    if(buffer)free(buffer);
    return status;
}

vsi_status vnn_PostProcessDncnn(vsi_nn_graph_t *graph)
{
    vsi_status status = VSI_FAILURE;

    /* Show the top5 result */
    status = show_top5(graph, vsi_nn_GetTensor(graph, graph->output.tensors[0]));
    TEST_CHECK_STATUS(status, final);

    /* Save all output tensor data to txt file */
//    save_output_data(graph);

final:
    return VSI_SUCCESS;
}
