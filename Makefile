SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build

TARGET = $(BUILD_DIR)/xcoder-opus

SRC := $(wildcard $(SRC_DIR)/*.c)
INC := $(addprefix -I, $(INC_DIR))
OBJ := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

CC := gcc
override FLAGS 		   += -std=c11 -Wall -Wextra -Werror -pedantic
override FLAGS_DEBUG   += $(FLAGS) -fsanitize=address -g
override FLAGS_RELEASE += $(FLAGS) -O2

CFLAGS := $(FLAGS_RELEASE)

ifeq ($(MAKECMDGOALS), debug)
	CFLAGS := $(FLAGS_DEBUG)
endif

LDLIBS := -lavformat -lavcodec -lavutil -lswresample

V ?= 0
ifeq ($(V),1)
    Q   =
    msg = @true
else
    Q = @
    msg = @printf "  %-8s %s\n"
endif

all: $(TARGET)

.PHONY: debug all clean

debug: FLAGS = $(FLAGS_DEBUG)
debug: all

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(msg) "CC" $@
	$(Q)$(CC) $(CFLAGS) -c $< $(INC) -o $@

$(TARGET): $(OBJ)
	$(msg) "LD" $@
	$(Q)$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(msg) "CLEAN" $(BUILD_DIR)
	$(Q)rm -rf $(BUILD_DIR)