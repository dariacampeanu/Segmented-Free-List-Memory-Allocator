# Copyright Campeanu Daria-Maria

# compiler setup
CC=gcc
CFLAGS=-Wall -Wextra -std=c99 -g

# define targets
TARGETS = sfl

build: $(TARGETS)

sfl: sfl.c
	$(CC) $(CFLAGS) sfl.c -o sfl

run_sfl:
	./sfl

pack:
	zip -FSr 314CA_CampeanuDariaMaria_Tema1.zip README Makefile *.c *.h
clean:
	rm -f $(TARGETS) 

.PHONY: pack clean