# mpi-game-of-life

Simple implementation od Game of Life (with customized live/dead conditions).
Parallelized using MPI.
Visible performance gain observed when running on multiple cores:

* Map size: 8000, iterations: 20, cores: 2 (1 performs calculations) - ~23s
* Map size: 8000, iterations: 20, cores: 5 (4 performs calculations) - ~13.5s

* Map size: 8000, iterations: 200, cores: 2 (1 performs calculations) - ~23s
* Map size: 8000, iterations: 200, cores: 5 (4 performs calculations) - ~13.5s
