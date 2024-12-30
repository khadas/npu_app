
CROSS_COMPILE=aarch64-linux-gnu-

INCLUDE += -I./include -I$(SDK_LIB)/nnsdk/include
CFLAGS += $(INCLUDE)

################################################################################
# Supply necessary libraries.
LIBS += -lm -ldl

CFLAGS +=-I. -O2 -fPIE -Wall -pthread $(LIBS)
CXXFLAGS += $(CFLAGS) -lstdc++
ODIR=obj
OFILE=libnn_audio.so

_OBJ= aiaudio.o
_OBJ += aiaudio_log.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR)/%.o: %.cpp
	$(CC) -c -o $@ $< $(CXXFLAGS)

$(OFILE): $(OBJ)
	#$(CC) -shared -o $(OFILE) ./obj/*.o
	$(CC) $(CFLAGS) -shared -o $(OFILE) ./obj/*.o

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(OFILE)
