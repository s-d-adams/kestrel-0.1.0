SOURCES:=$(shell ls *.lc)
TESTS:=$(patsubst %.lc,%.x,$(SOURCES))
BASES:=$(patsubst %.lc,%,$(SOURCES))
C_FILES:=$(patsubst %.lc,%.c,$(SOURCES))
OBJECTS:=$(patsubst %.lc,%.o,$(SOURCES))

all: $(SOURCES) test_list.txt
	./build_all_tests.sh

test_list.txt: $(SOURCES)
	echo $(BASES) >$@

.PHONY: test
test: test_list.txt all
	./run_all_tests.sh

.PHONY: clean
clean:
	rm -f $(TESTS) $(OBJECTS) $(C_FILES) test_list.txt test_results.txt
