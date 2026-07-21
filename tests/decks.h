#ifndef TESTS_DECKS_H
#define TESTS_DECKS_H

#include <stdio.h>

/*
 * Diagnostic deck locations for the tests.
 *
 * The scanned decks are available separately; ../DUMP1 is the working drop
 * where they live, and is the source of record -- look there first, so the
 * tests exercise the originals.  Site_Acceptance_Test/ is the fallback for an
 * environment without the drop.
 *
 * The funktionalcpu tests used to hardcode the ../DUMP1 path with no fallback
 * at all and returned early whenever the drop was not visible, printing
 * "[SKIP] ... not found" and passing -- reporting green coverage that never
 * ran.  Resolving here fixes that in both directions.
 */
static inline const char *deck_funktionalcpu_cap(void)
{
    static const char dump1[]    = "../DUMP1/funktionalcpu.cap";
    static const char fallback[] = "Site_Acceptance_Test/funktionalcpu.cap";
    FILE *probe = fopen(dump1, "rb");

    if (probe) {
        fclose(probe);
        return dump1;
    }
    return fallback;
}

#endif /* TESTS_DECKS_H */
