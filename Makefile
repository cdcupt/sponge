DIR_INC = ./include

all: web

web: web.o
	g++ -Wall -lpthread -o web web.o

web.o: web.cpp
	g++ -Wall -I$(DIR_INC) -c web.cpp

clean:
	rm web *.o
