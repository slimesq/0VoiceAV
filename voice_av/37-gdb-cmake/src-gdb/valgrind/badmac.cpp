#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    char *p = (char *)malloc(10);
    char *pt = p;
    for(int i = 0; i < 10; i++)
    {
        p[i] = 'z';
    }
    
    delete p;

    pt[1] = 'x';
    free(pt);

    printf("end \n");

    return 0;
}