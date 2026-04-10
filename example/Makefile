SRC := main.c
INC := $(addprefix -I, ../include/)

TARGET := xcoder-opus
LIB    := ../build/libxcoder-opus.a

CC 	    := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -pedantic -O2
LDFLAGS := -L../build
LDLIBS  := -lxcoder-opus -lavformat -lavcodec -lavutil -lswresample

all: $(TARGET)

$(LIB):
	$(MAKE) -C ..

$(TARGET): $(SRC) $(LIB)
	$(CC) $(CFLAGS) $^ $(INC) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -rf $(TARGET)
	$(MAKE) -C .. clean

.PHONY: all clean



