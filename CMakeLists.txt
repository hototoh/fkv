cmake_minimum_required(VERSION 2.8.4)
project(fkv)

add_definitions(-pthread)
#add_definitions(-g -Wall -Wconversion -Woverflow)
add_definitions(-g)
add_definitions(-O2)
add_definitions(-msse4.2)
add_definitions(-march=corei7)

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
set(util_SRC util.c)
set(city_SRC city.c)

set(shm_SRC shm.c)
set(test_shm_SRC test_shm.c)

set(mem_manager_SRC mem_manager.c)
set(test_mem_manager_SRC test_mem_manager.c)

set(bucket_SRC bucket.c)
set(test_bucket_SRC test_bucket.c)

set(circular_log_SRC circular_log.c)
set(test_circular_log_SRC test_circular_log.c)

set(localbenchmark_SRC localbenchmark.c)

#add_executable(fkv ${fkv_SRC})
add_executable(test_shm ${test_shm_SRC})
add_executable(test_mem_manager ${test_mem_manager_SRC})
add_executable(test_bucket ${test_bucket_SRC})
add_executable(test_circular_log ${test_circular_log_SRC})
add_executable(localbenchmark ${localbenchmark_SRC})
add_library(util ${util_SRC})
add_library(city ${city_SRC})
add_library(shm ${shm_SRC})
add_library(mem_manager ${mem_manager_SRC})
add_library(bucket ${bucket_SRC})
add_library(circular_log ${circular_log_SRC})
target_link_libraries(test_shm shm)
target_link_libraries(test_mem_manager mem_manager shm)
target_link_libraries(test_bucket bucket shm city)
target_link_libraries(test_circular_log circular_log mem_manager bucket shm city util)
target_link_libraries(localbenchmark pthread circular_log mem_manager bucket shm city util)
