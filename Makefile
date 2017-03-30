DIR_INC = ./include

all: web

web: web.c
	gcc -W -Wall -I$(DIR_INC) -lpthread -o web web.c

clean:
	rm web
