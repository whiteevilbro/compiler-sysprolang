CLANG = clang
MAIN_FLAGS = -std=c23 -g -O0
WARNINGS_FLAGS = -Wall -Wextra -Wpedantic
SANITIZER_FLAGS = -fsanitize=undefined
FLAGS = $(MAIN_FLAGS) $(WARNINGS_FLAGS) $(SANITIZER_FLAGS) 

C_SOURCES = $(shell find -L ./$(SOURCE_DIR) -iname "*.c")
C_HEADERS = $(shell find -L ./$(SOURCE_DIR) -iname "*.h")
HEADER_FLAGS = $(addprefix -I, $(call uniq,$(dir $(C_HEADERS))))

EXE = splc

all: clean build

$(EXE_64): $(FORMATTED_FILES)
	@rm -f $(FORMATTED_FILES)
	$(CLANG) $(FLAGS) $(SOURCES) -o $@ -m64

build : build/$(EXE)
build/$(EXE): $(C_SOURCES) $(C_HEADERS)
	mkdir -p build
	$(CLANG) $(MAIN_FLAGS) $(WARNINGS_FLAGS) $(SANITIZER_FLAGS) $(HEADER_FLAGS) $(C_SOURCES) -o $@

run: build/$(EXE)
	./build/$(EXE)

clean:
	rm -rf build/*

.PHONY: all build run clean