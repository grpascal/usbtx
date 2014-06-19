
TARGET := tools

MPTOOLS := ../../mptools

O ?= ..

VPATH	= src
IPATH   += ../libmxuvc/include
IPATH	+= $(MPTOOLS)/libctools/src
IPATH	+= $(MPTOOLS)/libcmsg/src


include $(MPTOOLS)/Makefile.common

EXTRA_LDFLAGS	+= -L$(MPTOOLS)/$(BUILDNAME)/lib \
	-Wl,-rpath,${shell readlink -f $(MPTOOLS)/$(BUILDNAME)/lib}
EXTRA_LDFLAGS += -lmxuvc ${shell pkg-config --libs alsa}

TOOL_LIST	+= $(BIN)/capture_mux
TOOL_LIST	+= $(BIN)/capture_audmux
TOOL_LIST	+= $(BIN)/capture_avmux
TOOL_LIST	+= $(BIN)/capture_raw


all : ${OBJ} $(TOOL_LIST)

$(BIN)/capture_mux : EXTRA_LDFLAGS += -lctools -lpthread
$(BIN)/capture_mux : ${OBJ}/capture_mux.o
$(BIN)/capture_audmux : ${OBJ}/capture_audmux.o
$(BIN)/capture_avmux : ${OBJ}/capture_avmux.o
$(BIN)/capture_raw : ${OBJ}/capture_raw.o

install : all
	cp $(TOOL_LIST) $(INSTALLDIR)/bin/
