#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

#include <stdio.h>
#include <string.h>

#include "m68k.h"
#include "simulator.h"


int main(void)
{

}

//#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_setup(size_t screen_write_pointer)
{
    screen_write = (void (*)(size_t, unsigned int))screen_write_pointer;
    g68k_setup();
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_reset(void)
{
    g68k_reset();
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_copy_to_ram(unsigned int dest, const unsigned char* src, size_t size)
{
    g68k_copy_to_ram(dest, src, size);
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_copy_from_ram(unsigned char* dest, const size_t src, size_t size)
{
    g68k_copy_from_ram(dest, src, size);
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_execute_cycles(unsigned int cycles)
{
    g68k_execute_cycles(cycles);
}

EXTERN EMSCRIPTEN_KEEPALIVE unsigned int emg68k_get_reg(m68k_register_t reg)
{
    return m68k_get_reg(NULL, reg);
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_set_reg(m68k_register_t reg, unsigned int value)
{
    m68k_set_reg(reg, value);
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_clean_ram(void)
{
    g68k_clean_ram();
}
//#endif
