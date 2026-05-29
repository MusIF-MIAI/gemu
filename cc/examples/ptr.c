/* address-of + write-through ; expect 123 */
int main(void) { int x; int *p; p = &x; *p = 123; return x; }
