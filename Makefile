# Just a basic makefile to quickly test that everyting is working, it just
# compiles the .o and the generator

EXENAME = emsim

MAINFILES        = simulator.c
MUSASHIFILES     = m68kcpu.c m68kdasm.c softfloat/softfloat.c
MUSASHIGENCFILES = m68kops.c
MUSASHIGENHFILES = m68kops.h
MUSASHIGENERATOR = m68kmake

# check if there is emscripten
EXE =
EXEPATH = ./

.CFILES   = $(MAINFILES) $(OSDFILES) $(MUSASHIFILES) $(MUSASHIGENCFILES)
.OFILES   = $(.CFILES:%.c=%.o)

NATIVE_CC = gcc
CC        = $(NATIVE_CC)
WARNINGS  = -Wall -Wextra -pedantic
CFLAGS    = $(WARNINGS)
LFLAGS    = $(WARNINGS)


ifeq ($(EMSCRIPTEN),1)
	CC = emcc
	LFLAGS += -sEXPORTED_RUNTIME_METHODS="['ccall','cwrap','stringToUTF8',\
				'setValue','HEAPU8','addFunction']" \
				-sMODULARIZE=1 -sEXPORT_NAME="emsim" -sALLOW_MEMORY_GROWTH \
				-sENVIRONMENT=web -sNO_EXIT_RUNTIME=1 -sEXPORT_ES6=1 \
				-lembind -sALLOW_TABLE_GROWTH=1 --emit-tsd $(EXENAME).d.ts\
				-sEXPORTED_FUNCTIONS="['_malloc','_free','_int_controller_set',\
				'_int_controller_clear','_setup','_step','_execute',\
				'_get_instruction_info','_get_reg','_set_reg','_cp_to_ram',\
				'_cp_from_ram','_reset']"
	TARGET = wasm/$(EXENAME).js
else
	TARGET = $(EXENAME)$(EXE)
endif



DELETEFILES = $(MUSASHIGENCFILES) $(MUSASHIGENHFILES) $(.OFILES) $(TARGET) $(MUSASHIGENERATOR)$(EXE)


all: $(TARGET)

clean:
	rm -f $(DELETEFILES)

$(TARGET): $(MUSASHIGENHFILES) $(.OFILES) Makefile
	mkdir -p wasm
	$(CC) -o $@ $(.OFILES) $(LFLAGS) -lm

m68kcpu.o: $(MUSASHIGENHFILES) m68kfpu.c m68kmmu.h softfloat/softfloat.c softfloat/softfloat.h

$(MUSASHIGENCFILES) $(MUSASHIGENHFILES): $(MUSASHIGENERATOR)$(EXE)
	$(EXEPATH)$(MUSASHIGENERATOR)$(EXE)

$(MUSASHIGENERATOR)$(EXE):  $(MUSASHIGENERATOR).c
	$(NATIVE_CC) -o  $(MUSASHIGENERATOR)$(EXE)  $(MUSASHIGENERATOR).c
