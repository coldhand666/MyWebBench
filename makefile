mywebbench:webbench.o socket.o
	gcc -g -o mywebbench webbench.o socket.o
webbench.o:webbench.c socket.h
	gcc -c -g webbench.c
socket.o:socket.c socket.h
	gcc -c -g socket.c
	
.PHONY:cleano cleanall
cleano:
	rm *.o
cleanall:
	rm *.o mywebbench
