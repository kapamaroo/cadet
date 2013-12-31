#ifndef __CAPER_PARSER_H__
#define __CAPER_PARSER_H__

#include "datatypes.h"

struct analysis_info;

#define SEMANTIC_ERRORS -2

void parse(const char *libcell, const char *chip_dim,
           const char *placement, const char *netlist,
           struct analysis_info *analysis);

#endif
