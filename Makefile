EXEC    = ir
FILES        = libir.c ir.c

CFLAGS += -I./include -Wall
LDFLAGS += -lpthread

all: 
	$(CC) $(FILES) -o $(EXEC) $(CFLAGS) $(LDFLAGS)

clean:
	      rm $(EXEC)
romfs:
	$(ROMFSINST) ir /bin/ir	
