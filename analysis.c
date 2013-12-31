#include <math.h>
#include <assert.h>

#include "analysis.h"

void analyse(struct analysis_info *analysis) {
    assert(analysis);
    assert(analysis->wire_size != 0.0);

    unsigned long dim_x = ceil(analysis->chip.width / analysis->wire_size);
    unsigned long dim_y = ceil(analysis->chip.height / analysis->wire_size);
}
