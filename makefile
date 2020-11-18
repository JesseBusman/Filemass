CC       = gcc
CPPC     = g++
CFLAGS   = -ldl -pthread -lpthread -MMD -MP
CPPFLAGS = -std=c++17 -ldl -pthread -lpthread -lstdc++ -MMD -MP -Wall -Wextra
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

CC = gcc
CXX = gcc

build/%.c.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.cpp.o: %.cpp
	$(CPPC) $(CPPFLAGS) -c -o $@ $<

