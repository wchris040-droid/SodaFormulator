#include <stdio.h>
#include <string.h>
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
   db_save_formulation
   ========================================================================= */
int db_save_formulation(const Formulation* f)
{
    sqlite3_stmt* stmt = NULL;
    sqlite3_int64 formulation_id;
    int rc;
    int i;

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
        "(formulation_id, compound_name, concentration_ppm) "
        "VALUES (?, ?, ?);",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Prepare error: %s\n", sqlite3_errmsg(g_db));
        db_exec_simple("ROLLBACK;");
        return rc;
    }

    for (i = 0; i < f->compound_count; i++) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64 (stmt, 1, formulation_id);
        sqlite3_bind_text  (stmt, 2, f->compounds[i].compound_name, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, (double)f->compounds[i].concentration_ppm);

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
