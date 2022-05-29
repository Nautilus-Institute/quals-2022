cmake_minimum_required(VERSION 3.0)

project(router)

option(USE_STANDALONE_ASIO "set ON to use standalone Asio instead of Boost.Asio" OFF)

add_library(router INTERFACE)

target_include_directories(router INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(router INTERFACE ${CMAKE_THREAD_LIBS_INIT})

# TODO 2020 when Debian Jessie LTS ends:
# Remove Boost system, thread, regex components; use Boost::<component> aliases; remove Boost target_include_directories
if(USE_STANDALONE_ASIO)
    target_compile_definitions(router INTERFACE ASIO_STANDALONE)
    find_path(ASIO_PATH asio.hpp)
    if(NOT ASIO_PATH)
        message(FATAL_ERROR "Standalone Asio not found")
    else()
        target_include_directories(router INTERFACE ${ASIO_PATH})
    endif()
else()
    find_package(Boost 1.53.0 COMPONENTS system thread REQUIRED)
    target_link_libraries(router INTERFACE ${Boost_LIBRARIES})
    target_include_directories(router INTERFACE ${Boost_INCLUDE_DIR})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        target_compile_definitions(router INTERFACE USE_BOOST_REGEX)
        find_package(Boost 1.53.0 COMPONENTS regex REQUIRED)
        target_link_libraries(router INTERFACE ${Boost_LIBRARIES})
        target_include_directories(router INTERFACE ${Boost_INCLUDE_DIR})
    endif()
endif()
if(WIN32)
    target_link_libraries(router INTERFACE ws2_32 wsock32)
endif()

# If Simple-Web-Server is not a sub-project:
if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT MSVC)
        add_compile_options(-std=c++11 -Wall -Wextra -O2 -fvisibility=hidden)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-Wthread-safety)
        endif()
    else()
        add_compile_options(/W1)
    endif()

    find_package(Boost 1.53.0 COMPONENTS system thread filesystem)
    if(Boost_FOUND)
        add_executable(server server.cpp)
        target_link_libraries(server router)
        target_link_libraries(server ${Boost_LIBRARIES})
        target_include_directories(server PRIVATE ${Boost_INCLUDE_DIR})
    endif()

    install(FILES asio_compatibility.hpp server_http.hpp client_http.hpp server_https.hpp client_https.hpp crypto.hpp utility.hpp status_code.hpp mutex.hpp flag_0.inc DESTINATION include/simple-web-server)
endif()

