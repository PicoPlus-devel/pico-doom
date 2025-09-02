#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include "dvhstx.h"

void hstx_setup(void(*fn)());
dvhstx_line_data_t *hstx_try_get_empty_line();
void hstx_put_filled_line(dvhstx_line_data_t *line);

#if defined(__cplusplus)
}
#endif
