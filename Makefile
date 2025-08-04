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

CC        = gcc
WARNINGS  = -Wall -Wextra -pedantic
CFLAGS    = $(WARNINGS)
LFLAGS    = $(WARNINGS)


ifeq ($(EMSCRIPTEN),1)
	TARGET = $(EXENAME).js
else
	TARGET = $(EXENAME)$(EXE)
endif



DELETEFILES = $(MUSASHIGENCFILES) $(MUSASHIGENHFILES) $(.OFILES) $(TARGET) $(MUSASHIGENERATOR)$(EXE)


all: $(TARGET)

clean:
	rm -f $(DELETEFILES)

$(TARGET): $(MUSASHIGENHFILES) $(.OFILES) Makefile
	$(CC) -o $@ $(.OFILES) $(LFLAGS) -lm

m68kcpu.o: $(MUSASHIGENHFILES) m68kfpu.c m68kmmu.h softfloat/softfloat.c softfloat/softfloat.h

$(MUSASHIGENCFILES) $(MUSASHIGENHFILES): $(MUSASHIGENERATOR)$(EXE)
	$(EXEPATH)$(MUSASHIGENERATOR)$(EXE)

$(MUSASHIGENERATOR)$(EXE):  $(MUSASHIGENERATOR).c
	$(CC) -o  $(MUSASHIGENERATOR)$(EXE)  $(MUSASHIGENERATOR).c
