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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "detect_log.h"

det_debug_level_t g_log_level = DET_DEBUG_LEVEL_ERROR;

void det_set_log_level(det_debug_level_t level,det_log_format_t output_format)
{
    (void) output_format;
    char* log_level = getenv("DETECT_LOG_LEVEL");

    if (log_level) {
        g_log_level = (det_debug_level_t)atoi(log_level);
        LOGW("Set log level by environmental variable.level=%d",g_log_level);
    } else {
        g_log_level = level;
        LOGW("Set log level=%d",g_log_level);
    }
    LOGW("output_format not support Imperfect, default to DET_LOG_TERMINAL");

    char* vsi_log = getenv("VSI_NN_LOG_LEVEL");
    if (!vsi_log) {
        setenv("VSI_NN_LOG_LEVEL", "1", 0);
        LOGW("Not exist VSI_NN_LOG_LEVEL, Setenv set_vsi_log_error_level");
    }
    return;
}

static int _check_log_level(det_debug_level_t level)
{
    det_debug_level_t env_log_level;
    char* log_level = getenv("DETECT_LOG_LEVEL");

    if (log_level) {
        env_log_level = (det_debug_level_t)atoi(log_level);
        if (env_log_level != g_log_level) {
            g_log_level = env_log_level;
            LOGW("Set log level by environmental variable..level=%d",g_log_level);
        }
    }

    if (g_log_level >= level) {
        return 1;
    }
    return 0;
}

void detect_nn_LogMsg(det_debug_level_t level, const char *fmt, ...)
{
    char arg_buffer[DEBUG_BUFFER_LEN] = {0};
    va_list arg;

    if (_check_log_level(level) == 0) {
        return ;
    }

    va_start(arg, fmt);
    vsnprintf(arg_buffer, DEBUG_BUFFER_LEN, fmt, arg);
    va_end(arg);

    fprintf(stderr, "%s\n", arg_buffer);
} /* detect_nn_LogMsg() */
