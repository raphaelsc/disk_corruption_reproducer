CC=gcc
CFLAGS=-I.
DEPS =
BLKX_OBJ = blkx-linux.o
BLKX_DISCARD_OBJ = blkx-discard-linux.o

all: blkx blkx-discard

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

blkx: $(BLKX_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

blkx-discard: $(BLKX_DISCARD_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
