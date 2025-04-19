// naively taken from linux kernel 6.4 
/** 
    andrei alexandescu also mentions the same trick with some performance numbers
    https://www.youtube.com/watch?v=MGTTO_r0fYg&ab_channel=nineteenraccoons
    optimze by reducing branches, instead of two conditions for each loop, 
    this reduces down to one, speedup by 2.28x in fbstring
    this is called unsigned wraparound
    "modulo subtraction is injective"
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct qstr 
{
    char* name;
    unsigned int len;
};
unsigned name_to_int(struct qstr* qstr)
{
    const char *name = qstr->name;
    int len = qstr->len;
    unsigned n = 0;

    if (len > 1 && *name == '0')
        goto out;
    do {
        unsigned c = *name++ - '0';
        if (c > 9)
            goto out;
        if (n >= (~0U-9)/10)
            goto out;
        n *= 10;
        n += c;
    } while (--len > 0);
    return n;
out:
    return ~0U;
}

int main()
{
    struct qstr *qstr = malloc(sizeof(struct qstr));
    if (!qstr) return 1;

    // Define the numeric string literal
    const char* source = "123456789";
    qstr->len = strlen(source); // number of digits only
    qstr->name = malloc(qstr->len + 1); // allocate extra byte for null terminator
    if (!qstr->name)
    {
        free(qstr);
        return 1;
    }
    // Copy the string into the allocated memory (ensuring no overflow)
    strncpy(qstr->name, source, qstr->len);
    qstr->name[qstr->len] = '\0';

    printf("%u\n", name_to_int(qstr));

    free(qstr->name);
    free(qstr);
    return 0;
}