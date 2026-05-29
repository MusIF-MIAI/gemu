/* sum 1..10 == 55 ; expect __rv = 55 */
int main(void) { int s; int i; s = 0; for (i = 1; i <= 10; i = i + 1) s = s + i; return s; }
