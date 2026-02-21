#ifndef DATABASE_H
#define DATABASE_H

#include "formulation.h"
#include "compound.h"
#include "tasting.h"

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

/*
 * Seed compound_library with pre-defined compounds (INSERT OR IGNORE).
 * Safe to call every startup. Returns 0 on success, negative on DB error.
 */
int db_seed_compound_library(void);

/*
 * Insert or replace a single compound in the library.
 * Returns 0 on success, negative on DB error.
 */
int db_add_compound(const CompoundInfo* c);

/*
 * Load a compound by name into c.
 * Returns 0 = found, 1 = not found, negative = DB error.
 */
int db_get_compound_by_name(const char* name, CompoundInfo* c);

/*
 * Print formatted table of all compounds in the library.
 * Returns 0 on success, negative on DB error.
 */
int db_list_compounds(void);

/*
 * Check every compound in f against library limits.
 * Prints [SAFETY WARNING] lines for each violation.
 * Returns count of violations (0 = all clear).
 */
int db_validate_formulation(const Formulation* f);

/* -------------------------------------------------------------------------
   Phase 3: Tasting Sessions
   ------------------------------------------------------------------------- */

/*
 * Save a tasting session linked to flavor_code v major.minor.patch.
 * Looks up the formulation_id internally.
 * Fills ts->id with the new row's primary key on success.
 * Returns 0 on success, 1 if the formulation version is not found,
 * negative on DB error.
 */
int db_save_tasting(const char* flavor_code,
                    int major, int minor, int patch,
                    TastingSession* ts);

/*
 * Print all tasting sessions for every version of flavor_code,
 * ordered by tasted_at ascending.
 * Returns 0 on success, negative on DB error.
 */
int db_list_tastings_for_flavor(const char* flavor_code);

/*
 * Print aggregate average scores for all sessions of flavor_code.
 * Returns 0 on success, negative on DB error.
 */
int db_get_avg_scores(const char* flavor_code);

#endif
