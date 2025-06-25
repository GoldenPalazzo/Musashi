EXENAME          = g68k

MAINFILES        = simulator.c
ifeq ($(WASM),1)
	MAINFILES += emg68k.c
else
	MAINFILES += g68k.c server.c
endif
MUSASHIFILES     = m68kcpu.c m68kdasm.c softfloat/softfloat.c
MUSASHIGENCFILES = m68kops.c
MUSASHIGENHFILES = m68kops.h
MUSASHIGENERATOR = m68kmake

EXE =
EXEPATH = ./

.CFILES   = $(MAINFILES) $(OSDFILES) $(MUSASHIFILES) $(MUSASHIGENCFILES)
.OFILES   = $(.CFILES:%.c=%.o)

NATIVE_CC = gcc
CC        = gcc
WARNINGS  = -Wall -Wextra -Wno-long-long -Wno-shift-count-overflow -pedantic
CFLAGS   ?= -std=gnu99 $(WARNINGS)
ifeq ($(DEBUG),1)
	CFLAGS   += -g -DDEBUG
endif
LFLAGS    = $(WARNINGS)
ifeq ($(WASM),1)
	CC = emcc
	LFLAGS += -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','stringToUTF8','setValue','HEAPU8'] \
				-sMODULARIZE=1 -sEXPORT_NAME="emG68k" -sALLOW_MEMORY_GROWTH=1 \
				-sENVIRONMENT=web -sNO_EXIT_RUNTIME=1 -sEXPORT_ES6=1 \
				-sEXPORTED_FUNCTIONS="['_malloc','_free','_emg68k_setup',\
				'_emg68k_reset','_emg68k_memcpy','_emg68k_execute_cycles',\
				'_emg68k_get_reg','_emg68k_set_reg']"
	TARGET = emg68k.js
else
	TARGET    = $(EXENAME)$(EXE)
endif


DELETEFILES = $(MUSASHIGENCFILES) $(MUSASHIGENHFILES) $(.OFILES) $(TARGET) $(MUSASHIGENERATOR)$(EXE)


all: $(TARGET)

clean:
	rm -f $(DELETEFILES)

$(TARGET): $(MUSASHIGENHFILES) $(.OFILES) Makefile
	$(CC) -o $(TARGET) $(.OFILES) $(CFLAGS) $(LFLAGS) -lm

$(MUSASHIGENCFILES) $(MUSASHIGENHFILES): $(MUSASHIGENERATOR)$(EXE)
	$(EXEPATH)$(MUSASHIGENERATOR)$(EXE)

$(MUSASHIGENERATOR)$(EXE):  $(MUSASHIGENERATOR).c
	$(NATIVE_CC) -o  $(MUSASHIGENERATOR)$(EXE)  $(MUSASHIGENERATOR).c
