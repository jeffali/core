#ifndef CFENGINE_TOKYO_CHECK_H
#define CFENGINE_TOKYO_CHECK_H

#ifdef HAVE_CONFIG_H
#include  <config.h>
#endif

#include "array_map_priv.h"
#include "hash_map_priv.h"
#include "map.h"
#include "string_lib.h"
#include "logging_old.h"
#include "cf3.defs.h"

int CheckTokyoDBCoherence( const char *path );

#endif
