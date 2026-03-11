

default: all

clean:
	rm *.exe
	
all: controller

controller: controller.c
	gcc -o controller controller.c  -llibmodbus -Wall -L /usr/local/lib

