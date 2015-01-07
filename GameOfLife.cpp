#include <mpi.h>
#include <iostream>
#include <chrono>

using namespace std;

// # Global parameters #
static const int MAP_SIZE = 8000;
static const int N_OF_ITERATIONS = 400;
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
char* generateMap();
char** initMap(int, int);
void fillMap(char** (&map));
char* toVector(char** map);
void freeMap(char** map, int width);
void updateMap(char* mapAsVector, char** storedResults, int nOfProcesses, int workloadPerProcess);
void printMap(char* mapAsVector);
void printResponses(char** storedResults, int nOfProcesses, int workloadPerProcess);
void printResponse(char* storedResults, int workloadPerProcess);
// Calculations
int countnNeighbours(char*, int);
int countLiveCells(char* mapAsVector);

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
	char* mapAsVector = generateMap();
	// printMap(mapAsVector);
	int fullSize = MAP_SIZE * MAP_SIZE;
	MPI_Request* sentRequests = (MPI_Request*) malloc(excEnv.numberOfProcesses * sizeof(MPI_Request));
	MPI_Request* recivedRequests = (MPI_Request*) malloc(excEnv.numberOfProcesses * sizeof(MPI_Request));
	MPI_Status status;
	int workload = fullSize / (excEnv.numberOfProcesses - 1);
	char** storedResults = initMap(excEnv.numberOfProcesses, workload);

	clock_t begin_pt = clock();

	for(int i = 0; i < N_OF_ITERATIONS; i++) {
		for (int p = 1; p < excEnv.numberOfProcesses; p++) {
			MPI_Send(mapAsVector, fullSize, MPI_CHAR, p, Tags::BROADCAST, MPI_COMM_WORLD);
			// cout << "[Master] :: send REQUEST to " << p << endl;
		}

		for(int p = 1; p < excEnv.numberOfProcesses; p++) {
			MPI_Recv(storedResults[p - 1], workload, MPI_CHAR, p, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			// cout << "[Master] :: got RESPONSE from " << p << endl;
		}
		// printResponses(storedResults, excEnv.numberOfProcesses, workload);
		updateMap(mapAsVector, storedResults, excEnv.numberOfProcesses, workload);
		// printMap(mapAsVector);
	}

	// Terminate slaves
	for(int p = 1; p < excEnv.numberOfProcesses; p++) {
		MPI_Isend(0, 0, MPI_CHAR, p, Tags::TERMINATE, MPI_COMM_WORLD, &sentRequests[p]);
	}

	cout << endl << "Iterations: " << N_OF_ITERATIONS << endl;
	cout << endl << "Map size: " << MAP_SIZE << endl;
	cout << endl << "Living cells: " << countLiveCells(mapAsVector) << endl;
	cout << endl << "Took  :  " << double(clock() - begin_pt) / CLOCKS_PER_SEC;
}

void slaveWork(ExecutionEnviornment excEnv) {
	char* mapAsVector = (char*) malloc(MAP_SIZE * MAP_SIZE * sizeof(char));
	int fullSize = MAP_SIZE * MAP_SIZE;
	MPI_Request recivedRequest;
	MPI_Request sentRequest;
	MPI_Status status;

	while(1) {
		// cout << "[" << excEnv.idOfCurrentProcess << "] :: " << "WAITING." << endl;
		MPI_Recv(mapAsVector, fullSize, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		// cout <<  "[" << excEnv.idOfCurrentProcess << "] :: " << "got REQUEST." << endl;

		if(status.MPI_TAG == Tags::TERMINATE) {
			// cout <<  "[" << excEnv.idOfCurrentProcess << "] :: " << "TERMINATING." << endl;
			return;
		}

		int workPerThread = fullSize / (excEnv.numberOfProcesses - 1); // Master is not working.
		char* result = (char*) malloc(workPerThread * sizeof(char));
		int relativePointer = 0;
		int nbhs;
		for(int i = (excEnv.idOfCurrentProcess - 1) * workPerThread; i < (excEnv.idOfCurrentProcess) * workPerThread; ++i) {
			nbhs = countnNeighbours(mapAsVector, i);
			if(nbhs == 1 || nbhs == 2) result[relativePointer] = 1;
			else result[relativePointer] = 0;
			++relativePointer;
		}

		// Sending results back to master
		MPI_Send(result, workPerThread, MPI_CHAR, 0, Tags::CALCULATION, MPI_COMM_WORLD);
		// cout << "[" << excEnv.idOfCurrentProcess << "] :: " << "sent data." << endl;

		free(result);
	}
}

int countnNeighbours(char* mapAsVector, int currentPosition) {
	int y = currentPosition / MAP_SIZE;
	int x = currentPosition % MAP_SIZE;
	int count = 0;
	for(int local_x = x - 1; local_x <= x + 1; local_x++) {
		for(int local_y = y - 1; local_y <= y + 1; local_y++) {
			if(local_x == -1 || local_y == -1 || local_x == MAP_SIZE || local_y == MAP_SIZE) {
				continue;
			}

			if(mapAsVector[local_y * MAP_SIZE + local_x] == 1) count++;
		}
	}
	return count;
}

char* generateMap() {
	char** map = initMap(MAP_SIZE, MAP_SIZE);
	fillMap(map);
	char* vector = toVector(map);
	freeMap(map, MAP_SIZE);
	return vector;
}

char** initMap(int width, int height) {
	char** map = (char**) malloc(width * sizeof(char*));
	for (int i = 0; i < width; i++)
		map[i] = (char*) malloc(height * sizeof(char));

	return map;
}

void fillMap(char** (&map)) {
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

char* toVector(char** map) {
	char* vector = (char*) malloc(MAP_SIZE * MAP_SIZE * sizeof(char));
	int idx = 0;
	for(int i = 0; i < MAP_SIZE; i++) {
		for(int j = 0; j < MAP_SIZE; j++) {
			vector[idx++] = map[i][j];
		}
	}

	return vector;
}

void freeMap(char** map, int width) {
	for(int i = 0; i < width; i++) {
		free(map[i]);
	}

	free(map);
}

void updateMap(char* mapAsVector, char** storedResults, int nOfProcesses, int workloadPerProcess) {
	int globalPointer = 0;
	for(int p = 0; p < nOfProcesses - 1; p++) { // We exclude master
		for(int localPointer = 0; localPointer < workloadPerProcess; localPointer++) {
			mapAsVector[globalPointer] = storedResults[p][localPointer];
			globalPointer++;
		}
	}
}

void printMap(char* mapAsVector) {
	for(int x = 0; x < MAP_SIZE; x++) {
		cout << endl;
		for(int y = 0; y < MAP_SIZE; y++) {
			cout << mapAsVector[x * MAP_SIZE + y];
		}
	}
	cout << endl;
}

void printResponses(char** storedResults, int nOfProcesses, int workloadPerProcess) {
	int globalPointer = 0;
	for(int p = 0; p < nOfProcesses - 1; p++) { // We exclude master
		cout << endl;
		printResponse(storedResults[p], workloadPerProcess);
	}
	cout << endl;
}

void printResponse(char* storedResults, int workloadPerProcess) {
	for(int localPointer = 0; localPointer < workloadPerProcess; localPointer++) {
		cout << storedResults[localPointer] << "  ";
	}

	cout << endl;
}

int countLiveCells(char* mapAsVector) {
	int count = 0;
	for(int i = 0; i < MAP_SIZE * MAP_SIZE; i++) {
		if(mapAsVector[i] == 1) count++;
	}

	return count;
}