
#ifndef PARSE_FILE_H
#define PARSE_FILE_H

#include <stdio.h>

//student struct
typedef struct student {
    char id[10];
    char password[256];
    char grade[4];
} student_t;

//TA struct
typedef struct TA {
    char id[10];
    char password[256];
} TA_t;


#endif // PARSE_FILE_H
