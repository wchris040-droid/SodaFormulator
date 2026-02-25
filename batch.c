#include <stdio.h>
#include <string.h>
#include <math.h>
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

/* =========================================================================
   batch_generate_fda_label
   Generates an FDA 21 CFR 101 compliant consumer label into out_buf.
   ========================================================================= */
int batch_generate_fda_label(
    const char        *batch_number,
    const char        *flavor_name,
    const char        *flavor_code,
    const char        *version_str,
    float              target_brix,
    const LabelConfig *cfg,
    char              *out_buf,
    int                out_len)
{
    /* Nutrition calculations */
    double ml       = cfg->container_oz * 29.5735;
    double g_sugar  = (target_brix / 100.0) * ml * 1.04; /* density correction */
    int    cal_raw  = (int)(g_sugar * 4.0);
    int    calories = (cal_raw < 50) ? ((cal_raw + 2) / 5 * 5)
                                     : ((cal_raw + 5) / 10 * 10);
    int    g_carbs  = (int)(g_sugar + 0.5);
    int    dv_carbs = (int)((g_sugar / 275.0) * 100.0 + 0.5);
    int    dv_added = (int)((g_sugar / 50.0)  * 100.0 + 0.5);
    int    ml_int   = (int)(ml + 0.5);
    int    oz_int   = (int)(cfg->container_oz);

    /* Ingredients list */
    char ingredients[512];
    int  ilen = 0;

    ilen += snprintf(ingredients + ilen, (int)sizeof(ingredients) - ilen,
                     "Carbonated Water");
    if (cfg->sweetener_name[0])
        ilen += snprintf(ingredients + ilen, (int)sizeof(ingredients) - ilen,
                         ", %s", cfg->sweetener_name);
    if (cfg->acid_name[0])
        ilen += snprintf(ingredients + ilen, (int)sizeof(ingredients) - ilen,
                         ", %s", cfg->acid_name);
    ilen += snprintf(ingredients + ilen, (int)sizeof(ingredients) - ilen,
                     ", Natural Flavors.");

    /* Build output with running offset */
    int written = 0;

#define W(...) \
    if (written < out_len - 1) \
        written += snprintf(out_buf + written, out_len - written, __VA_ARGS__)

    W("%s\r\n", cfg->company_name);
    W("%s\r\n", cfg->company_address);
    W("\r\n");
    W("  %s Artisan Craft Soda\r\n", flavor_name);
    W("\r\n");
    W("  NET CONTENTS: %d FL OZ (%d mL)\r\n", oz_int, ml_int);
    W("\r\n");
    W("INGREDIENTS: %s\r\n", ingredients);
    W("\r\n");
    W("CONTAINS NO MAJOR ALLERGENS.\r\n");
    W("\r\n");
    W("========================================\r\n");
    W("Nutrition Facts\r\n");
    W("%d serving per container\r\n", cfg->servings_per_container);
    W("Serving size         %d fl oz (%d mL)\r\n", oz_int, ml_int);
    W("----------------------------------------\r\n");
    W("Calories                            %4d\r\n", calories);
    W("----------------------------------------\r\n");
    W("                  Amount/serving  %%DV*\r\n");
    W("Total Fat 0g                        0%%\r\n");
    W("  Saturated Fat 0g                  0%%\r\n");
    W("  Trans Fat 0g\r\n");
    W("Cholesterol 0mg                     0%%\r\n");
    W("Sodium 0mg                          0%%\r\n");
    W("Total Carbohydrate %dg            %3d%%\r\n", g_carbs, dv_carbs);
    W("  Dietary Fiber 0g                  0%%\r\n");
    W("  Total Sugars %dg\r\n", g_carbs);
    W("    Incl. %dg Added Sugars        %3d%%\r\n", g_carbs, dv_added);
    W("Protein 0g\r\n");
    W("----------------------------------------\r\n");
    W("Vit. D 0mcg 0%%    Calcium 0mg  0%%\r\n");
    W("Iron 0mg  0%%      Potassium 0mg 0%%\r\n");
    W("----------------------------------------\r\n");
    W("*%%DV based on 2,000 calorie diet.\r\n");
    W("\r\n");
    W("Manufactured by:\r\n");
    W("%s\r\n", cfg->company_name);
    W("%s\r\n", cfg->company_address);
    W("\r\n");
    W("Batch: %s\r\n", batch_number);
    W("Formulation: %s v%s\r\n", flavor_code, version_str);
    W("========================================\r\n");

#undef W

    if (written > 0 && written < out_len)
        out_buf[written] = '\0';
    else if (out_len > 0)
        out_buf[out_len - 1] = '\0';

    return 0;
}

/* =========================================================================
   batch_calculate_from_ingredients
   ========================================================================= */
void batch_calculate_from_ingredients(
    BatchRun             *br,
    const FormBase       *bases,    int base_count,
    const FormIngredient *ings,     int ing_count,
    float                 volume_liters)
{
    int i;
    memset(br->ingredients, 0, sizeof(br->ingredients));
    br->volume_liters    = volume_liters;
    br->cost_total       = -1.0f;
    br->ingredient_count = 0;

    for (i = 0; i < base_count && br->ingredient_count < MAX_COMPOUNDS; i++) {
        BatchIngredient *bi = &br->ingredients[br->ingredient_count];
        strncpy(bi->compound_name, bases[i].base_name, 63);
        bi->compound_name[63] = '\0';
        bi->grams_needed = (strcmp(bases[i].unit, "%") == 0)
            ? (bases[i].amount / 100.0f) * volume_liters
            : bases[i].amount;
        bi->cost_line = -1.0f;
        br->ingredient_count++;
    }

    for (i = 0; i < ing_count && br->ingredient_count < MAX_COMPOUNDS; i++) {
        BatchIngredient *bi = &br->ingredients[br->ingredient_count];
        strncpy(bi->compound_name, ings[i].ingredient_name, 63);
        bi->compound_name[63] = '\0';
        bi->grams_needed = (strcmp(ings[i].unit, "%") == 0)
            ? (ings[i].amount / 100.0f) * volume_liters
            : ings[i].amount;
        bi->cost_line = -1.0f;
        br->ingredient_count++;
    }
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
