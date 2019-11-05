all:httpd
CC=gcc -g -lpthread

httpd:main.o function.o head.h 
	$(CC)  -o  httpd main.o function.o
	
main.o:main.c head.h
	$(CC)  -c main.c 
	
function.o:function.c head.h 
	$(CC)  -c function.c  
	
clean:
	rm -rf  httpd *.o
