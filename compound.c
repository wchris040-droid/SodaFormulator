#include <stdio.h>
#include "compound.h"

/* =========================================================================
   compound_check_limit
   Returns 0 = within limits, 1 = exceeds max, 2 = no limit data
   ========================================================================= */
int compound_check_limit(const CompoundInfo* c, float ppm)
{
    if (c->max_use_ppm == 0.0f) return 2;
    if (ppm > c->max_use_ppm)   return 1;
    return 0;
}

/* =========================================================================
   compound_print
   Prints a formatted info card to stdout.
   ========================================================================= */
void compound_print(const CompoundInfo* c)
{
    printf("=== %s ===\n", c->compound_name);
    printf("CAS:  %-18s FEMA: %d\n", c->cas_number, c->fema_number);

    if (c->water_solubility == 0.0f)
        printf("MW:   %.2f g/mol      Water solubility: miscible\n",
            c->molecular_weight);
    else
        printf("MW:   %.2f g/mol      Water solubility: %.1f mg/L\n",
            c->molecular_weight, c->water_solubility);

    if (c->max_use_ppm == 0.0f)
        printf("Max:  none established   Rec range: %.2f - %.2f ppm\n",
            c->rec_min_ppm, c->rec_max_ppm);
    else
        printf("Max:  %.1f ppm          Rec range: %.1f - %.1f ppm\n",
            c->max_use_ppm, c->rec_min_ppm, c->rec_max_ppm);

    printf("pH stable: %.1f - %.1f\n", c->ph_stable_min, c->ph_stable_max);
    printf("Odor: %s\n", c->odor_profile);
    printf("Storage: %-16s Requires solubilizer: %s\n",
        c->storage_temp,
        c->requires_solubilizer ? "Yes" : "No");
    if (c->cost_per_gram > 0.0f)
        printf("Cost:    $%.4f / g\n", c->cost_per_gram);
}
