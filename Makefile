DIR_INC = ./include

all: web

web: rio.o fastcgi.o web.o tpool.o
	g++ -lpthread -g -o web web.o rio.o fastcgi.o tpool.o

rio.o: rio.cpp
	g++ -g -I$(DIR_INC) -c rio.cpp

fastcgi.o: fastcgi.cpp
	g++ -g -I$(DIR_INC) -c fastcgi.cpp

web.o: web.cpp
	g++ -g -I$(DIR_INC) -c web.cpp

tpool.o: tpool.cpp
	g++ -g -I$(DIR_INC) -c tpool.cpp

clean:
	rm web *.o
