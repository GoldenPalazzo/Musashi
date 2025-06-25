#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

#include <stdio.h>
#include <string.h>

#include "m68k.h"
#include "simulator.h"


int main()
{

}

//#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_setup()
{
    g68k_setup();
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_reset()
{
    g68k_reset();
}

EXTERN EMSCRIPTEN_KEEPALIVE void emg68k_memcpy(unsigned int dest, const unsigned char* src, size_t size)
{
    g68k_memcpy(dest, src, size);
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

//#endif
