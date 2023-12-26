# SPDX-License-Identifier: MIT
CC      = g++
PROF    = -O0 -g -rdynamic
C_FLAGS = -std=c++17 -Wall -Wextra -pedantic-errors -Wconversion \
          -Wno-unused-parameter -fmax-errors=5 -fno-exceptions
L_FLAGS = -lm -lstdc++ -lpthread
SRC_DIR = src
OUT_DIR = bin

SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OUT_FILES := $(patsubst $(SRC_DIR)/%.cpp,%,$(SRC_FILES))

all:
	@$(MAKE) verbose -s

verbose: $(OUT_FILES)
	@printf "\033[1;32mAll done! Examples are in the '%s' directory.\033[0m\n" \
	$(OUT_DIR)

PRINT_FMT1 = "\033[1m\033[31mCompiling \033[37m....\033[34m %-20s"
PRINT_FMT2 = "\t\033[33m%6s\033[31m lines\033[0m \n"
PRINT_FMT  = $(PRINT_FMT1)$(PRINT_FMT2)

$(OUT_FILES): %: $(SRC_DIR)/%.cpp
	@printf $(PRINT_FMT) $*.cpp "`wc -l $(SRC_DIR)/$*.cpp | cut -f1 -d' '`"
	$(CC) $< $(C_FLAGS) $(PROF) -o $(OUT_DIR)/$@ $(L_FLAGS)

clean:
	@printf "\033[1;36mCleaning \033[37m ...."
	@rm -f $(OUT_FILES)
	@printf "\033[1;37m Binaries cleaned!\033[0m\n"
