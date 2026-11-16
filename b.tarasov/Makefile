CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2

all: task1 task2 task3 task4 task5 task6 task7

.PHONY: all task1 task2 task3 task4 task5 task6 task7 clean

task1: task1/program
task2: task2/program
task3: task3/program
task4: task4/program
task5: task5/program
task6: task6/program
task7: task7/program

task1/program: task1/main.c
	$(CC) $(CFLAGS) task1/main.c -o task1/program

task2/program: task2/main.c
	$(CC) $(CFLAGS) task2/main.c -o task2/program

task3/program: task3/main.c
	$(CC) $(CFLAGS) task3/main.c -o task3/program

task4/program: task4/main.c
	$(CC) $(CFLAGS) task4/main.c -o task4/program

task5/program: task5/main.c
	$(CC) $(CFLAGS) -DMODE=5 task5/main.c -o task5/program

task6/program: task6/main.c
	$(CC) $(CFLAGS) -DMODE=6 task6/main.c -o task6/program

task7/program: task7/main.c
	$(CC) $(CFLAGS) -DMODE=7 task7/main.c -o task7/program

clean:
	rm -f task1/program task2/program task3/program task4/program task5/program task6/program task7/program
