// step1.c
#include <stdio.h>

int func1(int a, int b)
{
    int c = a + b;
    c += 2;
    return c;
}

int func2(int p, int q)
{
    int t = q * p;
    return t * t;
}

int func3(int m, int n)
{
    return m + n;
}

int main()
{
    int c;
    c = func3(func1(1, 2), func2(8, 9));
    printf("c=%d.\n", c);
    return 0;
}
