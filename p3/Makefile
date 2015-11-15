CFLAGS = -Wall
HDRS = methods.h
OBJS = methods.o smallsh.o
SRCS = methods.c smallsh.c
EXES = smallsh

all: $(EXES)

%.o: %.c $(HDRS)
	gcc -c $< -o $@ $(CFLAGS)
	
$(EXES): $(OBJS) 
	gcc $(OBJS) -o $@ $(CFLAGS)

clean:
	rm -f $(OBJS) 
	rm -f $(EXE)
