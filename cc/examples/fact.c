/* recursion + multiply ; fact(5) == 120 */
int fact(int n) { if (n < 2) return 1; return n * fact(n - 1); }
int main(void) { return fact(5); }
