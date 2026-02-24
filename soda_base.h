#ifndef SODA_BASE_H
#define SODA_BASE_H

#include "version.h"

#define MAX_BASE_CODE        16
#define MAX_BASE_NAME        64
#define MAX_BASE_COMPOUNDS   50
#define MAX_BASE_INGREDIENTS 30

typedef struct {
    char  compound_name[64];
    float concentration_ppm;
    int   compound_library_id;
} BaseCompound;

typedef struct {
    int   ingredient_id;
    char  ingredient_name[64];
    float amount;
    char  unit[16];
} BaseIngredient;

typedef struct {
    int            id;
    char           base_code[MAX_BASE_CODE];
    char           base_name[MAX_BASE_NAME];
    Version        version;
    float          yield_liters;
    char           notes[256];
    BaseCompound   compounds[MAX_BASE_COMPOUNDS];
    int            compound_count;
    BaseIngredient ingredients[MAX_BASE_INGREDIENTS];
    int            ingredient_count;
} SodaBase;

#endif /* SODA_BASE_H */
