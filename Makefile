DESTDIR=/opt/bsiuk/usbtx

CMP += libmxuvc
CMP += capture-mux
#CMP += txui

all:
	for dir in $(CMP); do \
		make -C $$dir ; \
	done

O ?= .
ARCH			:= $(shell $(CC) -dumpmachine)
BUILDNAME		:= build-$(ARCH)
BUILD			:= $(O)/$(BUILDNAME)
OBJ				= $(BUILD)/obj/$(TARGET)
BIN				= $(BUILD)/bin

# overriden on the make command line
DESTDIR = /tmp/usbtx

install:
	mkdir -p $(DESTDIR)/bin $(DESTDIR)/lib
	cp $(BIN)/* $(DESTDIR)/bin/
	cp -a $(BUILD)/lib/* $(DESTDIR)/lib/

clean:
	rm -rf $(BUILD)
