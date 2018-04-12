# Copyright (c) Stephen D. Adams 2014,2017,2018 ALL RIGHTS RESERVED.
CC=gcc
C_FLAGS=-g -std=c11 -Wall -Wpedantic #-Werror
SOURCE_DIR=./src
DEST_DIR=./bin
EDITOR=editor
COMPILER=kc
OBJECTS_DIR=./.obj
MODULE_NAMES=\
	addr\
	assign\
	call\
	cond\
	conf\
	construct\
	construct_array\
	context\
	dec\
	def\
	deref\
	do\
	field\
	header\
	inc\
	lam\
	compiler\
	keyword\
	let\
	map\
	name\
	use\
	string_list\
	type\
	lex\
	parse\
	display

MODULE_SOURCES=$(addprefix $(SOURCE_DIR)/,$(addsuffix .c,$(MODULE_NAMES)))
OBJECTS=$(addprefix $(OBJECTS_DIR)/,$(addsuffix .o,$(MODULE_NAMES)))

all: $(DEST_DIR)/$(EDITOR) $(DEST_DIR)/$(COMPILER)

$(DEST_DIR):
	mkdir $(DEST_DIR)

$(DEST_DIR)/$(EDITOR): $(OBJECTS) | $(DEST_DIR)
	$(CC) $(C_FLAGS) -o $@ $(SOURCE_DIR)/editor.c $^ -lncurses -l:libreadline.so.6

$(DEST_DIR)/$(COMPILER): $(OBJECTS) | $(DEST_DIR)
	$(CC) $(C_FLAGS) -o $@ $(SOURCE_DIR)/compiler_main.c $^

$(OBJECTS_DIR)/%.o: $(SOURCE_DIR)/%.c | $(OBJECTS_DIR) $(DEST_DIR)
	$(CC) $(C_FLAGS) -c -o $@ $<

$(OBJECTS_DIR):
	mkdir $(OBJECTS_DIR)

tests: lex_test parse_test display_test ncurses_test

lex_test: src/lex.c tests/lex_test.c
	gcc $(CFLAGS) -o lex_test src/lex.c tests/lex_test.c

parse_test: src/lex.c src/parse.c tests/parse_test.c
	gcc $(CFLAGS) -o parse_test src/lex.c src/parse.c tests/parse_test.c

display_test: src/lex.c src/parse.c src/display.c tests/display_test.c
	gcc $(CFLAGS) -o display_test src/lex.c src/parse.c src/display.c tests/display_test.c

ncurses_test: tests/ncurses_test.c
	gcc -o ncurses_test tests/ncurses_test.c -lncurses

clean:
	#-rm lex_test parse_test display_test ncurses_test
	-rm -r $(OBJECTS_DIR) $(DEST_DIR)
