#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <math.h>
#include <vector>
#include <iostream>
#include <math.h>
#include <array>
#include <time.h>
#include <ctime>

using namespace std;

// # Global parameters #
static const int MAP_SIZE = 12;
static const int N_OF_ITERATIONS = 5;
// # End of global parameters

struct ExecutionEnviornment {
	int numberOfProcesses;
	int idOfCurrentProcess;
};

enum Tags {
	BROADCAST,
	CALCULATION,
	TERMINATE
};

// Basic methods
void determineEnviornment(ExecutionEnviornment &excEnv);
void masterWork(ExecutionEnviornment excEnv);
void slaveWork(ExecutionEnviornment excEnv);
// Map related methods
short* generateMap();
short** initMap(int, int);
void fillMap(short** (&map));
short* toVector(short** map);
void freeMap(short** map, int width);
void updateMap(short* mapAsVector, short** storedResults, int nOfProcesses, int workloadPerProcess);
void printMap(short* mapAsVector);
// Calculations
int countnNeighbours(short*, int);


int main(int argc, char *argv[]) {
	MPI_Init(&argc, &argv);
	ExecutionEnviornment excEnv;
	determineEnviornment(excEnv);

	if(excEnv.idOfCurrentProcess == 0) {
		masterWork(excEnv);
	} else {
		slaveWork(excEnv);
	}

	 MPI_Finalize();
}

void determineEnviornment(ExecutionEnviornment &excEnv) {
	MPI_Comm_size(MPI_COMM_WORLD, &excEnv.numberOfProcesses);
	MPI_Comm_rank(MPI_COMM_WORLD, &excEnv.idOfCurrentProcess);
}

void masterWork(ExecutionEnviornment excEnv) {
	short* mapAsVector = generateMap();
	cout << "[Master] :: map generated" << endl;
	int fullSize = MAP_SIZE * MAP_SIZE;
	MPI_Status status;
	int workload = fullSize / (excEnv.numberOfProcesses - 1);
	cout << "Workload : " << workload << endl;
	short** storedResults = initMap(excEnv.numberOfProcesses, workload);
	cout << "[Master] :: data generated" << endl;
	// printMap(mapAsVector);

	for(int i = 0; i < N_OF_ITERATIONS; i++) {
		cout << "----- Iteration " << i << " -----" << endl;
		for (int p = 1; p < excEnv.numberOfProcesses; p++) {
			MPI_Send(mapAsVector, fullSize, MPI_SHORT, p, Tags::BROADCAST, MPI_COMM_WORLD);
			cout << "[Master] :: sent REQUEST to " << p << endl;
		}

		for(int p = 1; p < excEnv.numberOfProcesses; p++) {
			MPI_Recv(storedResults[p - 1], workload, MPI_SHORT, p, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			cout << "[Master] :: got RESPONSE from " << p << endl;
		}

		// updateMap(mapAsVector, storedResults, excEnv.numberOfProcesses, workload);
		// printMap(mapAsVector);
	}

	// Terminate slaves
	for(int p = 1; p < excEnv.numberOfProcesses; p++) {
		MPI_Send(0, 0, MPI_SHORT, p, Tags::TERMINATE, MPI_COMM_WORLD);
		cout << "[Master] :: sent TERMINATION SIGNAL to " << p << endl;
	}
}

void slaveWork(ExecutionEnviornment excEnv) {
	short* mapAsVector = (short*) malloc(MAP_SIZE * MAP_SIZE * sizeof(short));
	int fullSize = MAP_SIZE * MAP_SIZE;
	MPI_Request recivedRequest;
	MPI_Request sentRequest;
	MPI_Status status;

	while(1) {
		cout << "[" << excEnv.idOfCurrentProcess << "] :: " << "WAITING." << endl;
		int resultCode = MPI_Recv(&mapAsVector, fullSize, MPI_SHORT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		cout <<  "[" << excEnv.idOfCurrentProcess << "] :: " << "got REQUEST." << endl;

		if(status.MPI_TAG == Tags::TERMINATE) {
			return;
		}

		int workPerThread = fullSize / (excEnv.numberOfProcesses - 1); // Master is not working.
		short* result = (short*) malloc(workPerThread * sizeof(short));
		int relativePointer = 0;
		int nbhs;
		for(int i = (excEnv.idOfCurrentProcess - 1) * workPerThread; i < (excEnv.idOfCurrentProcess) * workPerThread; ++i) {
			nbhs = countnNeighbours(mapAsVector, i);
			if(nbhs == 1 || nbhs == 2) result[relativePointer] = 1;
			else result[relativePointer] = 0;
			++relativePointer;
		}

		// Sending results back to master
		MPI_Send(result, workPerThread, MPI_SHORT, 0, Tags::CALCULATION, MPI_COMM_WORLD);
		cout << "[" << excEnv.idOfCurrentProcess << "] :: " << "sent data." << endl;

		free(result);
	}
}

int countnNeighbours(short* mapAsVector, int currentPosition) {
	int y = currentPosition / MAP_SIZE;
	int x = currentPosition % MAP_SIZE;
	int count = 0;
	for(int local_x = x - 1; local_x <= x + 1; local_x++) {
		for(int local_y = y - 1; local_y <= y + 1; local_y++) {
			if(local_x == -1 || local_y == -1 || local_x == MAP_SIZE || local_y == MAP_SIZE) {
				continue;
			}

			if(mapAsVector[y * MAP_SIZE + x] == 1) count++;
		}
	}

	return count;
}

short* generateMap() {
	short** map = initMap(MAP_SIZE, MAP_SIZE);
	fillMap(map);
	short* vector = toVector(map);
	freeMap(map, MAP_SIZE);
	return vector;
}

short** initMap(int width, int height) {
	short** map = (short**) malloc(width * sizeof(short*));
	for (int i = 0; i < width; ++i)
		map[i] = (short*) malloc(height * sizeof(short));

	return map;
}

void fillMap(short** (&map)) {
	for(int i = 0; i < MAP_SIZE; i++) {
		for(int j = 0; j < MAP_SIZE; j++) {
			if(i == j || MAP_SIZE - i - 1 == j) {
				map[i][j] = 1;
			} else {
				map[i][j] = 0;
			}
		}
	}
}

short* toVector(short** map) {
	short* vector = (short*) malloc(MAP_SIZE * MAP_SIZE * sizeof(short));
	int idx = 0;
	for(int i = 0; i < MAP_SIZE; i++) {
		for(int j = 0; j < MAP_SIZE; j++) {
			vector[idx++] = map[i][j];
		}
	}

	return vector;
}

void freeMap(short** map, int width) {
	for(int i = 0; i < width; i++) {
		free(map[i]);
	}

	free(map);
}

void updateMap(short* mapAsVector, short** storedResults, int nOfProcesses, int workloadPerProcess) {
	int globalPointer = 0;
	for(int p = 0; p < nOfProcesses - 1; p++) { // We exclude master
		for(int localPointer = 0; localPointer < workloadPerProcess; localPointer++) {
			mapAsVector[globalPointer] = storedResults[p][localPointer];
			globalPointer++;
		}
	}
}

void printMap(short* mapAsVector) {
	for(int x = 0; x < MAP_SIZE; x++) {
		cout << endl;
		for(int y = 0; y < MAP_SIZE; y++) {
			cout << mapAsVector[x * MAP_SIZE + y];
		}
	}
	cout << endl;
}
