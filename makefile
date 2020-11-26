CC       = gcc
CPPC     = g++
CFLAGS   = -ldl -O3 -pthread -lpthread -MMD -MP
CPPFLAGS = -std=c++17 -O3 -ldl -pthread -lpthread -lstdc++ -lmagic -MMD -MP -Wall -Wextra
CFILES   = $(wildcard *.c)
CPPFILES = $(wildcard *.cpp)
COBJFILES = $(CFILES:.c=.c.o)
CPPOBJFILES = $(CPPFILES:.cpp=.cpp.o)
OBJFILES = $(addprefix build/,$(CPPOBJFILES) $(COBJFILES))

CDEPENDS = $(patsubst %.c,build/%.c.d,$(CFILES))
CPPDEPENDS = $(patsubst %.cpp,build/%.cpp.d,$(CPPFILES))

build/filemass: $(OBJFILES)
	$(CPPC) $(CPPFLAGS) -o $@ $(OBJFILES)

-include $(CDEPENDS)
-include $(CPPDEPENDS)

build/%.c.o: %.c makefile
	$(CC) $(CFLAGS) -c -o $@ $(word 1, $<)

build/%.cpp.o: %.cpp makefile
	$(CPPC) $(CPPFLAGS) -c -o $@ $(word 1, $<)
