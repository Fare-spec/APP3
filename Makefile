CC := clang

SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin

CFLAGS += -g -Wall -Wextra -Werror -Wno-unused-parameter -gdwarf-4 -Wno-unused-result

OBJS := $(addprefix $(OBJ_DIR)/,arbres.o arbresphylo.o listes.o queue.o table.o common_tests.o)

ACTES := acte1 acte2 acte3 acte3b acte4

.PHONY: all main construire hero dot $(ACTES) tests_acte1 tests_acte2 tests_acte3 tests_acte3b tests_acte4 tests_acte5 clean clear

all: main construire hero dot $(ACTES)

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

main: $(BIN_DIR)/main

$(BIN_DIR)/main: $(OBJ_DIR)/main.o \
                 $(OBJ_DIR)/arbresphylo.o \
                 $(OBJ_DIR)/arbres.o \
                 $(OBJ_DIR)/listes.o \
                 $(OBJ_DIR)/queue.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

construire: $(BIN_DIR)/construire

$(BIN_DIR)/construire: $(OBJ_DIR)/construire.o $(OBJ_DIR)/table.o \
                 $(OBJ_DIR)/arbresphylo.o $(OBJ_DIR)/arbres.o \
                 $(OBJ_DIR)/listes.o $(OBJ_DIR)/queue.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

dot: $(BIN_DIR)/dot

$(BIN_DIR)/dot: $(OBJ_DIR)/dot.o $(OBJ_DIR)/arbres.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

hero: $(BIN_DIR)/hero

$(BIN_DIR)/hero: $(OBJ_DIR)/hero.o $(OBJ_DIR)/client.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(ACTES): %: $(BIN_DIR)/%

$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

tests_acte1: acte1
	@set -e; for f in $$(cat tests_acte1/liste.txt); do echo Test: $$f; $(BIN_DIR)/acte1 tests_acte1/$$f; done; printf "\033[0;32mSuccès\033[0m sur l'acte I !\n"

tests_acte2: acte2
	@set -e; for f in $$(cat tests_acte2/liste.txt); do echo Test: $$f; $(BIN_DIR)/acte2 tests_acte2/$$f; done; printf "\033[0;32mSuccès\033[0m sur l'acte II !\n"

tests_acte3: acte3
	@set -e; for f in $$(cat tests_acte3/liste.txt); do echo Test: $$f; $(BIN_DIR)/acte3 tests_acte3/$$f; done; printf "\033[0;32mSuccès\033[0m sur l'acte III-A !\n"

tests_acte3b: acte3b
	@set -e; for f in $$(cat tests_acte3b/liste.txt); do echo Test: $$f; $(BIN_DIR)/acte3b tests_acte3b/$$f; done; printf "\033[0;32mSuccès\033[0m sur l'acte III-B !\n"

tests_acte4: acte4
	@set -e; for f in $$(cat tests_acte4/liste.txt); do echo Test: $$f; $(BIN_DIR)/acte4 tests_acte4/$$f; done; printf "\033[0;32mSuccès\033[0m sur l'acte IV !\n"

tests_acte5: construire
	@set -e; for f in tests_acte5/ok_*.table; do echo Test: $$f; $(BIN_DIR)/construire $$f > /dev/null; done; for f in tests_acte5/impossible_*.table; do echo Test: $$f; if $(BIN_DIR)/construire $$f > /dev/null 2>&1; then echo "devait échouer : $$f"; exit 1; fi; done; printf "\033[0;32mSuccès\033[0m sur l'acte V !\n"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

clear: clean
