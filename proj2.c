#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proj2.h"

#define MAX_TI 1000
#define MAX_TB 1000

int initSems(semaphores_t * sems) {
	sems->mutex = sem_open("xsmata03_mutex", O_CREAT, 0644, 1);
	sems->hydrogen = sem_open("xsmata03_hydrogen", O_CREAT, 0644, 0);
	sems->oxygen = sem_open("xsmata03_oxygen", O_CREAT, 0644, 0);
	sems->barrier = sem_open("xsmata03_barrier", O_CREAT, 0644, 3);

	if (sems->mutex == SEM_FAILED || sems->hydrogen == SEM_FAILED || 
			sems->oxygen == SEM_FAILED || sems->barrier == SEM_FAILED) {
		return 1;
	}

	return 0;
}

void destroySems(semaphores_t * sems) {
	sem_close(sems->mutex);
	sem_close(sems->hydrogen);
	sem_close(sems->oxygen);
	sem_close(sems->hydrogen);

	sem_unlink("xsmata03_mutex");
	sem_unlink("xsmata03_hydrogen");
	sem_unlink("xsmata03_oxygen");
	sem_unlink("xsmata03_barrier");
}

int parse_p(int argc, char ** argv, params_t * pars) {
	if (argc != 5) {
		fprintf(stderr, "Usage: ./proj2 NO NH TI TB.\n");
		return 1;
	}

	char * endPtr = NULL;

	// argv[1] = NO, argv[2] = NH, argv[3] = TI, argv[4] = TB
	for (int i = 1; i < argc; i++) {
		int tmp = (int)strtol(argv[i], &endPtr, 10);

		if (tmp < 0 || *endPtr != 0) {
			fprintf(stderr, "Parsed value is not a positive integer.\n");
			return 1;
		}
	}

	pars->n_oxygens = (int)strtol(argv[1], &endPtr, 10);
	pars->n_hydrogens = (int)strtol(argv[2], &endPtr, 10);
	pars->t_i = (int)strtol(argv[3], &endPtr, 10);
	pars->t_b = (int)strtol(argv[4], &endPtr, 10);

	if (pars->t_b > MAX_TB || pars->t_i > MAX_TI) {
		fprintf(stderr, "TB or TI have to be in range <0,1000> incl.\n");
		return 1;
	}

	return 0; // success
}

int main(int argc, char ** argv) {
	params_t pars;
	if (parse_p(argc, argv, &pars)) {
		return 1;
	}

	return 0;
}
