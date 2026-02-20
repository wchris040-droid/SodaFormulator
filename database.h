#ifndef DATABASE_H
#define DATABASE_H

#include "formulation.h"

/*
 * Opens (or creates) the SQLite database at db_path.
 * Creates all tables (formulations, formulation_compounds, compound_library)
 * if they do not exist.
 * Returns 0 on success, negative on error.
 */
int db_open(const char* db_path);

/*
 * Closes the database connection.
 */
void db_close(void);

/*
 * Saves a formulation and its compounds in a single transaction.
 * Returns 0 on success, negative on error.
 * Prints a message and returns -1 if the version already exists (UNIQUE constraint).
 */
int db_save_formulation(const Formulation* f);

/*
 * Loads the highest version of flavor_code into f.
 * Returns 0 on success, 1 if not found, negative on DB error.
 */
int db_load_latest(const char* flavor_code, Formulation* f);

/*
 * Loads the exact version (major.minor.patch) of flavor_code into f.
 * Returns 0 on success, 1 if not found, negative on DB error.
 */
int db_load_version(const char* flavor_code, int major, int minor, int patch, Formulation* f);

/*
 * Prints a formatted table of all flavors at their latest version,
 * ordered alphabetically by flavor_code.
 * Returns 0 on success, negative on DB error.
 */
int db_list_formulations(void);

/*
 * Prints all saved versions of flavor_code with timestamps,
 * ordered oldest to newest.
 * Returns 0 on success, negative on DB error.
 */
int db_get_version_history(const char* flavor_code);

#endif
