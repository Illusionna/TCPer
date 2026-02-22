.PHONY: clean

CC = gcc
TARGET = main

SRC = $(call rwildcard, ./, %.c)
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

ifeq ($(OS), Windows_NT)
	REMOVE = cmd /c del
	TARGET := $(TARGET).exe
	SRC := $(subst /,\,$(SRC))
	PARAMS = -s -O2 -flto
	LIBRARY = -lws2_32 -lm
else
	REMOVE = rm -f
	TARGET := $(TARGET)
	SRC := $(subst \,/,$(SRC))
	PARAMS = -O2 -flto
	LIBRARY = -pthread -lm
endif

compilation = $(SRC:.c=.o)

$(TARGET): $(compilation)
	$(CC) $(compilation) -o $(TARGET) $(PARAMS) $(LIBRARY)

$(compilation): %.o: %.c
	$(CC) -c $< -o $@ $(PARAMS)

clean:
	$(REMOVE) $(compilation)
	$(REMOVE) $(TARGET)
