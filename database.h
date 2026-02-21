#ifndef DATABASE_H
#define DATABASE_H

#include "formulation.h"
#include "compound.h"
#include "tasting.h"
#include "batch.h"
#include "sqlite3.h"

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
 * Update the cost_per_gram for a single compound.
 * Change persists in the DB and survives restarts.
 * Returns 0 on success, 1 if compound not found, negative on DB error.
 */
int db_set_compound_cost(const char* compound_name, float cost_per_gram);

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

/* -------------------------------------------------------------------------
   Phase 4: Batch Scaling, Cost Analysis, Inventory
   ------------------------------------------------------------------------- */

/*
 * Seed compound_inventory with starting stock levels (INSERT OR IGNORE).
 * Safe to call every startup. Call after db_seed_compound_library.
 * Returns 0 on success, negative on DB error.
 */
int db_seed_inventory(void);

/*
 * Fill br->ingredients[*].cost_line and br->cost_total from compound_library.
 * Compounds with cost_per_gram == 0 get cost_line = -1.
 * Returns 0 on success, negative on DB error.
 */
int db_cost_batch(BatchRun* br);

/*
 * Save a batch run linked to flavor_code v major.minor.patch.
 * Auto-generates batch_number if br->batch_number is empty.
 * Fills br->id and br->batched_at on success.
 * Returns 0 on success, 1 if formulation version not found, negative on DB error.
 */
int db_save_batch(const char* flavor_code,
                  int major, int minor, int patch,
                  BatchRun* br);

/*
 * Print all batch runs for flavor_code ordered by batched_at.
 * Returns 0 on success, negative on DB error.
 */
int db_list_batches(const char* flavor_code);

/*
 * Check compound_inventory against a pending batch.
 * Prints [LOW STOCK] warnings for each shortfall.
 * Returns count of shortfalls (0 = sufficient stock for all compounds).
 */
int db_check_inventory(const BatchRun* br);

/*
 * Deduct batch ingredient quantities from compound_inventory.
 * Skips compounds not present in inventory (no error).
 * Returns 0 on success, negative on DB error.
 */
int db_deduct_inventory(const BatchRun* br);

/*
 * Print formatted inventory table with stock levels and reorder flags.
 * Returns 0 on success, negative on DB error.
 */
int db_list_inventory(void);

/*
 * Returns the raw SQLite database handle.
 * Used by panel code that queries directly.
 */
sqlite3* db_get_handle(void);

/* -------------------------------------------------------------------------
   Regulatory Limits
   ------------------------------------------------------------------------- */

/*
 * Insert a new regulatory override for compound_name.
 * effective_date must be "YYYY-MM-DD". notes may be NULL or empty.
 * Returns 0 on success, negative on DB error.
 */
int db_add_regulatory_limit(const char* compound_name,
                            const char* source,
                            float       max_use_ppm,
                            const char* effective_date,
                            const char* notes);

/*
 * Get the active maximum use ppm for compound_name.
 * Checks regulatory_limits first (most recent effective_date/id wins).
 * Falls back to compound_library if no override exists.
 * Returns  0 if an override was found,
 *          1 if falling back to compound_library,
 *         negative on DB error.
 * out_max_ppm is set on return codes 0 and 1.
 */
int db_get_active_limit(const char* compound_name, float* out_max_ppm);

#endif
