CC = cc
CFLAGS = -fsanitize=address
TESTS = $(wildcard tests/*.lc)

.EXPORT_ALL_VARIABLES:
ASAN_OPTIONS=detect_leaks=0

default: lc

.PHONY: clean
clean:
	rm -f lc0 lc1 lc *.s *.o toks.c.inc tests/*.o tests/*.test

.PHONY: regentokens
regentokens:
	python3 gentok.py > toks.c.inc
	sh updatetoks.sh
	clang-format -i lc.c

.PHONY: tests
tests: $(TESTS:.lc=.test) lc0

.PHONY: test
test: tests lc0
	bash test.sh

tests/%.test: tests/%.lc lc0
	cat "$<" | ./lc0 \
		&& as -g -s --32 -o "$@.o" out.s \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o

lc0: lc.c
	$(CC) -g -O0 -std=c89 -o $@ $^ $(CFLAGS)

lc1.s: lc0
	cat lc.c | ./lc0
	mv out.s lc1.s

lc1: lc1.s
	as -g -s --32 -o "$@.o" "$<" \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o

lc.s: lc1
	cat lc.c | ./lc1
	mv out.s lc.s

lc: lc.s
	as -g -s --32 -o "$@.o" "$<" \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o
	diff -ruN lc1.s lc.s
