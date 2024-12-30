
CROSS_COMPILE=$(TARGET_KERNEL_CROSS)
CCC = $(CROSS_COMPILE)gcc
CCXX = $(CROSS_COMPILE)g++

STATIC_LINK=shared
#STATIC_LINK=static

INCLUDE += -I./
INCLUDE += -I../mbi-1.0/mbi_base/include
CXXFLAGS = -O2 -g $(INCLUDE)
LIBS +=-L./ -L../mbi-1.0/mbi_base/ -L$(TARGET_DIR)/usr/lib/


ifeq ($(STATIC_LINK), static)
	CXXFLAGS += -static
	LDFLAGS += -static
	LIBS += -lstdc++ -lc -lgcc
	STATIC_LIBS += -static-libgcc -ladla
	DYNAMIC_LIBS +=  -lpthread -lm
	LDFLAGS += $(LIBS) -Wl,--start-group -Wl,--whole-archive $(STATIC_LIBS) -Wl,--no-whole-archive  -Wl,--end-group -Wl,-Bdynamic $(DYNAMIC_LIBS)
else
#   CXXFLAGS += -fPIC
#   CFLAGS += -fPIC
	LIBS += -lgcc
	DYNAMIC_LIBS += -lpthread -ldl -ladla -lmbi
#	STATIC_LIBS +=
	LDFLAGS += $(LIBS) -Wl,-Bstatic $(STATIC_LIBS) -Wl,-Bdynamic $(DYNAMIC_LIBS)
endif

ODIR=obj
OFILE=runtime

_OBJ=adla_runtime.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c
	$(CCC) -c -o $@ $< $(CXXFLAGS)

$(ODIR)/%.o: %.cpp
	$(CCXX) -c -o $@ $< $(CXXFLAGS)

$(OFILE): $(OBJ)
	$(CCXX) -o $@ $^ $(LDFLAGS)
#-pie
.PHONY: clean

clean:
	rm -f $(ODIR)/*.o $(OFILE)

