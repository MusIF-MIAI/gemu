/**
 * @file  bit.h
 * @brief Bit manipulation helpers
 */

#ifndef bit_h
#define bit_h

#define BIT(V, X)       (  !!(V &  (1 << X)))
#define SET_BIT(R, X)   (R = (R |  (1 << X)))
#define RESET_BIT(R, X) (R = (R & ~(1 << X)))

/* uses manual notation */
/* [ 4 | 3 | 2 | 1 ] for 16 bits and [ 2 | 1 ] for 8 bits */
#define NIBBLE_MASK(X)  (0xf << ((X-1)*4))

#endif
