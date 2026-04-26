CC = gcc
CFLAGS = -Wno-packed-bitfield-compat -g3 -Wall -Wextra -Wconversion -fsanitize=address,undefined -IScripts
SRC_DIR = Scripts
OBJ_FILES = Main.o Socket.o Message.o

pacLight: $(OBJ_FILES)
	$(CC) $(CFLAGS) $(OBJ_FILES) -o pacLight

Main.o: $(SRC_DIR)/Main.c $(SRC_DIR)/Socket.h $(SRC_DIR)/Message.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/Main.c

Socket.o: $(SRC_DIR)/Socket.c $(SRC_DIR)/Socket.h $(SRC_DIR)/Message.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/Socket.c

Message.o: $(SRC_DIR)/Message.c $(SRC_DIR)/Message.h
	$(CC) $(CFLAGS) -c $(SRC_DIR)/Message.c

clean:
	rm -f *.o pacLight
