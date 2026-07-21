#ifndef TESTS_DECKS_H
#define TESTS_DECKS_H

#include <stdio.h>

/*
 * Diagnostic deck locations for the tests.
 *
 * The CPU functional deck is tracked in this repository under
 * Site_Acceptance_Test/, so any checkout can run it.  The other diagnostic
 * decks (printermechanicaltest, control-program-cr, isolationcpu01) and the
 * .bin oracles still live only in the untracked ../DUMP1 scan drop, and the
 * tests that need those skip legitimately when it is absent.
 *
 * The funktionalcpu tests used to hardcode the ../DUMP1 path and so skipped
 * everywhere too, silently, even though the deck was sitting in the tree.
 * Resolve it here instead: prefer the tracked copy, fall back to ../DUMP1 if
 * it has been removed.
 */
static inline const char *deck_funktionalcpu_cap(void)
{
    static const char tracked[] = "Site_Acceptance_Test/funktionalcpu.cap";
    static const char dump1[]   = "../DUMP1/funktionalcpu.cap";
    FILE *probe = fopen(tracked, "rb");

    if (probe) {
        fclose(probe);
        return tracked;
    }
    return dump1;
}

#endif /* TESTS_DECKS_H */
