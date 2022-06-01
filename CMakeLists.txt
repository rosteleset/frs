cmake_minimum_required(VERSION 3.8)
project(frs)
set(TARGET_NAME "${PROJECT_NAME}")
set(CMAKE_CXX_STANDARD 20)
set(TRITON_CLIENT_DIR "$ENV{HOME}/triton-client/build/install" CACHE STRING "Директория Triton Client")

find_package(OpenCV REQUIRED)

list(APPEND TRITON_CLIENT_INCLUDE_DIRS ${TRITON_CLIENT_DIR}/include)
list(APPEND TRITON_CLIENT_LIBS ${TRITON_CLIENT_DIR}/lib/libhttpclient_static.a)

add_subdirectory(${PROJECT_SOURCE_DIR}/contrib)

list(APPEND SOURCE_CORE main.cpp singleton.h singleton.cpp frs_api.h frs_api.cpp tasks.h tasks.cpp)
add_executable(${TARGET_NAME} ${SOURCE_CORE})

# include directories
include_directories(${OpenCV_INCLUDE_DIRS} ${TRITON_CLIENT_INCLUDE_DIRS})

# specify which libraries to connect
if (UNIX)
  target_link_libraries(${TARGET_NAME} pthread)
endif()

target_link_libraries(${TARGET_NAME} ${OpenCV_LIBS} ${TRITON_CLIENT_LIBS} abseil_library concurrencpp_library cpr_library mysqlconncpp_library crow_library)
