CC = cc
CFLAGS = -fsanitize=address -D VERBOSE
TESTS = $(wildcard tests/*.lc)

.PHONY: clean
clean:
	rm -f lc0 lc1 lc *.s *.o out out.s out.s.* toks.c.inc tests/*.o tests/*.test

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

lc1: lc0
	rm -f out.s
	cat lc.c | ./lc0
	mv -v out.s out.s.lc0
	as -g -s --32 -o "$@.o" out.s.lc0 \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o

lc: lc1
	rm -f out.s
	cat lc.c | ./lc1
	mv -v out.s out.s.lc1
	as -g -s --32 -o "$@.o" out.s.lc1 \
		&& ld -m elf_i386 -o "$@" -dynamic-linker /lib/ld-linux.so.2 \
			/usr/lib32/crt1.o /usr/lib32/crti.o "$@.o" -lc /usr/lib32/crtn.o
	diff out.s.lc0 out.s.lc1
