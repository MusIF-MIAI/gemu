#include <stdio.h>

int main(void)
{
    char name[32];

    printf("Ciao! Come ti chiami? ");
    scanf("%s", name);
    printf("Piacere, %s Io sono il GE-120!\n", name);
    printf("E` ora possibile spegnere il computer\n");

    return 0;
}
