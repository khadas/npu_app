#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#define USE_DMA_BUFFER

#include <math.h>
#include <float.h>
#include <unistd.h>
#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>
using namespace std;
using namespace cv;

static unsigned char temp[112*112*3];
static unsigned char *inbuf_det = NULL;
static unsigned char *inbuf_reg = NULL;
static unsigned char *outbuf = NULL;
static Mat mul_mat;

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
	aml_module_create	       module_create;
	aml_module_input_set	       module_input_set;
	aml_module_output_get	       module_output_get;
	aml_module_destroy	       module_destroy;
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
	uint8_t				*input_ptr;
	network_status		status;
	void                *context;
	network_process 	process;
	void *				handle_id_user;
	void *				handle_id_demo;
}det_network_t, *p_det_network_t;

dev_type g_dev_type = DEV_REVA;
static det_network_t network[DET_BUTT];

const char* data_file_path = "/etc/nn_data";
const char * file_name[DET_BUTT]= {
	"yolo_face",
	"yolo_v2",
	"yolo_v3",
	"yolo_tiny",
	"ssd",
	"mtcnn_v1",
	"mtcnn_v2",
	"faster_rcnn",
	"deeplab_v1",
	"deeplab_v2",
	"deeplab_v3",
	"facenet",
	"aml_face_recognition",
	"aml_face_detection",
	"aml_person_detection",
	"aml_face_detect_rfb",
};

char model_path[48];

int get_input_size(det_model_type mtype);

static det_status_t check_input_param(input_image_t imageData, det_model_type modelType)
{
	int ret = DET_STATUS_OK;
	int size=0,imagesize=0;

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
		imagesize = imageData.width*imageData.height*imageData.channel;
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

	switch (customID)
	{
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
	switch (mtype)
	{
	case DET_YOLOFACE_V2:
		type_val = CAFFE;
		break;
	case DET_YOLO_V2:
		type_val = DARKNET;
		break;
	case DET_YOLO_V3:
		type_val = DARKNET;
		break;
	case DET_YOLO_TINY:
		type_val = DARKNET;
		break;
	case DET_SSD:
		type_val = CAFFE;
		break;
	case DET_MTCNN_V1:
		type_val = CAFFE;
		break;
	case DET_MTCNN_V2:
		type_val = CAFFE;
		break;
	case DET_FASTER_RCNN:
		type_val = CAFFE;
		break;
	case DET_DEEPLAB_V1:
		type_val = TENSORFLOW;
		break;
	case DET_DEEPLAB_V2:
		type_val = TENSORFLOW;
		break;
	case DET_DEEPLAB_V3:
		type_val = TENSORFLOW;
		break;
	case DET_FACENET:
		type_val = TENSORFLOW;
		break;
	case DET_AML_FACE_RECOGNITION:
		type_val = TENSORFLOW;
		break;
	case DET_AML_FACE_DETECTION:
		type_val = TENSORFLOW;
		break;
	case DET_AML_PERSON_DETECTION:
		type_val = ONNX;
		break;
	case DET_AML_FACE_DETECT_RFB:
		type_val = CAFFE;
		break;
	default:
		break;
	}
	return type_val;
}

int get_input_size(det_model_type mtype)
{
	int input_size = 0;
	switch (mtype)
	{
	case DET_YOLOFACE_V2:
		input_size = 416*416*3;
		break;
	case DET_YOLO_V2:
		input_size = 416*416*3;
		break;
	case DET_YOLO_V3:
		input_size = 416*416*3;
		break;
	case DET_YOLO_TINY:
		input_size = 0;
		break;
	case DET_SSD:
		input_size = 0;
		break;
	case DET_MTCNN_V1:
		input_size = 0;
		break;
	case DET_MTCNN_V2:
		input_size = 0;
		break;
	case DET_FASTER_RCNN:
		input_size = 0;
		break;
	case DET_DEEPLAB_V1:
		input_size = 0;
		break;
	case DET_DEEPLAB_V2:
		input_size = 0;
		break;
	case DET_DEEPLAB_V3:
		input_size = 0;
		break;
	case DET_FACENET:
		input_size = 160*160*3;
		break;
	case DET_AML_FACE_RECOGNITION:
		input_size = 112*112*3;
		break;
	case DET_AML_FACE_DETECTION:
		input_size = 640*384*3;
		break;
	case DET_AML_PERSON_DETECTION:
		input_size = 640*384*3;
		break;
	case DET_AML_FACE_DETECT_RFB:
		input_size = 320*320*3;
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

	switch (modelType)
	{
	case DET_AML_PERSON_DETECTION:
	case DET_AML_FACE_DETECTION:
	case DET_YOLOFACE_V2:
	case DET_YOLO_V2:
	case DET_YOLO_V3:
	case DET_AML_FACE_DETECT_RFB:
		config.inOut.io_type = AML_IO_VIRTUAL;
		inbuf_det = (unsigned char *)net->process.mallocAlignedBuffer(size);
		config.inOut.inAddr[0] = inbuf_det;
		break;
	case DET_FACENET:
	case DET_AML_FACE_RECOGNITION:
		inbuf_reg = (unsigned char *)net->process.mallocAlignedBuffer(size);
		config.inOut.inAddr[0] = inbuf_reg;
		break;
	default:
		break;
	}

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
	net->input_size = size* sizeof(uint8_t);
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
	switch (modelType)
	{
	case DET_YOLOFACE_V2:
	case DET_YOLO_V2:
	case DET_YOLO_V3:
		*width = 416;
		*height = 416;
		*channel = 3;
		break;
	case DET_FACENET:
		*width = 160;
		*height = 160;
		*channel = 3;
		break;
	case DET_AML_FACE_RECOGNITION:
		*width = 112;
		*height = 112;
		*channel = 3;
		break;
	case DET_AML_FACE_DETECTION:
	case DET_AML_PERSON_DETECTION:
		*width = 640;
		*height = 384;
		*channel = 3;
		break;
	case DET_AML_FACE_DETECT_RFB:
		*width = 320;
		*height = 320;
		*channel = 3;
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

	if (net->input_ptr != inbuf_det && net->input_ptr != inbuf_reg)
	{
		switch (modelType)
		{
			case DET_YOLOFACE_V2:
				det_get_model_size(modelType,&nn_width, &nn_height, &channels);
				for (i = 0; i < channels; i++)
				{
					offset = nn_width * nn_height *( channels -1 - i);
					for (j = 0; j < nn_width * nn_height; j++)
					{
						tmpdata = (imageData.data[j * channels + i]>>1);
						inbuf_det[j + offset] = (uint8_t)((tmpdata >  127) ? 127 : (tmpdata < -128) ? -128 : tmpdata);
					}
				}
				break;
			case DET_FACENET:
				for (i = 0;i<net->input_size;i++)
				{
					tmpdata = (imageData.data[i]>>1);
					inbuf_reg[i] = tmpdata;
				}
				break;
			case DET_AML_FACE_DETECTION:
				memcpy(inbuf_det,imageData.data,net->input_size);
				break;
			case DET_AML_FACE_RECOGNITION:
			{
				int nn_width = 112, nn_height = 112, channels = 3;
				std::vector<Point2f> srcpoint;
				std::vector<Point2f> dstpoint;
				int i = 0;
				int zero_num = 0;
				Mat src = Mat(112,112,CV_8UC3);
				Mat rot = Mat::zeros(src.rows, src.cols, src.type());

				uint8_t *datasrc = (uint8_t *)imageData.data;

				for (i=0;i<5;i++)
				{
					if (imageData.inPoint[i].x > 1.0000001 || imageData.inPoint[i].y > 1.000001)
						zero_num++;
					if (imageData.inPoint[i].x > 1)
						imageData.inPoint[i].x = 1;
					if (imageData.inPoint[i].y > 1)
						imageData.inPoint[i].y = 1;
					if (imageData.inPoint[i].x < 0.001 && imageData.inPoint[i].y < 0.001)
						zero_num++;
				}

				if ((abs(imageData.inPoint[0].x - imageData.inPoint[1].x) < 0.001 ) && (abs(imageData.inPoint[0].x - imageData.inPoint[2].x) < 0.001) &&  (abs(imageData.inPoint[0].x - imageData.inPoint[3].x )< 0.001) && (abs(imageData.inPoint[0].x - imageData.inPoint[4].x) < 0.001))
				{
					zero_num++;
				}

				if (zero_num >= 1)
				{
					LOGW("give point value error,should not parse it!\n");
					memcpy(inbuf_reg,imageData.data,net->input_size);
					break;
				}
				Point2f lefteye(imageData.inPoint[0].x*112,imageData.inPoint[0].y*112);
				Point2f righteye(imageData.inPoint[1].x*112,imageData.inPoint[1].y*112);
				Point2f nose(imageData.inPoint[2].x*112,imageData.inPoint[2].y*112);
				Point2f leftmouse(imageData.inPoint[3].x*112,imageData.inPoint[3].y*112);
				Point2f rightmouse(imageData.inPoint[4].x*112,imageData.inPoint[4].y*112);

				memset(temp,0,sizeof(temp));

				if (net->input_ptr == NULL || imageData.data == NULL)
				{
					printf("prt is null\n\n");
					memcpy(inbuf_reg,imageData.data,net->input_size);
					break;
				}

				srcpoint.push_back(lefteye);
				srcpoint.push_back(righteye);
				srcpoint.push_back(nose);
				srcpoint.push_back(leftmouse);
				srcpoint.push_back(rightmouse);
				dstpoint.push_back(Point2f(38.2946,51.6963));
				dstpoint.push_back(Point2f(73.5318,51.5014));
				dstpoint.push_back(Point2f(56.0252,71.7366));
				dstpoint.push_back(Point2f(41.5493,92.3655));
				dstpoint.push_back(Point2f(70.7299,92.2));

				mul_mat = estimateRigidTransform(srcpoint,dstpoint,1);
				memcpy(temp,datasrc,112*112*3);
				src.data = temp;

				warpAffine(src, rot, mul_mat, src.size());
				//memcpy(imageData.data,rot.data,112*112*3);
				memcpy(inbuf_reg,rot.data,net->input_size);
			}
				break;
			case DET_YOLO_V2:
			case DET_YOLO_V3:
				det_get_model_size(modelType,&nn_width, &nn_height, &channels);
				for (i = 0; i < channels; i++)
				{
					offset = nn_width * nn_height * i;
					for (j = 0; j < nn_width * nn_height; j++)
					{
						tmpdata = (imageData.data[j * channels + i]>>1);
						inbuf_det[j + offset] = (uint8_t)((tmpdata >  127) ? 127 : (tmpdata < -128) ? -128 : tmpdata);
					}
				}
				break;
			case DET_AML_PERSON_DETECTION:
				det_get_model_size(modelType,&nn_width, &nn_height, &channels);
				for (i = 0; i < channels; i++)
				{
					offset = nn_width * nn_height * i;
					for (j = 0; j < nn_width * nn_height; j++)
					{
						tmpdata = (imageData.data[j * channels + i]>>1);
						inbuf_det[j + offset] = (uint8_t)((tmpdata >  127) ? 127 : (tmpdata < -128) ? -128 : tmpdata);
					}
				}
				break;
			case DET_AML_FACE_DETECT_RFB:
				det_get_model_size(modelType,&nn_width, &nn_height, &channels);
				for (i = 0; i < channels; i++)
				{
					offset = nn_width * nn_height *( channels -1 - i);
					for (j = 0; j < nn_width * nn_height; j++)
					{
						tmpdata = (imageData.data[j * channels + i]>>1);
						inbuf_det[j + offset] = (uint8_t)((tmpdata >  127) ? 127 : (tmpdata < -128) ? -128 : tmpdata);
					}
				}
				break;
			default:
				break;
		}
		net->process.flushTensorHandle(net->context,AML_INPUT_TENSOR);
	}

	if (ret)
	{
		LOGE("aml_module_input_set fail.");
		_SET_STATUS_(ret, DET_STATUS_SET_INPUT_ERROR, exit);
	}
	net->status = NETWORK_PREPARING;

exit:
	LOGP("Leave, modeltype:%d", modelType);
	return ret;
}

det_status_t det_set_param(det_model_type modelType, det_param_t param)
{
	LOGP("Enter, modeltype:%d", modelType);
	int ret = DET_STATUS_OK;
	int number = param.param.det_param.detect_num;
	LOGI("detect num is %d\n",number);

	p_det_network_t net = &network[modelType];
	if (!net->status) {
		LOGE("Model has not created! modeltype:%d", modelType);
		_SET_STATUS_(ret, DET_STATUS_ERROR, exit);
	}
	switch (modelType)
	{
	case DET_YOLOFACE_V2:
	case DET_YOLO_V2:
	case DET_YOLO_V3:
	case DET_AML_FACE_DETECTION:
	case DET_AML_FACE_DETECT_RFB:
	case DET_AML_PERSON_DETECTION:
		if (number < DETECT_NUM || number > MAX_DETECT_NUM)
		{
			ret = -1;
		}
		else
		{
			g_detect_number = number;
		}
		LOGI("g_detect_number is %d\n",g_detect_number);
		break;
	case DET_FACENET:
	case DET_AML_FACE_RECOGNITION:
		LOGP("modeltype:%d not set param", modelType);
		break;
	default:
		break;
	}

exit:
	LOGP("Leave, modeltype:%d", modelType);
	return ret;
}

det_status_t det_get_param(det_model_type modelType,det_param_t *param)
{
	LOGP("Enter, modeltype:%d", modelType);
	int ret = DET_STATUS_OK;

	p_det_network_t net = &network[modelType];
	if (!net->status) {
		LOGE("Model has not created! modeltype:%d", modelType);
		_SET_STATUS_(ret, DET_STATUS_ERROR, exit);
	}

	switch (modelType)
	{
	case DET_YOLOFACE_V2:
	case DET_YOLO_V2:
	case DET_YOLO_V3:
	case DET_AML_FACE_DETECTION:
	case DET_AML_FACE_DETECT_RFB:
	case DET_AML_PERSON_DETECTION:
		param->param.det_param.detect_num = g_detect_number;
		LOGI("g_detect_number is %d\n",g_detect_number);
		break;
	case DET_FACENET:
		param->param.reg_param.length = 128;
		param->param.reg_param.scale = 0.001954314;
		LOGI("modelType:%d, length is %d ,scale is %f\n",modelType, param->param.reg_param.length, param->param.reg_param.scale);
		break;
	case DET_AML_FACE_RECOGNITION:
		param->param.reg_param.length = 512;
		param->param.reg_param.scale = 0.00176058;
		LOGI("modelType:%d, length is %d ,scale is %f\n",modelType, param->param.reg_param.length, param->param.reg_param.scale);
		break;
	default:
		break;
	}

exit:
	LOGP("Leave, modeltype:%d", modelType);
	return ret;
}

aml_module_t get_sdk_modeltype(det_model_type modelType)
{
	aml_module_t type = FACE_LANDMARK_5;
	switch (modelType)
	{
	case DET_YOLOFACE_V2:
		type = YOLOFACE_V2;
		break;
	case DET_YOLO_V2:
		type = YOLO_V2;
		break;
	case DET_YOLO_V3:
		type = YOLO_V3;
		break;
	case DET_FACENET:
		type = FACE_NET;
		break;
	case DET_AML_FACE_RECOGNITION:
		type = FACE_RECOG_U;
		break;
	case DET_AML_FACE_DETECTION:
		type = FACE_LANDMARK_5;
		break;
	case DET_AML_FACE_DETECT_RFB:
		type = FACE_RFB_DETECTION;
		break;
	case DET_AML_PERSON_DETECTION:
		type = PERSON_DETECT;
		break;
	default:
		break;
	}
	return type;
}

void post_process(det_model_type modelType,void* out,pDetResult resultData)
{
	LOGP("Enter, post_process modelType:%d", modelType);

	face_landmark5_out_t           *face_detect_out            = NULL;
	person_detect_out_t            *person_detect_out          = NULL;
	yoloface_v2_out_t              *yoloface_v2_out            = NULL;
	yolov2_out_t                   *yolov2_out                 = NULL;
	yolov3_out_t                   *yolov3_out                 = NULL;
	facenet_out_t                  *facenet_out                = NULL;
	face_rfb_detect_out_t          *face_rfb_det_out           = NULL;
	face_recognize_uint_out_t      *face_recognize_uint_out    = NULL;

	uint8_t *buffer = NULL;
	float left = 0, right = 0, top = 0, bot=0;
	unsigned int i = 0, j = 0;

	switch (modelType)
	{
	case DET_YOLOFACE_V2:
		yoloface_v2_out = (yoloface_v2_out_t *)out;
		resultData->result.det_result.detect_num = yoloface_v2_out->detNum;
		for (i=0;i<yoloface_v2_out->detNum;i++)
		{
			if (i >= g_detect_number) break;
			left = yoloface_v2_out->pBox[i].x - 0.5 * yoloface_v2_out->pBox[i].w;
			top = yoloface_v2_out->pBox[i].y - 0.5 * yoloface_v2_out->pBox[i].h;
			right = yoloface_v2_out->pBox[i].x + 0.5 * yoloface_v2_out->pBox[i].w;
			bot = yoloface_v2_out->pBox[i].y + 0.5 * yoloface_v2_out->pBox[i].h;
			if (left < 0) left = 0;
			if (right > 1) right = 1.0;
			if (top < 0) top = 0;
			if (bot > 1) bot = 1.0;
			resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
			resultData->result.det_result.point[i].point.rectPoint.left = left;
			resultData->result.det_result.point[i].point.rectPoint.top = top;
			resultData->result.det_result.point[i].point.rectPoint.right = right;
			resultData->result.det_result.point[i].point.rectPoint.bottom = bot;
		}
		break;
	case DET_YOLO_V2:
		yolov2_out = (yolov2_out_t *)out;
		resultData->result.det_result.detect_num = yolov2_out->detNum;
		for (i=0;i<yolov2_out->detNum;i++)
		{
			if (i >= g_detect_number) break;
			left = yolov2_out->pBox[i].x;
			top = yolov2_out->pBox[i].y;
			right = yolov2_out->pBox[i].x + yolov2_out->pBox[i].w;
			bot = yolov2_out->pBox[i].y + yolov2_out->pBox[i].h;

			if (left < 0) left = 0;
			if (right > 1) right = 1.0;
			if (top < 0) top = 0;
			if (bot > 1) bot = 1.0;

			resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
			resultData->result.det_result.point[i].point.rectPoint.left = left;
			resultData->result.det_result.point[i].point.rectPoint.top = top;
			resultData->result.det_result.point[i].point.rectPoint.right = right;
			resultData->result.det_result.point[i].point.rectPoint.bottom = bot;
		}
		break;
	case DET_YOLO_V3:
		yolov3_out = (yolov3_out_t *)out;
		resultData->result.det_result.detect_num = yolov3_out->detNum;
		for (i=0;i<yolov3_out->detNum;i++)
		{
			if (i >= g_detect_number) break;
			left = yolov3_out->pBox[i].x;
			top = yolov3_out->pBox[i].y;
			right = yolov3_out->pBox[i].x + yolov3_out->pBox[i].w;
			bot = yolov3_out->pBox[i].y + yolov3_out->pBox[i].h;
			if (left < 0) left = 0;
			if (right > 1) right = 1.0;
			if (top < 0) top = 0;
			if (bot > 1) bot = 1.0;
			resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
			resultData->result.det_result.point[i].point.rectPoint.left = left;
			resultData->result.det_result.point[i].point.rectPoint.top = top;
			resultData->result.det_result.point[i].point.rectPoint.right = right;
			resultData->result.det_result.point[i].point.rectPoint.bottom = bot;
		}
		break;
	case DET_FACENET:
		facenet_out = (facenet_out_t *)out;
		buffer = resultData->result.reg_result.uint8;
		memcpy(buffer, facenet_out->faceVector, 128);
		break;
	case DET_AML_FACE_RECOGNITION:
		face_recognize_uint_out = (face_recognize_uint_out_t *)out;
		buffer = resultData->result.reg_result.uint8;
		memcpy(resultData->result.reg_result.uint8, face_recognize_uint_out->faceVector, 512);
		break;
	case DET_AML_FACE_DETECTION:
		face_detect_out = (face_landmark5_out_t *)out;
		resultData->result.det_result.detect_num = face_detect_out->detNum;
		for (i=0;i<face_detect_out->detNum;i++)
		{
			if (i >= g_detect_number) break;
			left = face_detect_out->facebox[i].x;
			top = face_detect_out->facebox[i].y;
			right = face_detect_out->facebox[i].x + 1.1875 * face_detect_out->facebox[i].w;
			bot = face_detect_out->facebox[i].y + face_detect_out->facebox[i].h;
			if (left < 0) left = 0;
			if (right > 1) right = 1.0;
			if (top < 0) top = 0;
			if (bot > 1) bot = 1.0;
			resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
			resultData->result.det_result.point[i].point.rectPoint.left = left;
			resultData->result.det_result.point[i].point.rectPoint.top = top;
			resultData->result.det_result.point[i].point.rectPoint.right = right;
			resultData->result.det_result.point[i].point.rectPoint.bottom = bot;

			for (j=0 ;j <5 ; j++)
			{
				resultData->result.det_result.point[i].tpts.floatX[j] = face_detect_out->pos[i][j].x;
				resultData->result.det_result.point[i].tpts.floatY[j] = face_detect_out->pos[i][j].y;
			}
		}
		break;
	case DET_AML_FACE_DETECT_RFB:
		face_rfb_det_out = (face_rfb_detect_out_t *)out;
		resultData->result.det_result.detect_num = face_rfb_det_out->detNum;
		for (i=0;i<face_rfb_det_out->detNum;i++)
		{
			if (i >= g_detect_number) break;
			left = face_rfb_det_out->facebox[i].x;
			top = face_rfb_det_out->facebox[i].y;
			right = face_rfb_det_out->facebox[i].x + face_rfb_det_out->facebox[i].w;
			bot = face_rfb_det_out->facebox[i].y + face_rfb_det_out->facebox[i].h;
			if (left < 0) left = 0;
			if (right > 1) right = 1.0;
			if (top < 0) top = 0;
			if (bot > 1) bot = 1.0;
			resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
			resultData->result.det_result.point[i].point.rectPoint.left = left;
			resultData->result.det_result.point[i].point.rectPoint.top = top;
			resultData->result.det_result.point[i].point.rectPoint.right = right;
			resultData->result.det_result.point[i].point.rectPoint.bottom = bot;

			for (j=0 ;j <5 ; j++)
			{
				resultData->result.det_result.point[i].tpts.floatX[j] = face_rfb_det_out->pos[i][j].x;
				resultData->result.det_result.point[i].tpts.floatY[j] = face_rfb_det_out->pos[i][j].y;
			}
		}
		break;
	case DET_AML_PERSON_DETECTION:
		person_detect_out = (person_detect_out_t *)out;
		resultData->result.det_result.detect_num = person_detect_out->detNum;
		for (i=0;i<person_detect_out->detNum;i++)
		{
			if (i >= g_detect_number) break;
			left = person_detect_out->pBox[i].x;
			top = person_detect_out->pBox[i].y;
			right = person_detect_out->pBox[i].x + person_detect_out->pBox[i].w;
			bot = person_detect_out->pBox[i].y + person_detect_out->pBox[i].h;
			if (left < 0) left = 0;
			if (right > 1) right = 1.0;
			if (top < 0) top = 0;
			if (bot > 1) bot = 1.0;
			resultData->result.det_result.point[i].type = DET_RECTANGLE_TYPE;
			resultData->result.det_result.point[i].point.rectPoint.left = left;
			resultData->result.det_result.point[i].point.rectPoint.top = top;
			resultData->result.det_result.point[i].point.rectPoint.right = right;
			resultData->result.det_result.point[i].point.rectPoint.bottom = bot;
		}
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
	if (modelType == DET_FACENET || modelType == DET_AML_FACE_RECOGNITION)
	{
		outconfig.format = AML_OUTDATA_RAW;
	}
	else
	{
		outconfig.format = AML_OUTDATA_FLOAT32;
	}

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

	if (modelType == DET_FACENET || modelType == DET_AML_FACE_RECOGNITION)
	{
		if (inbuf_reg)net->process.freeAlignedBuffer(inbuf_reg);
	}
	else
	{
		if (inbuf_det)net->process.freeAlignedBuffer(inbuf_det);
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

