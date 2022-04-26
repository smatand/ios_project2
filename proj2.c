#include <stdlib.h>
#include <string.h>
#include <stdarg.h> 	// va_list, va_start, va_end
#include <unistd.h>		// fork(), usleep()
#include <fcntl.h>		// O_CREAT
#include <sys/mman.h> 	// shared memory
#include <sys/types.h>	// wait()
#include <sys/wait.h>	// wait()
#include <time.h>		// srand()

#include "proj2.h"		// <stdio.h>, <semaphores.h>

#define PRINT_SEMAPHORE_VALUE(sem) do {int temp=0; sem_getvalue(sem, &temp); fprint_act(fp, "SEMAPHORE_L%d\t%s: %d\n", __LINE__, #sem, temp);} while(0)
#define PRINT_SHARED_VAR(var) do {fprint_act(fp, "SHARED_VAR_L%d\t%s: %d\n", __LINE__, #var, var);} while(0)

#define MAX_TI 1000
#define MAX_TB 1000

int init_sems(semaphores_t * sems) {
	sems->mutex = sem_open("xsmata03_mutex", O_CREAT, 0644, 1);
	sems->hydrogen = sem_open("xsmata03_hydrogen", O_CREAT, 0644, 0);
	sems->hydrogen_create = sem_open("xsmata03_hydrogen_create", O_CREAT, 0644, 0);
	sems->oxygen = sem_open("xsmata03_oxygen", O_CREAT, 0644, 0);
	sems->barrier = sem_open("xsmata03_barrier", O_CREAT, 0644, 1);

	if (sems->mutex == SEM_FAILED || sems->hydrogen == SEM_FAILED || sems->hydrogen_create == SEM_FAILED ||
			sems->oxygen == SEM_FAILED || sems->barrier == SEM_FAILED) {
		return 1;
	}

	return 0;
}

void destroy_sems(semaphores_t * sems) {
	sem_close(sems->mutex);
	sem_close(sems->hydrogen);
	sem_close(sems->hydrogen_create);
	sem_close(sems->oxygen);
	sem_close(sems->hydrogen);

	sem_unlink("xsmata03_mutex");
	sem_unlink("xsmata03_hydrogen");
	sem_unlink("xsmata03_hydrogen_create");
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
	vars->count_oxygen = pars.n_oxygens;
	vars->count_hydrogen = pars.n_hydrogens;
	vars->count_hydrogen_max = 0;
	vars->count_molecule = 1;
	/************************************/

	srand(time(NULL) * getpid());

	//////////////////////// MAIN PART ///////////////////////////////

	for (int i = 0; i < pars.n_oxygens; i++) {
		pid_t pid_o = fork();

		// child process
		if (pid_o == 0) {
			sem_wait(sems->mutex);
			fprint_act(fp, "%d: O %d: started\n", vars->count_action++, i+1);
			sem_post(sems->mutex);

			usleep((rand() % (pars.t_i+1)) * 1000);

			sem_wait(sems->mutex);
			fprint_act(fp, "%d: O %d: going to queue\n", vars->count_action++, i+1);
			sem_post(sems->mutex);

			sem_wait(sems->barrier); // starting process of creating 1 molecule

			if (vars->count_hydrogen >= 2) {
				sem_post(sems->hydrogen); // free 1 H
				sem_post(sems->hydrogen); // free 1 H

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: O %d: creating molecule %d\n", vars->count_action++, i+1, vars->count_molecule);
				sem_post(sems->mutex);

				sem_wait(sems->oxygen);

				// simulation of molecule creation
				usleep((rand() % (pars.t_b+1)) * 1000);

				// signal that creating molecule is finished
				sem_post(sems->hydrogen_create); 
				sem_post(sems->hydrogen_create);

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: O %d: molecule %d created\n", vars->count_action++, i+1, vars->count_molecule);
				vars->count_oxygen--;
				sem_post(sems->mutex);

				// wait for molecule creation of 2 remaining Hs
				sem_wait(sems->oxygen); 
				vars->count_molecule++;
			} else {
				sem_wait(sems->mutex);
				fprint_act(fp, "%d: O %d: not enough H\n", vars->count_action++, i+1);
				sem_post(sems->mutex);
			}
			sem_post(sems->barrier);

			exit(0); // exit from child process TODO: _Exit()
		}
		// error
		else if (pid_o == -1) {
			ret_value = 1;
			fprintf(stderr, "Error creating oxygen process");
			goto cleanup_children;
		}
	}	
	
	for (int i = 0; i < pars.n_hydrogens; i++) {
		pid_t pid_h = fork();

		// child process
		if (pid_h == 0) {
			sem_wait(sems->mutex);
			fprint_act(fp, "%d: H %d: started\n", vars->count_action++, i+1);
			sem_post(sems->mutex);

			usleep((rand() % (pars.t_i+1)) * 1000);

			sem_wait(sems->mutex);
			fprint_act(fp, "%d: H %d: going to queue\n", vars->count_action++, i+1);
			sem_post(sems->mutex);

			// free 2 H
			if (vars->count_hydrogen + vars->count_hydrogen_max >= 2 && vars->count_oxygen >= 1) {
				sem_wait(sems->hydrogen); // 1st and 2nd hydrogen waits for oxygen to free 2 H
				vars->count_hydrogen--;

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: H %d: creating molecule %d\n", vars->count_action++, i+1, vars->count_molecule);
				vars->count_hydrogen_max++;
				sem_post(sems->mutex);

				if (vars->count_hydrogen_max == 2) {
					sem_post(sems->oxygen);
				}

				sem_wait(sems->hydrogen_create);

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: H %d: molecule %d created\n", vars->count_action++, i+1, vars->count_molecule);
				vars->count_hydrogen_max--;
				sem_post(sems->mutex);


				if (vars->count_hydrogen_max == 0) {
					sem_post(sems->oxygen);
				}
			} else {
				sem_wait(sems->mutex);
				fprint_act(fp, "%d: H %d: not enough O or H\n", vars->count_action++, i+1);
				sem_post(sems->mutex);
			}

			exit(0); // exit from child process TODO: _Exit()
		}
		// error
		else if (pid_h == -1) {
			ret_value = 1;
			fprintf(stderr, "Error creating hydrogen process");
			goto cleanup_children;
		}
	}

	//////////////////////////////////////////////////////////////////

cleanup_children:
	while (wait(NULL) > 0);
cleanup_sems:
	destroy_sems(sems);
cleanup_shm:
	clean_shm(vars, sems);

	fclose(fp);
	return ret_value;
}
