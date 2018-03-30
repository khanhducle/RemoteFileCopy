# Makefile for CPE464 library
#
# You should modify this makefile to meet your needs and to meet the
# requirements of the assignment.  This makefile is only provided as
# an example.  You can use it as much as you want but using this makefile
# is not required.

CC = g++
CFLAGS = -g -Wall -Werror

OS = $(shell uname -s)
ifeq ("$(OS)", "Linux")
	LIBS1 = -lstdc++
endif

LIBS += -lstdc++

SRCS = $(shell ls *.c 2> /dev/null)
OBJS = $(shell ls *.c 2> /dev/null | sed s/\.c[p]*$$/\.o/ )
LIBNAME = $(shell ls *cpe464*.a)

ALL = rcopy server 
#ALL = SREJ
all: $(OBJS) $(ALL)

lib:
	make -f lib.mk

echo:
	@echo "Objects: $(OBJS)"
	@echo "LIBNAME: $(LIBNAME)"


rcopy: rcopy.c 
	@echo "-------------------------------"
	@echo "*** Linking $@ with library $(LIBNAME)... "
	$(CC) $(CFLAGS) -o $@ $^ srej.o networks.o window.o $(LIBNAME) $(LIBS)
	@echo "*** Linking Complete!"
	@echo "-------------------------------"

server: server.c
	@echo "-------------------------------"
	@echo "*** Linking $@ with library $(LIBNAME)... "
	$(CC) $(CFLAGS) -o $@ $^ srej.o networks.o window.o $(LIBNAME) $(LIBS)
	@echo "*** Linking Complete!"
	@echo "-------------------------------"


# clean targets for Solaris and Linux
clean: 
	@echo "-------------------------------"
	@echo "*** Cleaning Files..."
	rm -f *.o $(ALL)
	@echo "-------------------------------"
