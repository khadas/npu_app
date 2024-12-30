#!/bin/sh

path=$(cd `dirname $0`;pwd)
cd $path
export NN_SAVE_OUTPUT=0
export NN_CHECK_OUTPUT=1
export NN_PROFILING=0
C3_SERIAL_NUM=ap22
T7C_SERIAL_NUM=360c
S5_SERIAL_NUM=3e0a
T3X_SERIAL_NUM=420a

C3_MODEL_DIR=./C3/
T7C_MODEL_DIR=./T7C/
S5_MODEL_DIR=./S5/
T3X_MODEL_DIR=./T3X/

if [ -f "/proc/cpu_chipid" ]; then
    CPU_SERIAL_NUM=$(cat /proc/cpu_chipid | awk -F: '/^Serial/{sub(/^[ \t]+/,"",$2);sub(/[ \t]+$/,"",$2);print substr($2,1,4)}')
    echo CPU_SERIAL_NUM=$CPU_SERIAL_NUM
elif [ -f "/proc/cpuinfo" ]; then
    CPU_SERIAL_NUM=$(cat /proc/cpuinfo | awk -F: '/^Serial/{sub(/^[ \t]+/,"",$2);sub(/[ \t]+$/,"",$2);print substr($2,1,4)}')
    echo CPU_SERIAL_NUM=$CPU_SERIAL_NUM
fi

if [ ! -n "$CPU_SERIAL_NUM" ]; then
    echo "Cant't get platform version"
    exit -1;
fi

chmod +x runtime

case "$CPU_SERIAL_NUM" in

$C3_SERIAL_NUM)
    export NN_INTERNAL_IO=0
    ./runtime ${C3_SERIAL_NUM}/mobilenet_v2_1.0_224_quant_u8.adla ${C3_SERIAL_NUM}/input_01.dat ${C3_SERIAL_NUM}/output_0.bin 1

    ./runtime ${C3_SERIAL_NUM}/mobilenet_v2_1.0_224_quant_u8.adla ${C3_SERIAL_NUM}/input_01.dat ${C3_SERIAL_NUM}/output_0.bin 1

    ./runtime ${C3_SERIAL_NUM}/mobilenet_v2_1.0_224_quant_u8.adla ${C3_SERIAL_NUM}/input_01.dat ${C3_SERIAL_NUM}/output_0.bin 1

    ./runtime ${C3_SERIAL_NUM}/mobilenet_v2_1.0_224_quant_u8.adla ${C3_SERIAL_NUM}/input_01.dat ${C3_SERIAL_NUM}/output_0.bin 1
;;

$T7C_SERIAL_NUM)
    export NN_INTERNAL_IO=0
    if [  -x "$(command -v file)" ]
    then
        ARCH=$(file runtime | grep arm64)
        if test -z  "$ARCH"
        then
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib
        else
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib64
        fi
    fi

    ./runtime ${T7C_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1

    ./runtime ${T7C_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1

    ./runtime ${T7C_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1

    ./runtime ${T7C_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1
;;

$S5_SERIAL_NUM)
    export NN_INTERNAL_IO=0
    if [  -x "$(command -v file)" ]
    then
        ARCH=$(file runtime | grep arm64)
        if test -z  "$ARCH"
        then
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib
        else
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib64
        fi
    fi

    ./runtime ${S5_MODEL_DIR}/resnetv2152/resnet_v2_152_quant_u8.adla ${S5_MODEL_DIR}/resnetv2152/input_0.bin ${S5_MODEL_DIR}/resnetv2152/output_0.bin 2

    ./runtime ${S5_MODEL_DIR}/resnetv2152/resnet_v2_152_quant_u8.adla ${S5_MODEL_DIR}/resnetv2152/input_0.bin ${S5_MODEL_DIR}/resnetv2152/output_0.bin 2

    ./runtime ${S5_MODEL_DIR}/resnetv2152/resnet_v2_152_quant_u8.adla ${S5_MODEL_DIR}/resnetv2152/input_0.bin ${S5_MODEL_DIR}/resnetv2152/output_0.bin 2

    ./runtime ${S5_MODEL_DIR}/resnetv2152/resnet_v2_152_quant_u8.adla ${S5_MODEL_DIR}/resnetv2152/input_0.bin ${S5_MODEL_DIR}/resnetv2152/output_0.bin 2
;;


$T3X_SERIAL_NUM)
    if [  -x "$(command -v file)" ]
    then
        ARCH=$(file runtime | grep arm64)
        if test -z  "$ARCH"
        then
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib
        else
            export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/vendor/lib64
        fi
    fi

    ./runtime ${T3X_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1

    ./runtime ${T3X_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1

    ./runtime ${T3X_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1

    ./runtime ${T3X_MODEL_DIR}/mobilenet_v2_1.0_224_quant_u8.adla input_01.dat output_0.bin 1
;;
*)

    echo "ERROR: Unknown CPU_SERIAL_NUM=$CPU_SERIAL_NUM, or not support so far"
    exit 1
;;

esac


