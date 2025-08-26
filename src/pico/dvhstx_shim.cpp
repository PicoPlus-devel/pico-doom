#include "dvhstx.hpp"
#include "dvhstx_shim.h"

static pimoroni::DVHSTX display; 
void hstx_setup(line_fun_t gen_line) {
    display.set_callback(gen_line, &display);
    display.init(640, 240, pimoroni::DVHSTX::MODE_LINE_CALLBACK_RGB565, {HSTX_CKP, HSTX_D0P, HSTX_D1P, HSTX_D2P}); 
}
