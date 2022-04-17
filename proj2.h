#ifndef IOS_PROJECT2_PROJ2_H
#define IOS_PROJECT2_PROJ2_H

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
 * @brief Parse the parameters
 * @param params structure to fill
 * @param argc number of arguments
 * @param argv array of arguments
 * @return 0 if successful, -1 otherwise
 */
int read_params(params_t *params, int argc, char *argv[]);

#endif //IOS_PROJECT2_PROJ2_H
