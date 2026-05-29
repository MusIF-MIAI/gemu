/* global array + pointer decay ; a[4] = 4*4 == 16 */
int a[5];
int main(void) { int i; int *p; p = a; for (i = 0; i < 5; i = i + 1) a[i] = i * i; return p[4]; }
