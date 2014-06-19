
MPTOOLS := ../../mptools


TARGET			= libmxuvc
DESC			= MX UVC Library

IPATH			+= include

# Auto load all the .c files dependencies, and object files
LIBSRC			:= ${notdir ${wildcard src/*.c}}
VPATH 			+= src
IPATH 			+= src

LIBSRC			+= ${notdir ${wildcard src/common/*.c}}
VPATH 			+= src/common
IPATH 			+= src/common

LIBSRC			+= ${notdir ${wildcard src/common/libskypeecxu/*.c}}
VPATH			+= src/common/libskypeecxu
IPATH			+= src/common/libskypeecxu

LIBSRC			+= ${notdir ${wildcard src/video/v4l2/*.c}}
VPATH 			+= src/video/v4l2
IPATH 			+= src/video/v4l2
-include src/video/v4l2/ld.mk

LIBSRC			+= ${notdir ${wildcard src/audio/alsa/*.c}}
VPATH 			+= src/audio/alsa
IPATH 			+= src/audio/alsa
-include src/audio/alsa/ld.mk


include $(MPTOOLS)/Makefile.common

all : $(OBJ) $(LIB)/$(TARGET).a \
	shared

shared: ${LIB}/$(TARGET).so.$(SOV)

LIBOBJ			:= ${patsubst %, ${OBJ}/%, ${notdir ${LIBSRC:.c=.o}}}

$(LIB)/$(TARGET).a : $(LIBOBJ)
$(LIB)/$(TARGET).so.$(SOV) : $(LIBOBJ)

clean: localclean
localclean:
	rm -rf $(LIBOBJ) $(LIB)/$(TARGET)* $(BIN)/cnode*
