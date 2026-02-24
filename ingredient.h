#ifndef INGREDIENT_H
#define INGREDIENT_H

#define MAX_INGREDIENT_NAME     64
#define MAX_INGREDIENT_UNIT     16
#define MAX_INGREDIENT_CATEGORY 32
#define MAX_INGREDIENT_BRAND    64

typedef struct {
    int   id;
    char  ingredient_name[MAX_INGREDIENT_NAME];
    char  category[MAX_INGREDIENT_CATEGORY];
    char  unit[MAX_INGREDIENT_UNIT];
    float cost_per_unit;
    int   supplier_id;   /* 0 = none */
    char  brand[MAX_INGREDIENT_BRAND];
    char  notes[256];
} Ingredient;

#endif /* INGREDIENT_H */
