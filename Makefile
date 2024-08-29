CC = cc
CFLAGS = -fsanitize=address -D VERBOSE
TESTS = $(wildcard tests/*.lc)

lc: lc.c
	$(CC) -g -O0 -std=c89 -o $@ $^ $(CFLAGS)

.PHONY: clean
clean:
	rm -f lc lc1 lc2 *.s *.o out toks.c.inc tests/*.o tests/*.test

.PHONY: regentokens
regentokens:
	python3 gentok.py > toks.c.inc
	sh updatetoks.sh
	clang-format -i lc.c

.PHONY: tests
tests: $(TESTS:.lc=.test) lc

.PHONY: test
test: tests lc
	bash test.sh

tests/%.test: tests/%.lc lc
	cat "$<" | ./lc \
		&& as -g -s --32 -o "$@.o" out.s \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o || exit 0

lc1: lc lc.c
	cat lc.c | ./lc \
		&& as -g -s --32 -o "$@.o" out.s \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o
	mv -v out.s out.s.lc

lc2: lc1 lc lc.c
	cat lc.c | ./lc1 \
		&& as -g -s --32 -o "$@.o" out.s \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o
	mv -v out.s out.s.lc1
