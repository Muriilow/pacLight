SRC_FILES = Main.o Socket.o
FLAGS = -Wno-packed-bitfield-compat -g3 -Wall -Wextra -Wconversion -fsanitize=address,undefined
main: $(SRC_FILES)
	gcc $(FLAGS) $(SRC_FILES) -o pacLight 

Main.o: Main.c
	gcc $(FLAGS) -c Main.c 

Socket.o: Socket.c
	gcc $(FLAGS) -c Socket.c 

clean: 
	rm -f *.o pacLight 
