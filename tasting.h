#ifndef TASTING_H
#define TASTING_H

#define MAX_TASTER_NAME   64
#define MAX_TASTING_NOTES 512

/*
 * TastingSession — one sensory evaluation of a saved formulation version.
 *
 * Scores are 1.0–10.0.  A value of -1.0 means "not scored" (optional fields).
 * overall_score is required.
 */
typedef struct {
    int   id;                          /* DB primary key (0 if unsaved)     */
    int   formulation_id;              /* FK → formulations.id              */
    char  tasted_at[32];               /* ISO datetime, filled by DB        */
    char  taster[MAX_TASTER_NAME];     /* evaluator name                    */

    float overall_score;               /* required, 1–10                    */
    float aroma_score;                 /* 1–10, or -1.0 = not scored        */
    float flavor_score;
    float mouthfeel_score;
    float finish_score;
    float sweetness_score;

    char  notes[MAX_TASTING_NOTES];    /* free-text sensory notes           */
} TastingSession;

/*
 * Zero-initialise a TastingSession and set the formulation_id and taster.
 * All optional scores default to -1.0 (not scored).
 */
void tasting_create(TastingSession* ts, int formulation_id, const char* taster);

/*
 * Print a formatted sensory report for a single session.
 */
void tasting_print(const TastingSession* ts);

#endif
