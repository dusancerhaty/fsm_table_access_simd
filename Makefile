IDIR=.
CC=g++
CFLAGS=-I$(IDIR)

ODIR=obj

LIBS=-lpthread

_OBJ = fsm_table_access_simd.o ya_getopt.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

fsm_table_access_simd: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o

