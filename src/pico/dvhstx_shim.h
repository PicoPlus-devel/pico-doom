#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef void(*line_fun_t)(void *cb_data, int line_num, uint32_t *data); 
void hstx_setup(line_fun_t gen_line);

#if defined(__cplusplus)
}
#endif
