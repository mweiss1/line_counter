
all: plc 

plc: plc.c
	gcc -o plc plc.c
