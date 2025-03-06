build:
	mpicc -o bittorent_simulator bittorent_simulator.c -pthread -Wall

clean:
	rm -rf bittorent_simulator
