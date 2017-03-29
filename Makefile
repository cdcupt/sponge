all: web

web: web.c
	gcc -W -Wall -lpthread -o web web.c

clean:
	rm web
