//*****************************************************************************
// read_id-key.c
//
// uNabto Task
//
//*****************************************************************************


#ifndef __READKEYID_H__
#define __READKEYID_H__

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Nabto Includes
#include <unabto/unabto_app.h>
#include <unabto/unabto_common_main.h>
#include <unabto_version.h>

// Global statics
extern char nabtoId[];
extern char presharedKey[];



int8_t readIdAndKey(char *, char *);

#endif //  __READKEYID_H__

