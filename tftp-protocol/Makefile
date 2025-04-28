CC = gcc
CFLAGS = -Iinclude -Wall -g

S_DIR = server
C_DIR = client
U_DIR = utils
BUILD_DIR = build

 SRC = $(wildcard $(S_DIR)/*.c) $(wildcard $(C_DIR)/*.c) $(wildcard $(U_DIR)/*.c) main.c
OBJ = $(SRC:.c=.o)

TARGET = tftp

 $(shell mkdir -p $(BUILD_DIR))

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(addprefix $(BUILD_DIR)/,$(notdir $^))

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $(BUILD_DIR)/$(notdir $@)

clean:
	rm -f $(BUILD_DIR)/*.o $(TARGET)

.PHONY: all clean