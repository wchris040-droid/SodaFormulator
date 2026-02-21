#include <stdio.h>
#include <string.h>
#include "compound.h"
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
       requires_solubilizer, requires_inert_atm */
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
    } compounds[] = {
        { "Benzaldehyde",        "100-52-7",    2127, 100.0f,  20.0f,  80.0f, 106.12f, 3000.0f, 2.5f, 5.0f, "almond cherry maraschino",   "RT",   0, 0 },
        { "Benzyl acetate",      "140-11-4",    2135,  50.0f,   2.0f,  15.0f, 150.17f, 5900.0f, 3.0f, 5.0f, "jasmine fruity sweet",       "RT",   0, 0 },
        { "Butyric acid",        "107-92-6",    2221,   1.0f,   0.1f,   0.5f,  88.11f,    0.0f, 2.5f, 5.0f, "butter cheese rancid",       "RT",   0, 0 },
        { "Cinnamaldehyde",      "104-55-2",    2286,  50.0f,  10.0f,  40.0f, 132.16f, 1400.0f, 2.5f, 4.5f, "cinnamon spicy warm",        "RT",   0, 0 },
        { "Cyclotene",           "80-71-7",     2700,  10.0f,   1.0f,   5.0f, 112.13f, 5000.0f, 2.5f, 5.0f, "maple caramel roasted",      "RT",   0, 0 },
        { "Delta-decalactone",   "705-86-2",    2361,  20.0f,   1.0f,  10.0f, 170.25f,  200.0f, 3.0f, 5.5f, "peach cream coconut",        "RT",   1, 0 },
        { "Diacetyl",            "431-03-8",    2370,   5.0f,   0.5f,   3.0f,  86.09f,    0.0f, 2.0f, 5.0f, "butter cream",               "2-8C", 0, 0 },
        { "Ethyl cinnamate",     "103-36-6",    2430,  10.0f,   1.0f,   5.0f, 176.21f,  500.0f, 3.0f, 5.0f, "fruity cinnamon sweet",      "RT",   1, 0 },
        { "Ethyl maltol",        "4940-11-8",   3487, 150.0f,  10.0f,  80.0f, 140.14f,55000.0f, 2.5f, 5.0f, "cotton candy sweet",         "RT",   0, 0 },
        { "Eugenol",             "97-53-0",     2467,  20.0f,   2.0f,  10.0f, 164.20f, 2400.0f, 3.0f, 7.0f, "clove spicy warm",           "RT",   1, 0 },
        { "Furaneol",            "3658-77-3",   3174,   5.0f,   0.1f,   1.0f, 128.13f,50000.0f, 2.5f, 4.5f, "caramel strawberry sweet",   "2-8C", 0, 0 },
        { "Gamma-undecalactone", "104-67-6",    3091,  20.0f,   1.0f,  10.0f, 184.28f,  100.0f, 3.0f, 5.5f, "peach creamy fruity",        "RT",   1, 0 },
        { "Maltol",              "118-71-8",    2656, 200.0f,  20.0f, 100.0f, 126.11f, 8000.0f, 2.5f, 5.0f, "sweet caramel cotton candy", "RT",   0, 0 },
        { "Sotolon",             "28664-35-9",  3634,   0.5f,  0.01f,   0.3f, 128.15f,50000.0f, 2.5f, 5.0f, "maple caramel fenugreek",    "2-8C", 0, 0 },
        { "Vanillin",            "121-33-5",    3107, 150.0f,  20.0f, 100.0f, 152.15f,10000.0f, 2.0f, 5.0f, "vanilla sweet creamy",       "RT",   0, 0 },
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
        " requires_solubilizer, requires_inert_atm) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
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

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "Seed insert error: %s\n", sqlite3_errmsg(g_db));
            sqlite3_finalize(stmt);
            db_exec_simple("ROLLBACK;");
            return rc;
        }
    }

    sqlite3_finalize(stmt);

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
        "       requires_solubilizer, requires_inert_atm "
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

    c->requires_solubilizer = sqlite3_column_int(stmt, 13);
    c->requires_inert_atm   = sqlite3_column_int(stmt, 14);

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
