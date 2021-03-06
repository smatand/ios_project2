#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <semaphore.h>	// semaphores
#include <stdarg.h> 	// va_list, va_start, va_end
#include <unistd.h>		// fork(), usleep()
#include <fcntl.h>		// O_CREAT
#include <sys/mman.h> 	// shared memory
#include <sys/types.h>	// wait()
#include <sys/wait.h>	// wait()
#include <time.h>		// srand()

#define MAX_TI 1000
#define MAX_TB 1000

/**
 * @struct Parameters
 */
struct params {
	int n_oxygens;
	int n_hydrogens;
	int t_i; // maximum time for atom to wait [ms]
	int t_b; // maximum time necessary for creating one molecule [ms]
};
typedef struct params params_t;

/**
 * @struct Semaphores
 */
struct semaphores {
	// allows only one action at a time
	sem_t * mutex;
	// for signaling oxygen to create a molecule
	sem_t * oxygen;
	// for signaling hydrogen to create a molecule
	sem_t * hydrogen;
	// signal, that creating molecule has ended
	sem_t * hydrogen_create;
	// signal to start with creating molecule (one at a time)
	sem_t * barrier;
	// signal for H to pass to the if() else condition
	sem_t * barrier_h;
};
typedef struct semaphores semaphores_t;

/**
 * @struct Shared variables
 */
struct shared_variables {
	int count_action;
	int count_oxygen;
	int count_hydrogen;
	int count_hydrogen_max; // 2
	int count_molecule;
};
typedef struct shared_variables shared_variables_t;

/**
 * @brief Initialize the semaphores and return the struct
 * 
 * @param sems struct to fill
 * @return 0 if successful, 1 otherwise
 */
int init_sems(semaphores_t * sems) {
	sems->mutex = sem_open("xsmata03_mutex", O_CREAT, 0644, 1);
	sems->hydrogen = sem_open("xsmata03_hydrogen", O_CREAT, 0644, 0);
	sems->hydrogen_create = sem_open("xsmata03_hydrogen_create", O_CREAT, 0644, 0);
	sems->oxygen = sem_open("xsmata03_oxygen", O_CREAT, 0644, 0);
	sems->barrier = sem_open("xsmata03_barrier", O_CREAT, 0644, 1);
	sems->barrier_h = sem_open("xsmata03_barrier_h", O_CREAT, 0644, 2);

	if (sems->mutex == SEM_FAILED || sems->hydrogen == SEM_FAILED || sems->hydrogen_create == SEM_FAILED ||
	sems->oxygen == SEM_FAILED || sems->barrier == SEM_FAILED || sems->barrier_h == SEM_FAILED) {
		return 1;
	}

	return 0;
}

/**
 * @brief Destroy the semaphores
 * 
 * @param sems struct to destruct
 */
void destroy_sems(semaphores_t * sems) {
	sem_close(sems->mutex);
	sem_close(sems->hydrogen);
	sem_close(sems->hydrogen_create);
	sem_close(sems->oxygen);
	sem_close(sems->hydrogen);
	sem_close(sems->barrier_h);

	sem_unlink("xsmata03_mutex");
	sem_unlink("xsmata03_hydrogen");
	sem_unlink("xsmata03_hydrogen_create");
	sem_unlink("xsmata03_oxygen");
	sem_unlink("xsmata03_barrier");
	sem_unlink("xsmata03_barrier_h");
}

/**
 * @brief Print a message to the file
 * 
 * @param fp file to print to
 * @param msg message to print
 * @param ... parameters of print
 */
void fprint_act(FILE * fp, const char *msg, ...) {
	va_list args;
	va_start(args, msg);
	vfprintf(fp, msg, args);
	va_end(args);
	fflush(fp);
}


/**
 * @brief Cleanup the shared memory
 * 
 * @param shm shared memory to destroy
 * @param sems semaphores to destroy
 */
void clean_shm(shared_variables_t * shm, semaphores_t * sems) {
	munmap(shm, sizeof(shared_variables_t));
	munmap(sems, sizeof(semaphores_t));
}


/**
 * @brief Parse the parameters
 * @param params structure to fill
 * @param argc number of arguments
 * @param argv array of arguments
 * @return 0 if successful, -1 otherwise
 */
int parse_p(int argc, char ** argv, params_t * pars) {
	if (argc != 5) {
		fprintf(stderr, "Usage: ./proj2 NO NH TI TB.\n");
		return 1;
	}

	char * endPtr = NULL;

	// argv[1] = NO, argv[2] = NH, argv[3] = TI, argv[4] = TB
	for (int i = 1; i < argc; i++) {
		int tmp = strtol(argv[i], &endPtr, 10);

		if (tmp < 0 || *endPtr != 0) {
			fprintf(stderr, "Parsed value is not a positive integer.\n");
			return 1;
		}
	}

	pars->n_oxygens = strtol(argv[1], &endPtr, 10);
	pars->n_hydrogens = strtol(argv[2], &endPtr, 10);
	pars->t_i = strtol(argv[3], &endPtr, 10);
	pars->t_b = strtol(argv[4], &endPtr, 10);

	if (pars->t_b > MAX_TB || pars->t_i > MAX_TI) {
		fprintf(stderr, "TB or TI have to be in range <0,1000> incl.\n");
		return 1;
	}
	
	if (pars->n_hydrogens == 0 || pars->n_oxygens == 0) {
		fprintf(stderr, "NH and NO have to be > 0.\n");
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
				sem_wait(sems->oxygen);

				// simulation of molecule creation
				usleep((rand() % (pars.t_b+1)) * 1000);

				// signal that creating molecule is finished
				sem_post(sems->hydrogen_create); 
				sem_post(sems->hydrogen_create);

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: O %d: molecule %d created\n", vars->count_action++, i+1, vars->count_molecule);
				sem_post(sems->mutex);

				// wait for molecule creation of 2 remaining Hs
				sem_wait(sems->oxygen); 
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
		// error forking
		else if (pid_o == -1) {
			ret_value = 1;
			fprintf(stderr, "Error creating oxygen process");
			kill(pid_o, SIGTERM);

			goto cleanup_sems;
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

			sem_wait(sems->barrier_h);
			
			if (vars->count_hydrogen >= 2 && vars->count_oxygen >= 1) {
				sem_wait(sems->hydrogen); // 1st and 2nd hydrogen waits for oxygen to free 2 H

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: H %d: creating molecule %d\n", vars->count_action++, i+1, vars->count_molecule);
				vars->count_hydrogen_max++;
				sem_post(sems->mutex);

				// decrement counters
				if (vars->count_hydrogen_max == 2) {
					vars->count_hydrogen -= 2;
					vars->count_hydrogen_max = 0;
					vars->count_oxygen--;
				}

				sem_post(sems->oxygen);

				sem_wait(sems->hydrogen_create);

				sem_wait(sems->mutex);
				fprint_act(fp, "%d: H %d: molecule %d created\n", vars->count_action++, i+1, vars->count_molecule);
				sem_post(sems->mutex);

				sem_post(sems->oxygen);
			} else {
				sem_wait(sems->mutex);
				fprint_act(fp, "%d: H %d: not enough O or H\n", vars->count_action++, i+1);
				sem_post(sems->mutex);
			}

			sem_post(sems->barrier_h);

			exit(0); // exit from child process TODO: _Exit()
		}
		// error forking
		else if (pid_h == -1) {
			ret_value = 1;
			fprintf(stderr, "Error creating hydrogen process");
			kill(pid_h, SIGTERM);

			goto cleanup_sems;
		}
	}
	//////////////////////////////////////////////////////////////////
	while (wait(NULL) > 0); // wait for child processes to die

cleanup_sems:
	destroy_sems(sems);
cleanup_shm:
	clean_shm(vars, sems);

	fclose(fp);
	return ret_value;
}