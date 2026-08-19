#ifndef UNITY_INTERNALS_H
#define UNITY_INTERNALS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef int UNITY_INT;
typedef unsigned int UNITY_UINT32;
typedef unsigned int UNITY_LINE_TYPE;

#ifndef UNITY_OUTPUT_CHAR
#define UNITY_OUTPUT_CHAR(a) putchar((a))
#endif

#endif
