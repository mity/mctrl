# This Makefile expects mingw/msys environment and GNU Make.

.PHONY: all clean

INCDIR ?= . ../include
SRCDIR ?= .
BINDIR ?= ../bin
OBJDIR ?= ../obj/examples

GCC ?= gcc

CC = $(GCC) -c
LD = $(GCC)
WINDRES ?= windres
RM = rm -rf

override CPPFLAGS += -I../include
override CFLAGS += -Wall -g -O0
override LDFLAGS += -mwindows -L../lib
override LIBS = -lmCtrl -lcomctl32

EXAMPLES = $(BINDIR)/ex_button.exe          \
		   $(BINDIR)/ex_chart.exe           \
           $(BINDIR)/ex_dllgetversion.exe   \
           $(BINDIR)/ex_expand.exe          \
           $(BINDIR)/ex_grid.exe            \
           $(BINDIR)/ex_html.exe            \
           $(BINDIR)/ex_mditab.exe          \
           $(BINDIR)/ex_menubar.exe         \
           $(BINDIR)/ex_propview.exe        \
           $(BINDIR)/ex_treelist.exe        \
           $(BINDIR)/ex_treelist2.exe

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

$(BINDIR)/ex_button.exe: $(OBJDIR)/ex_button.o $(OBJDIR)/ex_button_rc.o
$(BINDIR)/ex_chart.exe: $(OBJDIR)/ex_chart.o $(OBJDIR)/ex_chart_rc.o
$(BINDIR)/ex_dllgetversion.exe: $(OBJDIR)/ex_dllgetversion.o $(OBJDIR)/ex_manifest_rc.o
$(BINDIR)/ex_expand.exe: $(OBJDIR)/ex_expand.o $(OBJDIR)/ex_expand_rc.o
$(BINDIR)/ex_grid.exe: $(OBJDIR)/ex_grid.o $(OBJDIR)/ex_grid_rc.o
$(BINDIR)/ex_html.exe: $(OBJDIR)/ex_html.o $(OBJDIR)/ex_html_rc.o
$(BINDIR)/ex_menubar.exe: $(OBJDIR)/ex_menubar.o $(OBJDIR)/ex_menubar_rc.o
$(BINDIR)/ex_mditab.exe: $(OBJDIR)/ex_mditab.o $(OBJDIR)/ex_mditab_rc.o
$(BINDIR)/ex_propview.exe: $(OBJDIR)/ex_propview.o $(OBJDIR)/ex_manifest_rc.o
$(BINDIR)/ex_treelist.exe: $(OBJDIR)/ex_treelist.o $(OBJDIR)/ex_manifest_rc.o
$(BINDIR)/ex_treelist2.exe: $(OBJDIR)/ex_treelist2.o $(OBJDIR)/ex_treelist2_rc.o


#################
# Generic rules #
#################

$(BINDIR)/%.exe:
	$(LD) $(LDFLAGS) $^ -o $@ $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(wildcard $(SRCDIR)/%.h)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

$(OBJDIR)/%_rc.o: $(SRCDIR)/%.rc $(wildcard res/*) $(wildcard $(SRCDIR)/%.h)
	$(WINDRES) $(filter-out -DUNICODE -D_UNICODE -DUNICODE=1 -D_UNICODE=1, $(CPPFLAGS)) $< $@

