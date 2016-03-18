LDLIBS  := ${LDLIBS} -lrdmacm -libverbs -lpthread

server: server.c
	gcc server.c -O0 -ggdb -o server $(LDLIBS)

clean:
	rm -rf client server
