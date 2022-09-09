# CP/M Archiver. Copyright 1983 Clifford Heath. See the LICENSE

SOURCE =	car.c cpm.c floppy.c
OBJECT =	car.o cpm.o floppy.o
HEADER = 	cpm.h

car:	$(OBJECT)
	cc -o car $(OBJECT)

$(SOURCE):	$(HEADER)
