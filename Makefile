all:
	g++ torr.cpp lib_server.cpp lib_server.h lib_client.cpp lib_client.h -o torr.o -l sqlite3
	g++ tracker.cpp -o tracker.o -l sqlite3

clean:
	rm torr.o
	rm tracker.o