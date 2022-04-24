#ifndef IOS_PROJECT2_PROJ2_H
#define IOS_PROJECT2_PROJ2_H

#include <semaphore.h>

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

struct semaphores {
	// allows only one action at a time
	sem_t * mutex;
	// for signaling oxygen to create a molecule
	sem_t * oxygen;
	// for signaling hydrogen to create a molecule
	sem_t * hydrogen;
	// for signaling oxygen to release a molecule
	sem_t * barrier;
};
typedef struct semaphores semaphores_t;

/**
 * @brief Parse the parameters
 * @param params structure to fill
 * @param argc number of arguments
 * @param argv array of arguments
 * @return 0 if successful, -1 otherwise
 */
int read_params(params_t *params, int argc, char *argv[]);

/**
 * @brief Initialize the semaphores and return the struct
 * 
 * @param sems struct to fill
 * @return 0 if successful, 1 otherwise
 */
int init_sems(semaphores_t * sems); 

/**
 * @brief Destroy the semaphores
 * 
 * @param sems struct to destruct
 */
void destroy_sems(semaphores_t * sems);

#endif //IOS_PROJECT2_PROJ2_H
