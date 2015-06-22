## TODO : remove hardcoding for mtcp.

.PHONY: all clean $(subdirs)

TARGETS = libfkv fkv_server fkv_client localbenchmark
CC = gcc -g -O3 -std=c99

# DPDK LIBRARY and HEADER
DPDK_INC=/home/tkk/dev/mtcp/dpdk/include
DPDK_LIB=/home/tkk/dev/mtcp/dpdk/lib

# mtcp library and header 
MTCP_FLD    =../mtcp
MTCP_INC    =-I${MTCP_FLD}/include
MTCP_LIB    =-L${MTCP_FLD}/lib
MTCP_TARGET = ${MTCP_LIB}/libmtcp.a

# cityhash library and header
CITY_FLD    =../cityhash
CITY_INC	=-I${CITY_FLD}/include
CITY_LIB	=-L${CITY_FLD}/lib
CITY_TARGET	= ${CITY_LIN}/libcity.a

#UTIL_FLD = ../../util
#UTIL_INC = -I${UTIL_FLD}/include
#UTIL_OBJ = ${UTIL_FLD}/http_parsing.o ${UTIL_FLD}/tdate_parse.o

# fkv sources
FKV_FLD 	=./
#FKV_INC		=-I./${FKV_FLD}/include
#FKV_LIB		=-L./${FKV_FLD}/lib
FKV_TARGET	=./${FKV_FLD}/libfkv.a
FKV_SRC	 	= bucket.c circular_log.c mem_manager.c shm.c
FKV_OBJS	= bucket.o circular_log.o mem_manager.o shm.o

INC =  ${MTCP_INC} ${CITY_INC}
LIBS = ${MTCP_LIB} ${CITY_LIB}

# CFLAGS for DPDK-related compilation
DPDK_MACHINE_FLAGS = $(shell cat /home/tkk/dev/mtcp/dpdk/include/cflags.txt)
INC += ${DPDK_MACHINE_FLAGS} -I${DPDK_INC} -include $(DPDK_INC)/rte_config.h

LIBS += -m64 -g -O3 -pthread -lrt -march=native -Wl,-export-dynamic ${MTCP_TARGET} ${CITY_TARGET} ${FKV_TARGET} -L${DPDK_LIB} -Wl,-lnuma -Wl,-lmtcp -Wl,-lpthread -Wl,-lrt -Wl,-ldl -Wl,--whole-archive -Wl,-lrte_distributor -Wl,-lrte_kni -Wl,-lrte_pipeline -Wl,-lrte_table -Wl,-lrte_port -Wl,-lrte_timer -Wl,-lrte_hash -Wl,-lrte_lpm -Wl,-lrte_power -Wl,-lrte_acl -Wl,-lrte_meter -Wl,-lrte_sched -Wl,-lm -Wl,-lrt -Wl,--start-group -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_ip_frag -Wl,-lethdev -Wl,-lrte_malloc -Wl,-lrte_mempool -Wl,-lrte_ring -Wl,-lrte_eal -Wl,-lrte_cmdline -Wl,-lrte_cfgfile -Wl,-lrte_pmd_bond -Wl,-lrte_pmd_vmxnet3_uio -Wl,-lrte_pmd_virtio_uio -Wl,-lrte_pmd_i40e -Wl,-lrte_pmd_ixgbe -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ring -Wl,-lrt -Wl,-lm -Wl,-ldl -Wl,--end-group -Wl,--no-whole-archive


all: $(TARGETS)

libfkv: $(FKV_OBJS)
	@ar csvr $@.a $(FKV_OBJS)

fkv_server.o: fkv_server.c
	${CC} -c $< ${CFLAGS} ${INC} -o $@

fkv_server: fkv_server.o
	${CC} $< ${LIBS} -o $@

fkv_client.o: fkv_client.c
	${CC} -c $< ${CFLAGS} ${INC} -o $@

fkv_client: fkv_client.o
	${CC} $< ${LIBS} -o $@

clean:
	rm -f *~ *.o ${TARGETS} log_*

