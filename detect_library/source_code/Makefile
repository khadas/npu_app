
CROSS_COMPILE=aarch64-linux-gnu-

OPENCV_LIB=../../build/opencv3-3.4.9
LIB_DIR=$(CC)/libc/lib
INCLUDE += -I./include -I$(SDK_LIB)/nnsdk/include -I$(OPENCV_LIB)/modules/video/include -I$(OPENCV_LIB)/include  -I$(OPENCV_LIB)/modules/imgproc/include -I$(OPENCV_LIB)/modules/core/include  -I$(OPENCV_LIB)/modules/core/include  -I$(OPENCV_LIB)/modules/highgui/include -I$(OPENCV_LIB)/modules/imgcodecs/include  -I$(OPENCV_LIB)/modules/videoio/include
CFLAGS += $(INCLUDE)

################################################################################
# Supply necessary libraries.
LIBS +=-L$(LIB_DIR) -l stdc++  -l m
LIBS += -L$(LIB_DIR) -lm -ldl
LIBS +=-L$(OPENCV_LIB)/buildroot-build/lib/ -lopencv_imgproc -lopencv_core  -lopencv_video -l opencv_highgui -lopencv_imgcodecs -lopencv_videoio

CFLAGS +=-I. -O2 -fPIE -Wall -pthread $(LIBS)
ODIR=obj
OFILE=libnn_detect.so

_OBJ= detect.o
_OBJ += detect_log.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(OFILE): $(OBJ)
	#$(CC) -shared -o $(OFILE) ./obj/*.o
	$(CC) $(CFLAGS) -shared -o $(OFILE) ./obj/*.o

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(OFILE)
