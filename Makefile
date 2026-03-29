SRC_FILES = Main.o 

main: $(SRC_FILES)
	gcc -g3 -Wall -Wextra -Wconversion -fsanitize=address,undefined $(SRC_FILES) -o pacLight 

Main.o: Main.c
	gcc -g3 -Wall -Wextra -Wconversion -fsanitize=address,undefined -c Main.c 

clean: 
	rm -f *.o pacLight 
