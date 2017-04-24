DIR_INC = ./include

all: web

web: web.cpp
	g++ -Wall -I$(DIR_INC) -lpthread -o web web.cpp

clean:
	rm web
