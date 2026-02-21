#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "compound.h"
#include "tasting.h"
#include "batch.h"
#include "database.h"
#include "sqlite3.h"

static sqlite3* g_db = NULL;

/* =========================================================================
   Private helper: execute a parameter-free SQL statement (DDL / PRAGMA /
   transaction control).  Uses sqlite3_exec with NULL callback.
   ========================================================================= */
static int db_exec_simple(const char* sql)
{
    char* errmsg = NULL;
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error (%d): %s\n  SQL: %s\n", rc, errmsg, sql);
        sqlite3_free(errmsg);
    }
    return rc;
}

/* =========================================================================
   db_open
   ========================================================================= */
int db_open(const char* db_path)
{
    int rc;

    rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database '%s': %s\n",
                db_path, sqlite3_errmsg(g_db));
        sqlite3_close(g_db);
        g_db = NULL;
        return rc;
    }

    /* Performance and integrity settings */
    db_exec_simple("PRAGMA journal_mode=WAL;");
    db_exec_simple("PRAGMA foreign_keys=ON;");

    /* formulations — one row per saved version */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS formulations ("
        "    id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    flavor_code TEXT    NOT NULL,"
        "    flavor_name TEXT    NOT NULL,"
        "    ver_major   INTEGER NOT NULL,"
        "    ver_minor   INTEGER NOT NULL,"
        "    ver_patch   INTEGER NOT NULL,"
        "    target_ph   REAL    NOT NULL,"
        "    target_brix REAL    NOT NULL,"
        "    saved_at    TEXT    NOT NULL DEFAULT (DATETIME('now', 'localtime')),"
        "    UNIQUE (flavor_code, ver_major, ver_minor, ver_patch)"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    /* formulation_compounds — compounds per formulation version */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS formulation_compounds ("
        "    id                  INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    formulation_id      INTEGER NOT NULL REFERENCES formulations(id),"
        "    compound_name       TEXT    NOT NULL,"
        "    concentration_ppm   REAL    NOT NULL,"
        "    compound_library_id INTEGER"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    /* compound_library — Phase 2 stub; full schema defined now to avoid
       future migration.  All columns defined; only id/compound_name required. */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS compound_library ("
        "    id                   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    compound_name        TEXT    NOT NULL UNIQUE,"
        "    cas_number           TEXT,"
        "    fema_number          INTEGER,"
        "    max_use_ppm          REAL,"
        "    rec_min_ppm          REAL,"
        "    rec_max_ppm          REAL,"
        "    molecular_weight     REAL,"
        "    water_solubility     REAL,"
        "    ph_stable_min        REAL,"
        "    ph_stable_max        REAL,"
        "    odor_profile         TEXT,"
        "    storage_temp         TEXT,"
        "    requires_solubilizer INTEGER NOT NULL DEFAULT 0,"
        "    requires_inert_atm   INTEGER NOT NULL DEFAULT 0"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    /* cost_per_gram column — added in Phase 4.
       ALTER TABLE ADD COLUMN is a no-op if the column already exists
       (we just ignore the error). */
    sqlite3_exec(g_db,
        "ALTER TABLE compound_library ADD COLUMN cost_per_gram REAL NOT NULL DEFAULT 0;",
        NULL, NULL, NULL);  /* ignore error — already exists on fresh DB */

    /* tasting_sessions — one row per sensory evaluation */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS tasting_sessions ("
        "    id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    formulation_id  INTEGER NOT NULL REFERENCES formulations(id),"
        "    tasted_at       TEXT    NOT NULL DEFAULT (DATETIME('now', 'localtime')),"
        "    taster          TEXT    NOT NULL DEFAULT 'unknown',"
        "    overall_score   REAL    NOT NULL,"
        "    aroma_score     REAL,"
        "    flavor_score    REAL,"
        "    mouthfeel_score REAL,"
        "    finish_score    REAL,"
        "    sweetness_score REAL,"
        "    notes           TEXT"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    /* batch_runs — one row per production batch */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS batch_runs ("
        "    id             INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    formulation_id INTEGER NOT NULL REFERENCES formulations(id),"
        "    batch_number   TEXT    NOT NULL UNIQUE,"
        "    volume_liters  REAL    NOT NULL,"
        "    cost_total     REAL,"
        "    batched_at     TEXT    NOT NULL DEFAULT (DATETIME('now', 'localtime')),"
        "    notes          TEXT"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    /* batch_ingredients — compound weights per batch */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS batch_ingredients ("
        "    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    batch_run_id INTEGER NOT NULL REFERENCES batch_runs(id),"
        "    compound_name TEXT   NOT NULL,"
        "    grams_needed  REAL   NOT NULL,"
        "    cost_line     REAL"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    /* compound_inventory — stock levels per compound */
    rc = db_exec_simple(
        "CREATE TABLE IF NOT EXISTS compound_inventory ("
        "    id                      INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    compound_library_id     INTEGER NOT NULL UNIQUE REFERENCES compound_library(id),"
        "    stock_grams             REAL    NOT NULL DEFAULT 0,"
        "    reorder_threshold_grams REAL    NOT NULL DEFAULT 10,"
        "    last_updated            TEXT    NOT NULL DEFAULT (DATETIME('now', 'localtime'))"
        ");"
    );
    if (rc != SQLITE_OK) return rc;

    printf("Database opened: %s\n", db_path);
    return 0;
}

/* =========================================================================
   db_close
   ========================================================================= */
void db_close(void)
{
    if (g_db != NULL) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

/* =========================================================================
   Private helper: look up compound_library.id by name.
   Returns the id, or 0 if not found.
   ========================================================================= */
static sqlite3_int64 lookup_compound_library_id(const char* name)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 id = 0;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT id FROM compound_library WHERE compound_name = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) return 0;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
        id = sqlite3_column_int64(stmt, 0);

    sqlite3_finalize(stmt);
    return id;
}

/* =========================================================================
   db_save_formulation
   ========================================================================= */
int db_save_formulation(const Formulation* f)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 formulation_id;
    int rc;
    int i;
    int violations;

    violations = db_validate_formulation(f);
    if (violations > 0)
        fprintf(stderr, "  Warning: %d safety limit violation(s) noted above. Saving anyway.\n",
                violations);

    rc = db_exec_simple("BEGIN;");
    if (rc != SQLITE_OK) return rc;

    /* Insert the formulation header row */
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO formulations "
        "(flavor_code, flavor_name, ver_major, ver_minor, ver_patch, "
        " target_ph, target_brix) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    sqlite3_bind_text  (stmt, 1, f->flavor_code,   -1, SQLITE_STATIC);
    sqlite3_bind_text  (stmt, 2, f->flavor_name,   -1, SQLITE_STATIC);
    sqlite3_bind_int   (stmt, 3, f->version.major);
    sqlite3_bind_int   (stmt, 4, f->version.minor);
    sqlite3_bind_int   (stmt, 5, f->version.patch);
    sqlite3_bind_double(stmt, 6, (double)f->target_ph);
    sqlite3_bind_double(stmt, 7, (double)f->target_brix);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc == SQLITE_CONSTRAINT) {
        fprintf(stderr,
            "Error: %s v%d.%d.%d already exists in the database. "
            "Increment the version before saving.\n",
            f->flavor_code,
            f->version.major, f->version.minor, f->version.patch);
        db_exec_simple("ROLLBACK;");
        return -1;
    }
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    formulation_id = sqlite3_last_insert_rowid(g_db);

    /* Insert each compound, reusing a single prepared statement */
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO formulation_compounds "
        "(formulation_id, compound_name, concentration_ppm, compound_library_id) "
        "VALUES (?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    for (i = 0; i < f->compound_count; i++) {
        sqlite3_int64 lib_id = lookup_compound_library_id(f->compounds[i].compound_name);
        sqlite3_reset(stmt);
        sqlite3_bind_int64 (stmt, 1, formulation_id);
        sqlite3_bind_text  (stmt, 2, f->compounds[i].compound_name, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, (double)f->compounds[i].concentration_ppm);
        if (lib_id > 0)
            sqlite3_bind_int64(stmt, 4, lib_id);
        else
            sqlite3_bind_null(stmt, 4);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Compound insert error: %s\n", sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            db_exec_simple("ROLLBACK;");
            return rc;
        }
    }

    sqlite3_finalize(stmt);

    rc = db_exec_simple("COMMIT;");
    if (rc != SQLITE_OK) return rc;

    printf("Saved: %s v%d.%d.%d (%d compound%s)\n",
        f->flavor_code,
        f->version.major, f->version.minor, f->version.patch,
        f->compound_count,
        f->compound_count == 1 ? "" : "s");

    return 0;
}

/* =========================================================================
   Private helper: load compounds into f given the formulation's DB row id.
   Called while no other statement is active on g_db.
   ========================================================================= */
static int load_compounds(sqlite3_int64 formulation_id, Formulation* f)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT compound_name, concentration_ppm "
        "FROM formulation_compounds "
        "WHERE formulation_id = ? "
        "ORDER BY id ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_int64(stmt, 1, formulation_id);

    f->compound_count = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (f->compound_count >= MAX_COMPOUNDS) break;
        strncpy(f->compounds[f->compound_count].compound_name,
                (const char*)sqlite3_column_text(stmt, 0), 63);
        f->compounds[f->compound_count].compound_name[63] = '\0';
        f->compounds[f->compound_count].concentration_ppm =
            (float)sqlite3_column_double(stmt, 1);
        f->compound_count++;
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_load_latest
   ========================================================================= */
int db_load_latest(const char* flavor_code, Formulation* f)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 row_id;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT id, flavor_code, flavor_name, ver_major, ver_minor, ver_patch, "
        "       target_ph, target_brix "
        "FROM formulations "
        "WHERE flavor_code = ? "
        "ORDER BY ver_major DESC, ver_minor DESC, ver_patch DESC "
        "LIMIT 1;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 1;  /* not found */
    }
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return rc;
    }

    /* Copy the scalar fields and row id before finalizing */
    row_id = sqlite3_column_int64(stmt, 0);

    strncpy(f->flavor_code, (const char*)sqlite3_column_text(stmt, 1),
            MAX_FLAVOR_CODE - 1);
    f->flavor_code[MAX_FLAVOR_CODE - 1] = '\0';

    strncpy(f->flavor_name, (const char*)sqlite3_column_text(stmt, 2),
            MAX_FLAVOR_NAME - 1);
    f->flavor_name[MAX_FLAVOR_NAME - 1] = '\0';

    f->version.major = sqlite3_column_int(stmt, 3);
    f->version.minor = sqlite3_column_int(stmt, 4);
    f->version.patch = sqlite3_column_int(stmt, 5);
    f->target_ph     = (float)sqlite3_column_double(stmt, 6);
    f->target_brix   = (float)sqlite3_column_double(stmt, 7);

    sqlite3_finalize(stmt);  /* finalize before opening compound query */

    return load_compounds(row_id, f);
}

/* =========================================================================
   db_load_version
   ========================================================================= */
int db_load_version(const char* flavor_code, int major, int minor, int patch,
                    Formulation* f)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 row_id;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT id, flavor_code, flavor_name, ver_major, ver_minor, ver_patch, "
        "       target_ph, target_brix "
        "FROM formulations "
        "WHERE flavor_code = ? "
        "  AND ver_major = ? AND ver_minor = ? AND ver_patch = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, major);
    sqlite3_bind_int (stmt, 3, minor);
    sqlite3_bind_int (stmt, 4, patch);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 1;  /* not found */
    }
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return rc;
    }

    row_id = sqlite3_column_int64(stmt, 0);

    strncpy(f->flavor_code, (const char*)sqlite3_column_text(stmt, 1),
            MAX_FLAVOR_CODE - 1);
    f->flavor_code[MAX_FLAVOR_CODE - 1] = '\0';

    strncpy(f->flavor_name, (const char*)sqlite3_column_text(stmt, 2),
            MAX_FLAVOR_NAME - 1);
    f->flavor_name[MAX_FLAVOR_NAME - 1] = '\0';

    f->version.major = sqlite3_column_int(stmt, 3);
    f->version.minor = sqlite3_column_int(stmt, 4);
    f->version.patch = sqlite3_column_int(stmt, 5);
    f->target_ph     = (float)sqlite3_column_double(stmt, 6);
    f->target_brix   = (float)sqlite3_column_double(stmt, 7);

    sqlite3_finalize(stmt);  /* finalize before opening compound query */

    return load_compounds(row_id, f);
}

/* =========================================================================
   db_list_formulations
   ========================================================================= */
int db_list_formulations(void)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    /* Correlated subquery: for each row, verify it is the highest version
       for its flavor_code.  ORDER BY flavor_code for consistent output. */
    rc = sqlite3_prepare_v2(g_db,
        "SELECT f.flavor_code, f.flavor_name, "
        "       f.ver_major, f.ver_minor, f.ver_patch, f.saved_at "
        "FROM formulations f "
        "WHERE f.id = ("
        "    SELECT id FROM formulations "
        "    WHERE flavor_code = f.flavor_code "
        "    ORDER BY ver_major DESC, ver_minor DESC, ver_patch DESC "
        "    LIMIT 1"
        ") "
        "ORDER BY f.flavor_code ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    printf("\n--- All Formulations (latest versions) ---\n");
    printf("%-14s | %-20s | %-7s | Saved At\n",
           "Flavor Code", "Flavor Name", "Version");
    printf("%-14s-+-%-20s-+-%-7s-+-%-19s\n",
           "--------------", "--------------------", "-------",
           "-------------------");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        printf("%-14s | %-20s | %d.%d.%d   | %s\n",
               (const char*)sqlite3_column_text(stmt, 0),
               (const char*)sqlite3_column_text(stmt, 1),
               sqlite3_column_int(stmt, 2),
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               (const char*)sqlite3_column_text(stmt, 5));
    }

    sqlite3_finalize(stmt);
    printf("\n");
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_get_version_history
   ========================================================================= */
int db_get_version_history(const char* flavor_code)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT ver_major, ver_minor, ver_patch, saved_at "
        "FROM formulations "
        "WHERE flavor_code = ? "
        "ORDER BY ver_major ASC, ver_minor ASC, ver_patch ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);

    printf("Version history for %s:\n", flavor_code);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        printf("  %d.%d.%d  saved at %s\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_int(stmt, 1),
               sqlite3_column_int(stmt, 2),
               (const char*)sqlite3_column_text(stmt, 3));
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_seed_compound_library
   INSERT OR IGNORE for all 15 pre-defined compounds.
   Safe to call every startup.
   ========================================================================= */
int db_seed_compound_library(void)
{
    /* compound_name, cas_number, fema_number, max_use_ppm, rec_min_ppm,
       rec_max_ppm, molecular_weight, water_solubility, ph_stable_min,
       ph_stable_max, odor_profile, storage_temp,
       requires_solubilizer, requires_inert_atm, cost_per_gram (USD/g) */
    static const struct {
        const char* compound_name;
        const char* cas_number;
        int         fema_number;
        float       max_use_ppm;
        float       rec_min_ppm;
        float       rec_max_ppm;
        float       molecular_weight;
        float       water_solubility;
        float       ph_stable_min;
        float       ph_stable_max;
        const char* odor_profile;
        const char* storage_temp;
        int         requires_solubilizer;
        int         requires_inert_atm;
        float       cost_per_gram;
    } compounds[] = {
        { "Benzaldehyde",        "100-52-7",    2127, 100.0f,  20.0f,  80.0f, 106.12f, 3000.0f, 2.5f, 5.0f, "almond cherry maraschino",   "RT",   0, 0, 0.05f  },
        { "Benzyl acetate",      "140-11-4",    2135,  50.0f,   2.0f,  15.0f, 150.17f, 5900.0f, 3.0f, 5.0f, "jasmine fruity sweet",       "RT",   0, 0, 0.08f  },
        { "Butyric acid",        "107-92-6",    2221,   1.0f,   0.1f,   0.5f,  88.11f,    0.0f, 2.5f, 5.0f, "butter cheese rancid",       "RT",   0, 0, 0.10f  },
        { "Cinnamaldehyde",      "104-55-2",    2286,  50.0f,  10.0f,  40.0f, 132.16f, 1400.0f, 2.5f, 4.5f, "cinnamon spicy warm",        "RT",   0, 0, 0.15f  },
        { "Cyclotene",           "80-71-7",     2700,  10.0f,   1.0f,   5.0f, 112.13f, 5000.0f, 2.5f, 5.0f, "maple caramel roasted",      "RT",   0, 0, 2.00f  },
        { "Delta-decalactone",   "705-86-2",    2361,  20.0f,   1.0f,  10.0f, 170.25f,  200.0f, 3.0f, 5.5f, "peach cream coconut",        "RT",   1, 0, 1.50f  },
        { "Diacetyl",            "431-03-8",    2370,   5.0f,   0.5f,   3.0f,  86.09f,    0.0f, 2.0f, 5.0f, "butter cream",               "2-8C", 0, 0, 0.20f  },
        { "Ethyl cinnamate",     "103-36-6",    2430,  10.0f,   1.0f,   5.0f, 176.21f,  500.0f, 3.0f, 5.0f, "fruity cinnamon sweet",      "RT",   1, 0, 1.00f  },
        { "Ethyl maltol",        "4940-11-8",   3487, 150.0f,  10.0f,  80.0f, 140.14f,55000.0f, 2.5f, 5.0f, "cotton candy sweet",         "RT",   0, 0, 0.10f  },
        { "Eugenol",             "97-53-0",     2467,  20.0f,   2.0f,  10.0f, 164.20f, 2400.0f, 3.0f, 7.0f, "clove spicy warm",           "RT",   1, 0, 0.08f  },
        { "Furaneol",            "3658-77-3",   3174,   5.0f,   0.1f,   1.0f, 128.13f,50000.0f, 2.5f, 4.5f, "caramel strawberry sweet",   "2-8C", 0, 0, 5.00f  },
        { "Gamma-undecalactone", "104-67-6",    3091,  20.0f,   1.0f,  10.0f, 184.28f,  100.0f, 3.0f, 5.5f, "peach creamy fruity",        "RT",   1, 0, 2.00f  },
        { "Maltol",              "118-71-8",    2656, 200.0f,  20.0f, 100.0f, 126.11f, 8000.0f, 2.5f, 5.0f, "sweet caramel cotton candy", "RT",   0, 0, 0.15f  },
        { "Sotolon",             "28664-35-9",  3634,   0.5f,  0.01f,   0.3f, 128.15f,50000.0f, 2.5f, 5.0f, "maple caramel fenugreek",    "2-8C", 0, 0, 10.00f },
        { "Vanillin",            "121-33-5",    3107, 150.0f,  20.0f, 100.0f, 152.15f,10000.0f, 2.0f, 5.0f, "vanilla sweet creamy",       "RT",   0, 0, 0.05f  },
    };
    static const int NCOMPOUNDS = 15;

    sqlite3_stmt* stmt = NULL;
    int rc;
    int i;
    int count;

    rc = db_exec_simple("BEGIN;");
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR IGNORE INTO compound_library "
        "(compound_name, cas_number, fema_number, max_use_ppm, rec_min_ppm, "
        " rec_max_ppm, molecular_weight, water_solubility, ph_stable_min, "
        " ph_stable_max, odor_profile, storage_temp, "
        " requires_solubilizer, requires_inert_atm, cost_per_gram) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    for (i = 0; i < NCOMPOUNDS; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_text  (stmt,  1, compounds[i].compound_name,    -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt,  2, compounds[i].cas_number,       -1, SQLITE_STATIC);
        sqlite3_bind_int   (stmt,  3, compounds[i].fema_number);
        sqlite3_bind_double(stmt,  4, (double)compounds[i].max_use_ppm);
        sqlite3_bind_double(stmt,  5, (double)compounds[i].rec_min_ppm);
        sqlite3_bind_double(stmt,  6, (double)compounds[i].rec_max_ppm);
        sqlite3_bind_double(stmt,  7, (double)compounds[i].molecular_weight);
        sqlite3_bind_double(stmt,  8, (double)compounds[i].water_solubility);
        sqlite3_bind_double(stmt,  9, (double)compounds[i].ph_stable_min);
        sqlite3_bind_double(stmt, 10, (double)compounds[i].ph_stable_max);
        sqlite3_bind_text  (stmt, 11, compounds[i].odor_profile,     -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt, 12, compounds[i].storage_temp,     -1, SQLITE_STATIC);
        sqlite3_bind_int   (stmt, 13, compounds[i].requires_solubilizer);
        sqlite3_bind_int   (stmt, 14, compounds[i].requires_inert_atm);
        sqlite3_bind_double(stmt, 15, (double)compounds[i].cost_per_gram);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Seed insert error: %s\n", sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            db_exec_simple("ROLLBACK;");
            return rc;
        }
    }

    sqlite3_finalize(stmt);
    stmt = NULL;

    /* Set cost_per_gram only where it hasn't been set yet (cost == 0).
       This preserves any user edits made after initial seeding. */
    rc = sqlite3_prepare_v2(g_db,
        "UPDATE compound_library SET cost_per_gram = ? "
        "WHERE compound_name = ? AND cost_per_gram = 0;",
        -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        for (i = 0; i < NCOMPOUNDS; i++) {
            sqlite3_reset(stmt);
            sqlite3_bind_double(stmt, 1, (double)compounds[i].cost_per_gram);
            sqlite3_bind_text  (stmt, 2, compounds[i].compound_name, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    rc = db_exec_simple("COMMIT;");
    if (rc != SQLITE_OK) return rc;

    /* Report total count in library */
    {
        sqlite3_stmt* cnt_stmt = NULL;
        count = 0;
        if (sqlite3_prepare_v2(g_db,
                "SELECT COUNT(*) FROM compound_library;",
                -1, &cnt_stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(cnt_stmt) == SQLITE_ROW)
                count = sqlite3_column_int(cnt_stmt, 0);
            sqlite3_finalize(cnt_stmt);
        }
    }

    printf("Compound library: %d compounds seeded.\n", count);
    return 0;
}

/* =========================================================================
   db_add_compound
   ========================================================================= */
int db_add_compound(const CompoundInfo* c)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO compound_library "
        "(compound_name, cas_number, fema_number, max_use_ppm, rec_min_ppm, "
        " rec_max_ppm, molecular_weight, water_solubility, ph_stable_min, "
        " ph_stable_max, odor_profile, storage_temp, "
        " requires_solubilizer, requires_inert_atm) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text  (stmt,  1, c->compound_name,      -1, SQLITE_STATIC);
    sqlite3_bind_text  (stmt,  2, c->cas_number,         -1, SQLITE_STATIC);
    sqlite3_bind_int   (stmt,  3, c->fema_number);
    sqlite3_bind_double(stmt,  4, (double)c->max_use_ppm);
    sqlite3_bind_double(stmt,  5, (double)c->rec_min_ppm);
    sqlite3_bind_double(stmt,  6, (double)c->rec_max_ppm);
    sqlite3_bind_double(stmt,  7, (double)c->molecular_weight);
    sqlite3_bind_double(stmt,  8, (double)c->water_solubility);
    sqlite3_bind_double(stmt,  9, (double)c->ph_stable_min);
    sqlite3_bind_double(stmt, 10, (double)c->ph_stable_max);
    sqlite3_bind_text  (stmt, 11, c->odor_profile,       -1, SQLITE_STATIC);
    sqlite3_bind_text  (stmt, 12, c->storage_temp,       -1, SQLITE_STATIC);
    sqlite3_bind_int   (stmt, 13, c->requires_solubilizer);
    sqlite3_bind_int   (stmt, 14, c->requires_inert_atm);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_get_compound_by_name
   ========================================================================= */
int db_get_compound_by_name(const char* name, CompoundInfo* c)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT id, compound_name, cas_number, fema_number, max_use_ppm, "
        "       rec_min_ppm, rec_max_ppm, molecular_weight, water_solubility, "
        "       ph_stable_min, ph_stable_max, odor_profile, storage_temp, "
        "       requires_solubilizer, requires_inert_atm, cost_per_gram "
        "FROM compound_library WHERE compound_name = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 1;  /* not found */
    }
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return rc;
    }

    c->id = sqlite3_column_int(stmt, 0);

    strncpy(c->compound_name, (const char*)sqlite3_column_text(stmt, 1), 127);
    c->compound_name[127] = '\0';

    strncpy(c->cas_number, (const char*)sqlite3_column_text(stmt, 2), 15);
    c->cas_number[15] = '\0';

    c->fema_number      = sqlite3_column_int   (stmt,  3);
    c->max_use_ppm      = (float)sqlite3_column_double(stmt,  4);
    c->rec_min_ppm      = (float)sqlite3_column_double(stmt,  5);
    c->rec_max_ppm      = (float)sqlite3_column_double(stmt,  6);
    c->molecular_weight = (float)sqlite3_column_double(stmt,  7);
    c->water_solubility = (float)sqlite3_column_double(stmt,  8);
    c->ph_stable_min    = (float)sqlite3_column_double(stmt,  9);
    c->ph_stable_max    = (float)sqlite3_column_double(stmt, 10);

    strncpy(c->odor_profile, (const char*)sqlite3_column_text(stmt, 11), 255);
    c->odor_profile[255] = '\0';

    strncpy(c->storage_temp, (const char*)sqlite3_column_text(stmt, 12), 15);
    c->storage_temp[15] = '\0';

    c->requires_solubilizer = sqlite3_column_int   (stmt, 13);
    c->requires_inert_atm   = sqlite3_column_int   (stmt, 14);
    c->cost_per_gram        = (float)sqlite3_column_double(stmt, 15);

    sqlite3_finalize(stmt);
    return 0;
}

/* =========================================================================
   db_list_compounds
   ========================================================================= */
int db_list_compounds(void)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_stmt* cnt_stmt = NULL;
    int count = 0;
    int rc;

    /* Get total count for the header */
    if (sqlite3_prepare_v2(g_db,
            "SELECT COUNT(*) FROM compound_library;",
            -1, &cnt_stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(cnt_stmt) == SQLITE_ROW)
            count = sqlite3_column_int(cnt_stmt, 0);
        sqlite3_finalize(cnt_stmt);
    }

    rc = sqlite3_prepare_v2(g_db,
        "SELECT compound_name, fema_number, max_use_ppm, rec_min_ppm, "
        "       rec_max_ppm, requires_solubilizer, odor_profile "
        "FROM compound_library ORDER BY compound_name ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    printf("\n--- Compound Library (%d compounds) ---\n", count);
    printf("%-22s| %-4s | %-7s | %-15s | %-11s | %s\n",
           "Compound", "FEMA", "Max ppm", "Rec range", "Solubilizer", "Odor");
    printf("%-22s+------+---------+-----------------+-------------+------------------------------\n",
           "----------------------");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* cname     = (const char*)sqlite3_column_text(stmt, 0);
        int         fema      = sqlite3_column_int(stmt, 1);
        double      max_ppm   = sqlite3_column_double(stmt, 2);
        double      rec_min   = sqlite3_column_double(stmt, 3);
        double      rec_max   = sqlite3_column_double(stmt, 4);
        int         solub     = sqlite3_column_int(stmt, 5);
        const char* odor_full = (const char*)sqlite3_column_text(stmt, 6);
        char        odor[32];
        int         len;

        /* Truncate odor at 28 chars */
        strncpy(odor, odor_full, 28);
        odor[28] = '\0';
        len = (int)strlen(odor);
        if (len == 28 && odor_full[28] != '\0') {
            odor[25] = '.'; odor[26] = '.'; odor[27] = '.'; odor[28] = '\0';
        }

        printf("%-22s| %4d | %7.1f | %6.2f - %6.2f   | %-11s | %s\n",
               cname, fema, max_ppm, rec_min, rec_max,
               solub ? "Yes" : "No",
               odor);
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_set_compound_cost
   ========================================================================= */
int db_set_compound_cost(const char* compound_name, float cost_per_gram)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "UPDATE compound_library SET cost_per_gram = ? WHERE compound_name = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_double(stmt, 1, (double)cost_per_gram);
    sqlite3_bind_text  (stmt, 2, compound_name, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Cost update error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    if (sqlite3_changes(g_db) == 0) {
        fprintf(stderr, "Cost update: compound '%s' not found.\n", compound_name);
        return 1;
    }

    printf("Cost updated: %s = $%.4f / g\n", compound_name, cost_per_gram);
    return 0;
}

/* =========================================================================
   db_validate_formulation
   ========================================================================= */
int db_validate_formulation(const Formulation* f)
{
    CompoundInfo info;
    int violations = 0;
    int i;
    int result;

    for (i = 0; i < f->compound_count; i++) {
        result = db_get_compound_by_name(f->compounds[i].compound_name, &info);
        if (result == 0) {
            if (compound_check_limit(&info, f->compounds[i].concentration_ppm) == 1) {
                fprintf(stderr, "  [SAFETY WARNING] %s: %.2f ppm exceeds max %.2f ppm\n",
                    f->compounds[i].compound_name,
                    f->compounds[i].concentration_ppm,
                    info.max_use_ppm);
                violations++;
            }
        }
    }
    return violations;
}

/* =========================================================================
   Private helper: bind a score that may be -1 (not scored) → NULL.
   ========================================================================= */
static void bind_score(sqlite3_stmt* stmt, int col, float score)
{
    if (score < 0.0f)
        sqlite3_bind_null(stmt, col);
    else
        sqlite3_bind_double(stmt, col, (double)score);
}

/* =========================================================================
   db_save_tasting
   ========================================================================= */
int db_save_tasting(const char* flavor_code,
                    int major, int minor, int patch,
                    TastingSession* ts)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 formulation_id = 0;
    int rc;

    /* Look up the formulation's DB id */
    rc = sqlite3_prepare_v2(g_db,
        "SELECT id FROM formulations "
        "WHERE flavor_code = ? "
        "  AND ver_major = ? AND ver_minor = ? AND ver_patch = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, major);
    sqlite3_bind_int (stmt, 3, minor);
    sqlite3_bind_int (stmt, 4, patch);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        fprintf(stderr, "Tasting: formulation %s v%d.%d.%d not found.\n",
                flavor_code, major, minor, patch);
        return 1;
    }
    if (rc != SQLITE_ROW) {
        fprintf(stderr, "Step error: %s\n", sqlite3_errmsg(g_db));
        sqlite3_finalize(stmt);
        return rc;
    }
    formulation_id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* Insert tasting session */
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO tasting_sessions "
        "(formulation_id, taster, overall_score, aroma_score, flavor_score, "
        " mouthfeel_score, finish_score, sweetness_score, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_int64(stmt, 1, formulation_id);
    sqlite3_bind_text (stmt, 2, ts->taster[0] ? ts->taster : "unknown", -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, (double)ts->overall_score);
    bind_score(stmt, 4, ts->aroma_score);
    bind_score(stmt, 5, ts->flavor_score);
    bind_score(stmt, 6, ts->mouthfeel_score);
    bind_score(stmt, 7, ts->finish_score);
    bind_score(stmt, 8, ts->sweetness_score);
    if (ts->notes[0])
        sqlite3_bind_text(stmt, 9, ts->notes, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 9);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Tasting insert error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    ts->id             = (int)sqlite3_last_insert_rowid(g_db);
    ts->formulation_id = (int)formulation_id;

    printf("Tasting saved: %s v%d.%d.%d  taster=%s  overall=%.1f\n",
           flavor_code, major, minor, patch,
           ts->taster[0] ? ts->taster : "unknown",
           ts->overall_score);
    return 0;
}

/* =========================================================================
   db_list_tastings_for_flavor
   ========================================================================= */
int db_list_tastings_for_flavor(const char* flavor_code)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT t.id, f.ver_major, f.ver_minor, f.ver_patch, "
        "       t.taster, t.tasted_at, "
        "       t.overall_score, t.aroma_score, t.flavor_score, "
        "       t.mouthfeel_score, t.finish_score, t.sweetness_score, "
        "       t.notes "
        "FROM tasting_sessions t "
        "JOIN formulations f ON f.id = t.formulation_id "
        "WHERE f.flavor_code = ? "
        "ORDER BY t.tasted_at ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);

    printf("\n--- Tasting Sessions: %s ---\n", flavor_code);
    printf("%-4s | %-7s | %-16s | %-20s | %-5s | %-5s | %-5s | %-5s | %-5s | %-5s | Notes\n",
           "ID", "Version", "Taster", "Tasted At",
           "Ovrl", "Arom", "Flav", "Mfeel", "Fin", "Sweet");
    printf("-----+----------+------------------+----------------------+-------+-------+-------+-------+-------+-------+-----------\n");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int   sid    = sqlite3_column_int(stmt, 0);
        int   vmaj   = sqlite3_column_int(stmt, 1);
        int   vmin   = sqlite3_column_int(stmt, 2);
        int   vpat   = sqlite3_column_int(stmt, 3);
        const char* taster  = (const char*)sqlite3_column_text(stmt, 4);
        const char* tat     = (const char*)sqlite3_column_text(stmt, 5);
        double ovrl  = sqlite3_column_double(stmt, 6);
        char arom[6], flav[6], mf[6], fin[6], sw[6];

        /* Format optional scores */
#define FMT_OPT(buf, col) \
        if (sqlite3_column_type(stmt, (col)) == SQLITE_NULL) \
            strncpy((buf), "  --", 5); \
        else \
            snprintf((buf), 6, "%5.1f", sqlite3_column_double(stmt, (col)))

        FMT_OPT(arom, 7);
        FMT_OPT(flav, 8);
        FMT_OPT(mf,   9);
        FMT_OPT(fin, 10);
        FMT_OPT(sw,  11);
#undef FMT_OPT

        const char* notes = sqlite3_column_type(stmt, 12) == SQLITE_NULL
                          ? ""
                          : (const char*)sqlite3_column_text(stmt, 12);

        char version[10];
        snprintf(version, sizeof(version), "%d.%d.%d", vmaj, vmin, vpat);

        printf("%-4d | %-7s | %-16s | %-20s | %5.1f | %s | %s | %s | %s | %s | %s\n",
               sid, version,
               taster ? taster : "unknown",
               tat ? tat : "",
               ovrl, arom, flav, mf, fin, sw,
               notes);
    }

    sqlite3_finalize(stmt);
    printf("\n");
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_get_avg_scores
   ========================================================================= */
int db_get_avg_scores(const char* flavor_code)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT COUNT(*), "
        "       AVG(t.overall_score), "
        "       AVG(t.aroma_score), "
        "       AVG(t.flavor_score), "
        "       AVG(t.mouthfeel_score), "
        "       AVG(t.finish_score), "
        "       AVG(t.sweetness_score) "
        "FROM tasting_sessions t "
        "JOIN formulations f ON f.id = t.formulation_id "
        "WHERE f.flavor_code = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        int    n    = sqlite3_column_int(stmt, 0);
        double ovrl = sqlite3_column_double(stmt, 1);

        printf("\n--- Avg Scores: %s  (%d session%s) ---\n",
               flavor_code, n, n == 1 ? "" : "s");

        if (n == 0) {
            printf("  No tasting sessions recorded.\n");
        } else {
#define PRINT_AVG(label, col) \
            if (sqlite3_column_type(stmt, (col)) == SQLITE_NULL) \
                printf("  %-18s  --\n", (label)); \
            else \
                printf("  %-18s  %.2f / 10\n", (label), sqlite3_column_double(stmt, (col)))

            printf("  %-18s  %.2f / 10\n", "Overall", ovrl);
            PRINT_AVG("Aroma",     2);
            PRINT_AVG("Flavor",    3);
            PRINT_AVG("Mouthfeel", 4);
            PRINT_AVG("Finish",    5);
            PRINT_AVG("Sweetness", 6);
#undef PRINT_AVG
        }
        printf("\n");
    }

    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW || rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_seed_inventory
   INSERT OR IGNORE so user edits are preserved across restarts.
   ========================================================================= */
int db_seed_inventory(void)
{
    /* name, stock_grams, reorder_threshold_grams */
    static const struct { const char* name; float stock; float reorder; } inv[] = {
        { "Benzaldehyde",        50.0f,  10.0f },
        { "Benzyl acetate",      25.0f,   5.0f },
        { "Butyric acid",        10.0f,   2.0f },
        { "Cinnamaldehyde",      50.0f,  10.0f },
        { "Cyclotene",           15.0f,   3.0f },
        { "Delta-decalactone",   20.0f,   5.0f },
        { "Diacetyl",            10.0f,   2.0f },
        { "Ethyl cinnamate",     15.0f,   3.0f },
        { "Ethyl maltol",       100.0f,  20.0f },
        { "Eugenol",             30.0f,   5.0f },
        { "Furaneol",             5.0f,   1.0f },
        { "Gamma-undecalactone", 15.0f,   3.0f },
        { "Maltol",             100.0f,  20.0f },
        { "Sotolon",              2.0f,   0.5f },
        { "Vanillin",           200.0f,  40.0f },
    };
    static const int NCOMP = 15;

    sqlite3_stmt* stmt = NULL;
    int rc;
    int i;

    rc = db_exec_simple("BEGIN;");
    if (rc != SQLITE_OK) return rc;

    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR IGNORE INTO compound_inventory "
        "(compound_library_id, stock_grams, reorder_threshold_grams) "
        "SELECT id, ?, ? FROM compound_library WHERE compound_name = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    for (i = 0; i < NCOMP; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_double(stmt, 1, (double)inv[i].stock);
        sqlite3_bind_double(stmt, 2, (double)inv[i].reorder);
        sqlite3_bind_text  (stmt, 3, inv[i].name, -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Inventory seed error: %s\n", sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            db_exec_simple("ROLLBACK;");
            return rc;
        }
    }

    sqlite3_finalize(stmt);
    rc = db_exec_simple("COMMIT;");
    if (rc != SQLITE_OK) return rc;

    printf("Inventory seeded: %d compounds.\n", NCOMP);
    return 0;
}

/* =========================================================================
   db_cost_batch
   ========================================================================= */
int db_cost_batch(BatchRun* br)
{
    sqlite3_stmt* stmt = NULL;
    int rc;
    int i;
    float total = 0.0f;
    int   has_all_costs = 1;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT cost_per_gram FROM compound_library WHERE compound_name = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    for (i = 0; i < br->ingredient_count; i++) {
        double cpg = 0.0;
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, br->ingredients[i].compound_name,
                          -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            cpg = sqlite3_column_double(stmt, 0);

        if (cpg > 0.0) {
            br->ingredients[i].cost_line =
                br->ingredients[i].grams_needed * (float)cpg;
            total += br->ingredients[i].cost_line;
        } else {
            br->ingredients[i].cost_line = -1.0f;
            has_all_costs = 0;
        }
    }

    sqlite3_finalize(stmt);
    br->cost_total = has_all_costs ? total : -1.0f;
    return 0;
}

/* =========================================================================
   db_save_batch
   ========================================================================= */
int db_save_batch(const char* flavor_code,
                  int major, int minor, int patch,
                  BatchRun* br)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 formulation_id = 0;
    sqlite3_int64 batch_run_id   = 0;
    int rc;
    int i;

    /* Look up formulation DB id */
    rc = sqlite3_prepare_v2(g_db,
        "SELECT id FROM formulations "
        "WHERE flavor_code = ? "
        "  AND ver_major = ? AND ver_minor = ? AND ver_patch = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }
    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 2, major);
    sqlite3_bind_int (stmt, 3, minor);
    sqlite3_bind_int (stmt, 4, patch);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        fprintf(stderr, "Batch: formulation %s v%d.%d.%d not found.\n",
                flavor_code, major, minor, patch);
        return 1;
    }
    formulation_id = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    /* Auto-generate batch_number if not set */
    if (!br->batch_number[0]) {
        int seq = 1;
        rc = sqlite3_prepare_v2(g_db,
            "SELECT COUNT(*) FROM batch_runs br "
            "JOIN formulations f ON f.id = br.formulation_id "
            "WHERE f.flavor_code = ?;",
            -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_ROW)
                seq = sqlite3_column_int(stmt, 0) + 1;
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
        snprintf(br->batch_number, MAX_BATCH_NUMBER,
                 "%s-2026-%03d", flavor_code, seq);
    }

    rc = db_exec_simple("BEGIN;");
    if (rc != SQLITE_OK) return rc;

    /* Insert batch_run header */
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO batch_runs "
        "(formulation_id, batch_number, volume_liters, cost_total, notes) "
        "VALUES (?, ?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    sqlite3_bind_int64(stmt, 1, formulation_id);
    sqlite3_bind_text (stmt, 2, br->batch_number, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 3, (double)br->volume_liters);
    if (br->cost_total >= 0.0f)
        sqlite3_bind_double(stmt, 4, (double)br->cost_total);
    else
        sqlite3_bind_null(stmt, 4);
    if (br->notes[0])
        sqlite3_bind_text(stmt, 5, br->notes, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(stmt, 5);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    stmt = NULL;

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Batch run insert error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    batch_run_id = sqlite3_last_insert_rowid(g_db);

    /* Insert batch_ingredients */
    rc = sqlite3_prepare_v2(g_db,
        "INSERT INTO batch_ingredients "
        "(batch_run_id, compound_name, grams_needed, cost_line) "
        "VALUES (?, ?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    for (i = 0; i < br->ingredient_count; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64 (stmt, 1, batch_run_id);
        sqlite3_bind_text  (stmt, 2, br->ingredients[i].compound_name,
                            -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, (double)br->ingredients[i].grams_needed);
        if (br->ingredients[i].cost_line >= 0.0f)
            sqlite3_bind_double(stmt, 4,
                                (double)br->ingredients[i].cost_line);
        else
            sqlite3_bind_null(stmt, 4);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Ingredient insert error: %s\n",
                    sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            db_exec_simple("ROLLBACK;");
            return rc;
        }
    }

    sqlite3_finalize(stmt);
    stmt = NULL;

    /* Fetch batched_at back from DB */
    rc = sqlite3_prepare_v2(g_db,
        "SELECT batched_at FROM batch_runs WHERE id = ?;",
        -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, batch_run_id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            strncpy(br->batched_at,
                    (const char*)sqlite3_column_text(stmt, 0), 31);
            br->batched_at[31] = '\0';
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }

    rc = db_exec_simple("COMMIT;");
    if (rc != SQLITE_OK) return rc;

    br->id             = (int)batch_run_id;
    br->formulation_id = (int)formulation_id;

    printf("Batch saved: %s  %.1f L", br->batch_number, br->volume_liters);
    if (br->cost_total >= 0.0f)
        printf("  flavor cost $%.4f", br->cost_total);
    printf("\n");
    return 0;
}

/* =========================================================================
   db_list_batches
   ========================================================================= */
int db_list_batches(const char* flavor_code)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT br.id, br.batch_number, "
        "       f.ver_major, f.ver_minor, f.ver_patch, "
        "       br.volume_liters, br.cost_total, br.batched_at "
        "FROM batch_runs br "
        "JOIN formulations f ON f.id = br.formulation_id "
        "WHERE f.flavor_code = ? "
        "ORDER BY br.batched_at ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, flavor_code, -1, SQLITE_STATIC);

    printf("\n--- Batch Runs: %s ---\n", flavor_code);
    printf("%-4s | %-18s | %-7s | %-8s | %-12s | Batched At\n",
           "ID", "Batch No", "Version", "Vol (L)", "Cost (USD)");
    printf("-----+--------------------+---------+----------+--------------+-----------------------\n");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int    id   = sqlite3_column_int(stmt, 0);
        const char* bn  = (const char*)sqlite3_column_text(stmt, 1);
        int    vmaj = sqlite3_column_int(stmt, 2);
        int    vmin = sqlite3_column_int(stmt, 3);
        int    vpat = sqlite3_column_int(stmt, 4);
        double vol  = sqlite3_column_double(stmt, 5);
        const char* bat = (const char*)sqlite3_column_text(stmt, 7);
        char   cost[14];
        char   ver[10];

        if (sqlite3_column_type(stmt, 6) == SQLITE_NULL)
            strncpy(cost, "--", sizeof(cost));
        else
            snprintf(cost, sizeof(cost), "$%.4f", sqlite3_column_double(stmt, 6));

        snprintf(ver, sizeof(ver), "%d.%d.%d", vmaj, vmin, vpat);

        printf("%-4d | %-18s | %-7s | %8.2f | %-12s | %s\n",
               id, bn ? bn : "", ver, vol, cost, bat ? bat : "");
    }

    sqlite3_finalize(stmt);
    printf("\n");
    return (rc == SQLITE_DONE) ? 0 : rc;
}

/* =========================================================================
   db_check_inventory
   ========================================================================= */
int db_check_inventory(const BatchRun* br)
{
    sqlite3_stmt* stmt = NULL;
    int rc;
    int i;
    int shortfalls = 0;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT ci.stock_grams "
        "FROM compound_inventory ci "
        "JOIN compound_library cl ON cl.id = ci.compound_library_id "
        "WHERE cl.compound_name = ?;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    for (i = 0; i < br->ingredient_count; i++) {
        double stock = 0.0;
        int    found = 0;

        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, br->ingredients[i].compound_name,
                          -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stock = sqlite3_column_double(stmt, 0);
            found = 1;
        }

        if (!found) {
            printf("  [INVENTORY] %s: not tracked — add to inventory.\n",
                   br->ingredients[i].compound_name);
        } else if (stock < (double)br->ingredients[i].grams_needed) {
            printf("  [LOW STOCK] %s: need %.4f g, have %.4f g (short %.4f g)\n",
                   br->ingredients[i].compound_name,
                   br->ingredients[i].grams_needed,
                   stock,
                   br->ingredients[i].grams_needed - (float)stock);
            shortfalls++;
        }
    }

    if (shortfalls == 0)
        printf("  Inventory check: all compounds sufficient.\n");

    sqlite3_finalize(stmt);
    return shortfalls;
}

/* =========================================================================
   db_deduct_inventory
   ========================================================================= */
int db_deduct_inventory(const BatchRun* br)
{
    sqlite3_stmt* stmt = NULL;
    int rc;
    int i;

    rc = sqlite3_prepare_v2(g_db,
        "UPDATE compound_inventory "
        "SET stock_grams = MAX(0, stock_grams - ?), "
        "    last_updated = DATETIME('now', 'localtime') "
        "WHERE compound_library_id = "
        "    (SELECT id FROM compound_library WHERE compound_name = ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    for (i = 0; i < br->ingredient_count; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_double(stmt, 1,
                            (double)br->ingredients[i].grams_needed);
        sqlite3_bind_text  (stmt, 2,
                            br->ingredients[i].compound_name,
                            -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Inventory deduct error: %s\n",
                    sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            return rc;
        }
    }

    sqlite3_finalize(stmt);
    printf("Inventory updated after batch %s.\n", br->batch_number);
    return 0;
}

/* =========================================================================
   db_list_inventory
   ========================================================================= */
int db_list_inventory(void)
{
    sqlite3_stmt* stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(g_db,
        "SELECT cl.compound_name, cl.cost_per_gram, "
        "       ci.stock_grams, ci.reorder_threshold_grams, ci.last_updated "
        "FROM compound_inventory ci "
        "JOIN compound_library cl ON cl.id = ci.compound_library_id "
        "ORDER BY cl.compound_name ASC;",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        return rc;
    }

    printf("\n--- Compound Inventory ---\n");
    printf("%-24s | %9s | %9s | %9s | %-6s | Last Updated\n",
           "Compound", "Stock (g)", "Reorder (g)", "$/g", "Status");
    printf("-------------------------+-----------+-----------+-----------+--------+---------------------\n");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const char* name    = (const char*)sqlite3_column_text(stmt, 0);
        double      cpg     = sqlite3_column_double(stmt, 1);
        double      stock   = sqlite3_column_double(stmt, 2);
        double      reorder = sqlite3_column_double(stmt, 3);
        const char* updated = (const char*)sqlite3_column_text(stmt, 4);
        const char* status;

        if (stock <= 0.0)
            status = "OUT   ";
        else if (stock <= reorder)
            status = "LOW   ";
        else
            status = "OK    ";

        printf("%-24s | %9.4f | %9.4f | %9.4f | %s | %s\n",
               name ? name : "",
               stock, reorder,
               cpg,
               status,
               updated ? updated : "");
    }

    sqlite3_finalize(stmt);
    printf("\n");
    return (rc == SQLITE_DONE) ? 0 : rc;
}
