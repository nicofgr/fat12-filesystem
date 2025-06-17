run: main.c
	clang main.c -o saida.out

memory_debug: main.c
	clang -lgmp -lpthread -std=c99 -Wall -fsanitize=address -O0 main.c -o saida.out

debug: main.c
	clang -lgmp -lpthread -O0 main.c -o saida.out

release: main.c
	clang -lgmp -lpthread -std=c99 -Wall -Werror main.c -o saida.out

time:
	time ./saida.out
