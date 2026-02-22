#ifndef BATCH_H
#define BATCH_H

#include "formulation.h"

#define MAX_BATCH_NUMBER 32
#define MAX_BATCH_NOTES  256

typedef struct {
    char  compound_name[64];
    float grams_needed;
    float cost_line;       /* grams_needed * cost_per_gram; -1.0 = no price data */
} BatchIngredient;

typedef struct {
    int   id;                              /* DB primary key; 0 if unsaved       */
    int   formulation_id;                  /* FK -> formulations.id               */
    char  batch_number[MAX_BATCH_NUMBER];  /* e.g. "CINROLL-2026-001"             */
    float volume_liters;
    float cost_total;                      /* sum of cost_line; -1.0 = no data   */
    char  batched_at[32];                  /* filled by DB on save                */
    char  notes[MAX_BATCH_NOTES];

    BatchIngredient ingredients[MAX_COMPOUNDS];
    int   ingredient_count;
} BatchRun;

typedef struct {
    char  company_name[128];
    char  company_address[256]; /* single line: "123 Main St, Fresno, CA 93701" */
    float container_oz;         /* e.g. 12.0 */
    int   servings_per_container;
    char  sweetener_name[64];   /* "Cane Sugar", "HFCS-55", etc. */
    char  acid_name[64];        /* "Citric Acid", "" = omit from ingredients */
} LabelConfig;

/*
 * Generate an FDA 21 CFR 101 compliant consumer label into out_buf.
 * Nutrition Facts are calculated from target_brix and cfg->container_oz.
 * Returns 0 on success.
 */
int batch_generate_fda_label(
    const char        *batch_number,
    const char        *flavor_name,
    const char        *flavor_code,
    const char        *version_str,
    float              target_brix,
    const LabelConfig *cfg,
    char              *out_buf,
    int                out_len);

/*
 * Populate br->ingredients from f at the given volume.
 * Formula: grams = (concentration_ppm [mg/L] * volume_liters) / 1000
 * Sets volume_liters; clears cost fields to -1 (call db_cost_batch after).
 */
void batch_calculate(BatchRun* br, const Formulation* f, float volume_liters);

/*
 * Print a weighing manifest suitable for bench use.
 * Shows grams per compound and line cost if available.
 */
void batch_print_manifest(const BatchRun* br);

/*
 * Print a formatted production label.
 * Ingredients listed in descending order by concentration (= descending mass).
 */
void batch_print_label(const BatchRun* br, const Formulation* f);

#endif
