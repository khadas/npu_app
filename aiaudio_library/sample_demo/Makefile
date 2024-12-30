
CROSS_COMPILE=$(TARGET_KERNEL_CROSS)

CFLAGS += $(INCLUDE)

################################################################################
# Supply necessary libraries.
LIBS += -l nnsdk -l m
LIBS += -l stdc++

CFLAGS +=-I. -g -fPIE -Wall -pthread $(LIBS)
ODIR = obj
OFILE = aiaudio_test

_OBJ = main.o
_OBJ += WAVE.o
_OBJ += wav2mfcc.o
_OBJ += util.o
_OBJ += sigProcess.o
_OBJ += postprocess_classify.o
_OBJ += mfcc.o
_OBJ += hmath.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(OFILE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -pie

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(OFILE)
