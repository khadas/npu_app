#! /bin/sh
path=$(cd `dirname $0`;pwd)
cd $path
if [ ! -f "dncnn" ] || [ ! -f "DnCNN.export.data" ] || [ ! -f "inp_2.tensor" ] || [ ! -f "output.bin" ]; then
    echo
    echo "The file needed to run is missing from the current folder, Please check it!"
    echo
    exit
fi
export VSI_NN_LOG_LEVEL=0
export VNN_NOT_PRINT_TIME=0
export VNN_LOOP_TIME=10
export VIV_VX_VERIFYSPEED_OPT_MODE=1
./dncnn DnCNN.export.data inp_2.tensor
