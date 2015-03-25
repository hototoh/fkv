cmake_minimum_required(VERSION 2.8.4)
project(fkv)

add_definitions(-pthread)
add_definitions(-g -Wall -Wconversion -Woverflow)
add_definitions(-O3)
add_definitions(-msse4.2)
#add_definitions(-march=corei7)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c99")

if (APPLE)
    set(NETMAP_HEADERS /Users/tkk/dev/netmap/sys)
    include_directories(${NETMAP_HEADERS})
elseif(UNIX)
    set(NETMAP_HEADERS /home/tkk/lib/netmap/sys)
    include_directories(${NETMAP_HEADERS})
endif ()


#set(fkv_SRC fkv.c)
set(city_SRC city.c)
set(mem_alloc_SRC mem_alloc.c)
set(test_mem_alloc_SRC test_mem_alloc.c)

set(circular_log_SRC circular_log.c)
set(test_circular_log_SRC test_circular_log.c)

#add_executable(fkv ${fkv_SRC})
add_executable(test_mem_alloc ${test_mem_alloc_SRC})
add_executable(test_circular_log ${test_circular_log_SRC})
add_library(city ${city_SRC})
add_library(mem_alloc ${mem_alloc_SRC})
add_library(circular_log ${circular_log_SRC})
target_link_libraries(test_mem_alloc mem_alloc)
target_link_libraries(test_circular_log circular_log mem_alloc city)
