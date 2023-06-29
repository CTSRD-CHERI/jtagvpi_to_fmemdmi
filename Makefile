#CC = riscv64-unknown-elf-gcc
CC = gcc

all: jtagvpi_to_fmemdmi

jtagvpi_to_fmemdmi: main.o fmemdmi.o jtagvpi.o
	$(CC) $^ -o $@

%.o: %.c
	$(CC) -c $^ -o $@

.PHONY: clean mrproper

clean:
	rm -rf *.o

mrproper: clean
	rm -rf jtagvpi_to_fmemdmi
