/* double recursion (R7 spill) ; fib(10) == 55 */
int fib(int n) { if (n < 2) return n; return fib(n - 1) + fib(n - 2); }
int main(void) { return fib(10); }
