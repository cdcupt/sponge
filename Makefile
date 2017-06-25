DIR_INC = ./include

all: web

web: rio.o fastcgi.o web.o
	g++ -Wall -lpthread -o web web.o rio.o fastcgi.o

rio.o: rio.cpp
	g++ -Wall -I$(DIR_INC) -c rio.cpp

fastcgi.o: fastcgi.cpp
	g++ -Wall -I$(DIR_INC) -c fastcgi.cpp

web.o: web.cpp
	g++ -Wall -I$(DIR_INC) -c web.cpp

clean:
	rm web *.o
