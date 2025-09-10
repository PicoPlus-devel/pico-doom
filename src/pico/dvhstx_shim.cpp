#include "dvhstx.hpp"
#include "dvhstx_shim.h"

#include "pico/platform.h"

static pimoroni::DVHSTX display; 
void hstx_setup(void (*fn)()) {
    display.set_release_fn(fn);
    display.init(640, 480, pimoroni::DVHSTX::MODE_RGB565_H2X, {HSTX_CKP, HSTX_D0P, HSTX_D1P, HSTX_D2P}); 
}
__scratch_x("display") dvhstx_line_data_t *hstx_try_get_empty_line() {
    return display.try_get_empty_line();
}
__scratch_x("display") void hstx_put_filled_line(dvhstx_line_data_t *line) {
    display.put_filled_line(line);
}
