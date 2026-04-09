SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build

TARGET = $(BUILD_DIR)/transcoder

SRC := $(wildcard $(SRC_DIR)/*.c)
INC := $(addprefix -I, $(INC_DIR))
OBJ := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

CC 	   := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra
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

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(msg) "CC" $@
	$(Q)$(CC) $(CFLAGS) -c $< $(INC) -o $@

$(TARGET): $(OBJ)
	$(msg) "LD" $@
	$(Q)$(CC) $^ $(LDLIBS) -o $@

clean:
	$(msg) "CLEAN" $(BUILD_DIR)
	$(Q)rm -rf $(BUILD_DIR)