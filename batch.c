#include <stdio.h>
#include <string.h>
#include "batch.h"
#include "formulation.h"

/* =========================================================================
   batch_calculate
   ========================================================================= */
void batch_calculate(BatchRun* br, const Formulation* f, float volume_liters)
{
    int i;

    memset(br->ingredients, 0, sizeof(br->ingredients));
    br->volume_liters    = volume_liters;
    br->cost_total       = -1.0f;
    br->ingredient_count = 0;

    for (i = 0; i < f->compound_count && i < MAX_COMPOUNDS; i++) {
        strncpy(br->ingredients[i].compound_name,
                f->compounds[i].compound_name, 63);
        br->ingredients[i].compound_name[63] = '\0';

        /* ppm == mg/L for water-based beverages (density ~1 kg/L)
           grams = mg/L * L / 1000 */
        br->ingredients[i].grams_needed =
            (f->compounds[i].concentration_ppm * volume_liters) / 1000.0f;

        br->ingredients[i].cost_line = -1.0f;
        br->ingredient_count++;
    }
}

/* =========================================================================
   batch_print_manifest
   ========================================================================= */
void batch_print_manifest(const BatchRun* br)
{
    int i;

    printf("========================================\n");
    printf("  BATCH WEIGHING MANIFEST\n");
    printf("========================================\n");
    printf("  Batch No : %s\n", br->batch_number[0] ? br->batch_number : "(unsaved)");
    printf("  Volume   : %.2f L\n", br->volume_liters);
    if (br->batched_at[0])
        printf("  Date     : %s\n", br->batched_at);
    printf("----------------------------------------\n");
    printf("  %-24s  %10s  %12s\n", "Compound", "Grams", "Cost (USD)");
    printf("  %-24s  %10s  %12s\n",
           "------------------------", "----------", "------------");

    for (i = 0; i < br->ingredient_count; i++) {
        if (br->ingredients[i].cost_line >= 0.0f)
            printf("  %-24s  %10.4f  %12.4f\n",
                   br->ingredients[i].compound_name,
                   br->ingredients[i].grams_needed,
                   br->ingredients[i].cost_line);
        else
            printf("  %-24s  %10.4f  %12s\n",
                   br->ingredients[i].compound_name,
                   br->ingredients[i].grams_needed,
                   "--");
    }

    printf("  %-24s  %10s  %12s\n",
           "------------------------", "----------", "------------");

    if (br->cost_total >= 0.0f)
        printf("  %-24s  %10s  %12.4f\n", "TOTAL FLAVOR COST", "", br->cost_total);
    else
        printf("  %-24s  %10s  %12s\n",   "TOTAL FLAVOR COST", "", "--");

    printf("========================================\n\n");
}

/* =========================================================================
   batch_print_label
   Ingredients sorted descending by concentration (highest mass first),
   matching FDA labeling convention for flavor ingredients.
   ========================================================================= */

/* Indices into the Formulation's compound array, sorted by ppm desc */
static int cmp_ppm_desc(const void* a, const void* b,
                         const Formulation* f)
{
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    float pa = f->compounds[ia].concentration_ppm;
    float pb = f->compounds[ib].concentration_ppm;
    return (pb > pa) - (pb < pa);  /* descending */
}

/* Simple insertion-sort wrapper (no qsort_r on MSVC) */
static void sort_indices(int* idx, int n, const Formulation* f)
{
    int i, j, tmp;
    for (i = 1; i < n; i++) {
        tmp = idx[i];
        j   = i - 1;
        while (j >= 0 &&
               f->compounds[idx[j]].concentration_ppm <
               f->compounds[tmp].concentration_ppm)
        {
            idx[j + 1] = idx[j];
            j--;
        }
        idx[j + 1] = tmp;
    }
    (void)cmp_ppm_desc;  /* suppress unused-function warning */
}

void batch_print_label(const BatchRun* br, const Formulation* f)
{
    int idx[MAX_COMPOUNDS];
    int i;

    /* Build sorted index */
    for (i = 0; i < f->compound_count; i++) idx[i] = i;
    sort_indices(idx, f->compound_count, f);

    printf("========================================\n");
    printf("  NONEYA CRAFT SODA\n");
    printf("  PRODUCTION LABEL\n");
    printf("========================================\n");
    printf("  Flavor    : %s\n", f->flavor_name);
    printf("  Code      : %s\n", f->flavor_code);
    printf("  Version   : %d.%d.%d\n",
           f->version.major, f->version.minor, f->version.patch);
    printf("  Batch No  : %s\n",
           br->batch_number[0] ? br->batch_number : "(unsaved)");
    printf("  Volume    : %.1f L\n", br->volume_liters);
    if (br->batched_at[0])
        printf("  Date      : %s\n", br->batched_at);
    printf("  pH target : %.1f\n", f->target_ph);
    printf("  Brix      : %.1f\n", f->target_brix);
    if (br->cost_total >= 0.0f)
        printf("  Flavor cost: $%.4f\n", br->cost_total);
    printf("----------------------------------------\n");
    printf("  NATURAL FLAVOR INGREDIENTS:\n");
    printf("  (listed highest to lowest concentration)\n");
    for (i = 0; i < f->compound_count; i++) {
        int k = idx[i];
        printf("    %-24s  %.2f ppm\n",
               f->compounds[k].compound_name,
               f->compounds[k].concentration_ppm);
    }
    printf("========================================\n\n");
}
