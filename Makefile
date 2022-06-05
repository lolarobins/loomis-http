CC=clang

build: loomis-http.c
	$(CC) -o loomis-http loomis-http.c