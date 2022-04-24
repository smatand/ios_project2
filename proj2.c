#include <stdlib.h>
#include <string.h>
#include <stdarg.h> 	// va_list, va_start, va_end
#include <fcntl.h>		// O_CREAT
#include <sys/mman.h> 	// shared memory

#include "proj2.h"		// <stdio.h>, <semaphores.h>

#define PRINT_SEMAPHORE_VALUE(sem) do {int temp=0; sem_getvalue(sem, &temp); fprint_act(fp, "SEMAPHORE_L%d\t%s: %d\n", __LINE__, #sem, temp);} while(0)
#define PRINT_SHARED_VAR(var) do {fprint_act(fp, "SHARED_VAR_L%d\t%s: %d\n", __LINE__, #var, var);} while(0)

#define MAX_TI 1000
#define MAX_TB 1000

int init_sems(semaphores_t * sems) {
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

void destroy_sems(semaphores_t * sems) {
	sem_close(sems->mutex);
	sem_close(sems->hydrogen);
	sem_close(sems->oxygen);
	sem_close(sems->hydrogen);

	sem_unlink("xsmata03_mutex");
	sem_unlink("xsmata03_hydrogen");
	sem_unlink("xsmata03_oxygen");
	sem_unlink("xsmata03_barrier");
}

void fprint_act(FILE * fp, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	vfprintf(fp, msg, args);
	va_end(args);
	fflush(fp);
}

void clean_shm(shared_variables_t * shm, semaphores_t * sems) {
	munmap(shm, sizeof(shared_variables_t));
	munmap(sems, sizeof(semaphores_t));
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
	/*** Parse parameters ***/
	params_t pars;
	if (parse_p(argc, argv, &pars)) {
		return 1;
	}	
	/************************/
	
	/*** File initialization ***/
	FILE * fp;
	fp = fopen("proj2.out", "w");
	if (fp == NULL) {
		fprintf(stderr, "Error creating log file");
		return 1;
	}
	/**************************/

	int ret_value = 0;

	/*** Shared memory initialization ***/
	shared_variables_t * vars = mmap(NULL, sizeof(shared_variables_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
	semaphores_t * sems = mmap(NULL, sizeof(semaphores_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);

	if (sems == MAP_FAILED || vars == MAP_FAILED) {
		ret_value = 1;
		fprintf(stderr, "Error creating shared memory");
		goto cleanup_shm;
	}

	if (init_sems(sems)) {
		ret_value = 1;
		fprintf(stderr, "Error creating semaphores");
		goto cleanup_sems;
	}

	vars->count_action = 1;
	vars->count_oxygen = 0;
	vars->count_hydrogen = 0;
	/************************************/

	//////////////////////// MAIN PART ///////////////////////////////
	PRINT_SEMAPHORE_VALUE(sems->mutex);
	PRINT_SEMAPHORE_VALUE(sems->hydrogen);
	PRINT_SEMAPHORE_VALUE(sems->oxygen);
	PRINT_SEMAPHORE_VALUE(sems->barrier);
	PRINT_SHARED_VAR(vars->count_action);
	PRINT_SHARED_VAR(vars->count_oxygen);
	PRINT_SHARED_VAR(vars->count_hydrogen);
	//////////////////////////////////////////////////////////////////

cleanup_sems:
	destroy_sems(sems);
cleanup_shm:
	clean_shm(vars, sems);

	fclose(fp);
	return ret_value;
}
