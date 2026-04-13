CC = gcc
CFLAGS = -pthread
LIBS = -lrt

SRC = shared.c dialog.c message.c receiver.c main.c
OBJ = $(SRC:.c=.o)

all: chat

chat: $(OBJ)
	$(CC) $(CFLAGS) -o chat $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

clean:
	rm -f $(OBJ) chat