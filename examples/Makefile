# This Makefile expects mingw/msys environment and GNU Make.

.PHONY: all clean

INCDIR = . ../include
SRCDIR = .
BINDIR = ../bin
OBJDIR = ../obj/examples

CC = $(PREFIX)gcc -c
LD = $(PREFIX)gcc
WINDRES = $(PREFIX)windres
RM = rm -rf

CPPFLAGS = -I../include
CFLAGS = -Wall
LDFLAGS = -mwindows -L../lib
LIBS = -lmCtrl -lcomctl32

EXAMPLES = $(addprefix $(BINDIR)/ex_, button.exe dllgetversion.exe html.exe mditab.exe)

C_SOURCES = $(wildcard $(SRCDIR)/*.c)
RC_SOURCES = $(wildcard $(SRCDIR)/*.rc)
SOURCES = $(C_SOURCES) $(RC_SOURCES)
C_OBJECTS = $(addprefix $(OBJDIR)/, $(notdir $(addsuffix .o,$(basename $(C_SOURCES)))))
RC_OBJECTS = $(addprefix $(OBJDIR)/, $(notdir $(addsuffix _rc.o,$(basename $(RC_SOURCES)))))
OBJECTS = $(C_OBJECTS) $(RC_OBJECTS)


all: $(EXAMPLES)

clean: 
	$(RM) $(OBJECTS)
	$(RM) $(EXAMPLES)


################
# Dependencies #
################

$(BINDIR)/ex_button.exe: $(OBJDIR)/button.o $(OBJDIR)/button_rc.o
$(BINDIR)/ex_dllgetversion.exe: $(OBJDIR)/dllgetversion.o
$(BINDIR)/ex_html.exe: $(OBJDIR)/html.o $(OBJDIR)/manifest_rc.o
$(BINDIR)/ex_mditab.exe: $(OBJDIR)/mditab.o $(OBJDIR)/mditab_rc.o


#################
# Generic rules #
#################

$(BINDIR)/ex_%.exe: 
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJDIR)/%_rc.o: $(SRCDIR)/%.rc $(wildcard res/*)
	$(WINDRES) $(filter-out -DUNICODE -D_UNICODE -DUNICODE=1 -D_UNICODE=1, $(CPPFLAGS)) $< $@

