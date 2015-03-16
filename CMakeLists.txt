cmake_minimum_required(VERSION 2.8.4)
project(fkv)

add_definitions("-Wall ")
add_definitions("-lpthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")

if (APPLE)
    set(NETMAP_HEADERS /Users/tkk/dev/netmap/sys)
    include_directories(${NETMAP_HEADERS})
elseif(UNIX)
    set(NETMAP_HEADERS /home/tkk/lib/netmap/sys)
    include_directories(${NETMAP_HEADERS})
endif ()

#set(fkv_SRC fkv.c)
set(mem_alloc_SRC mem_alloc.c)
set(test_mem_alloc_SRC test_mem_alloc.c)

#add_executable(fkv ${fkv_SRC})
add_executable(test_mem_alloc ${test_mem_alloc_SRC})
add_library(mem_alloc ${mem_alloc_SRC})
target_link_libraries(test_mem_alloc mem_alloc)
