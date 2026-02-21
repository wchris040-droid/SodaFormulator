#include <stdio.h>
#include <string.h>
#include "tasting.h"

/* =========================================================================
   tasting_create
   ========================================================================= */
void tasting_create(TastingSession* ts, int formulation_id, const char* taster)
{
    memset(ts, 0, sizeof(*ts));
    ts->id             = 0;
    ts->formulation_id = formulation_id;
    strncpy(ts->taster, taster, MAX_TASTER_NAME - 1);

    ts->overall_score   = -1.0f;
    ts->aroma_score     = -1.0f;
    ts->flavor_score    = -1.0f;
    ts->mouthfeel_score = -1.0f;
    ts->finish_score    = -1.0f;
    ts->sweetness_score = -1.0f;
}

/* =========================================================================
   tasting_print
   ========================================================================= */
static void print_score(const char* label, float score)
{
    if (score < 0.0f)
        printf("  %-18s  --\n", label);
    else
        printf("  %-18s  %.1f / 10\n", label, score);
}

void tasting_print(const TastingSession* ts)
{
    printf("----------------------------------------\n");
    printf("  Tasting Session  (ID: %d)\n", ts->id);
    printf("  Formulation ID : %d\n", ts->formulation_id);
    printf("  Taster         : %s\n", ts->taster[0] ? ts->taster : "unknown");
    if (ts->tasted_at[0])
        printf("  Date           : %s\n", ts->tasted_at);
    printf("  Scores:\n");
    print_score("Overall",   ts->overall_score);
    print_score("Aroma",     ts->aroma_score);
    print_score("Flavor",    ts->flavor_score);
    print_score("Mouthfeel", ts->mouthfeel_score);
    print_score("Finish",    ts->finish_score);
    print_score("Sweetness", ts->sweetness_score);
    if (ts->notes[0])
        printf("  Notes          : %s\n", ts->notes);
    printf("----------------------------------------\n");
}
