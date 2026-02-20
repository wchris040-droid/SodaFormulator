#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "formulation.h"

Formulation* create_formulation(const char* flavor_code, const char* flavor_name) {
    Formulation* f = (Formulation*)malloc(sizeof(Formulation));
    
    strcpy(f->flavor_code, flavor_code);
    strcpy(f->flavor_name, flavor_name);
    f->version = create_version(1, 0, 0);
    f->compound_count = 0;
    f->target_ph = 0.0;
    f->target_brix = 0.0;
    
    return f;
}

void add_compound(Formulation* f, const char* compound_name, float concentration_ppm) {
    if (f->compound_count >= MAX_COMPOUNDS) {
        printf("Error: Maximum compounds reached\n");
        return;
    }
    
    strcpy(f->compounds[f->compound_count].compound_name, compound_name);
    f->compounds[f->compound_count].concentration_ppm = concentration_ppm;
    f->compound_count++;
}

void print_formulation(Formulation* f) {
    printf("\n=== %s (%s) ===\n", f->flavor_name, f->flavor_code);
    printf("Version: ");
    print_version(f->version);
    printf("\n");
    printf("Target pH: %.1f\n", f->target_ph);
    printf("Target Brix: %.1f\n", f->target_brix);
    printf("\nCompounds (%d):\n", f->compound_count);
    
    for (int i = 0; i < f->compound_count; i++) {
        printf("  - %-20s: %.1f ppm\n", 
               f->compounds[i].compound_name,
               f->compounds[i].concentration_ppm);
    }
    printf("\n");
}

void free_formulation(Formulation* f) {
    free(f);
}
