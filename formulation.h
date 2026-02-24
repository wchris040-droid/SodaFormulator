#ifndef FORMULATION_H
#define FORMULATION_H

#include "version.h"
#include "ingredient.h"
#include "soda_base.h"

#define MAX_FLAVOR_CODE 16
#define MAX_FLAVOR_NAME 64
#define MAX_COMPOUNDS 50

typedef struct {
    char compound_name[64];
    float concentration_ppm;
} FormulaCompound;

typedef struct {
    char flavor_code[MAX_FLAVOR_CODE];
    char flavor_name[MAX_FLAVOR_NAME];
    Version version;
    
    FormulaCompound compounds[MAX_COMPOUNDS];
    int compound_count;
    
    float target_ph;
    float target_brix;
    char  production_instructions[2048];
} Formulation;

#define MAX_FORM_BASES       10
#define MAX_FORM_INGREDIENTS 20

typedef struct {
    int   soda_base_id;
    char  base_name[MAX_BASE_NAME];
    float amount;
    char  unit[16];
} FormBase;

typedef struct {
    int   ingredient_id;
    char  ingredient_name[MAX_INGREDIENT_NAME];
    float amount;
    char  unit[16];
} FormIngredient;

Formulation* create_formulation(const char* flavor_code, const char* flavor_name);
void add_compound(Formulation* f, const char* compound_name, float concentration_ppm);
void print_formulation(Formulation* f);
void free_formulation(Formulation* f);

#endif
