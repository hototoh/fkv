subdirs := src 

.PHONY: all clean $(subdirs)

TARGETS = libcity
CC = gcc -g -O3
AR = ar
ARFLAG = csvr

# cityhash library and header
CITY_FLD    =.
CITY_SRC_DIR=${CITY_FLD}/src
CITY_INC	=${CITY_FLD}/include
CITY_LIB	=${CITY_FLD}/lib
CITY_TARGET	=${CITY_LIB}/libcity.a

INC = -I${CITY_INC}

LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic -Wl,-lnuma -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,--whole-archive -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive

SOURCES    = ${CITY_SRC_DIR}/city.c
OBJSROUCES = ${CITY_SRC_DIR}/city.o

all: $(TARGETS)

.c.o: ${SOURCES}
	${CC} -c $< ${CFLAGS} ${INC} -o $@

libcity: ${OBJSROUCES} ${SOURCES}
	${AR} ${ARFLAG} ${CITY_TARGET} ${OBJSROUCES}
	@rm -f ${OBJSROUCES}

clean:
	rm -f ${CITY_TARGET} 
