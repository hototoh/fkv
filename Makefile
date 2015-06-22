## TODO : remove hardcoding for mtcp.

subdirs := cityhash src

.PHONY: all clean $(subdirs)

CC = gcc -g -O3

all: $(subdirs) 

$(subdirs):
	$(MAKE) -C $@ $(MAKECMDGOALS)

clean: $(subdirs)

