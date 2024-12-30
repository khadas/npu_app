/*
 * Copyright (C) 2021 Amlogic, Inc. All rights reserved.
 */

#pragma once

typedef enum ADLA_MODEL_TYPE
{
    ADLA_MODEL_TYPE_ADLA_LOADABLE = 0,
    ADLA_MODEL_TYPE_TENSORFLOW,
    ADLA_MODEL_TYPE_TENSORFLOW_LITE
} ADLA_MODEL_TYPE;

typedef enum ADLA_MODEL_IN_OUT_TYPE
{
    ADLA_MODEL_IN_OUT_TYPE_MEMORY = 0,
    ADLA_MODEL_IN_OUT_TYPE_FILE
} ADLA_MODEL_IN_OUT_TYPE;

typedef enum AdlaStatus
{
    AdlaStatus_Success              = 0,
    AdlaStatus_Failed               = 1, // general error, unknown
    AdlaStatus_OutOfMemory          = 2,
    AdlaStatus_OutOfDeviceMemory    = 3,
    AdlaStatus_BadParameter         = 4,
    AdlaStatud_DeviceLost           = 5,
} AdlaStatus;
