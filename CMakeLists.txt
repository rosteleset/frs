cmake_minimum_required(VERSION 3.8)
project(frs)
set(CMAKE_CXX_STANDARD 17)

set(TARGET_NAME "${PROJECT_NAME}")
add_definitions(-DQT_NO_KEYWORDS)

find_package(Qt5 REQUIRED COMPONENTS Core Network Xml Sql)
find_package(OpenCV REQUIRED)
find_package(Wt REQUIRED Wt HTTP)

set(TRITON_CLIENT_DIR ~/triton_client)
list(APPEND TRITON_CLIENT_LIBS 
  ${TRITON_CLIENT_DIR}/lib/libhttpclient.so)

list(APPEND SOURCE_CORE main.cpp singleton.h singleton.cpp frs_api.h frs_api.cpp scheduler.h scheduler.cpp sql_pool.h sql_pool.cpp)
add_executable(${TARGET_NAME} ${SOURCE_CORE}) 

# include directories
include_directories(${OpenCV_INCLUDE_DIRS} ${TRITON_CLIENT_DIR}/include)

# specify which libraries to connect
if (UNIX)
  target_link_libraries(${TARGET_NAME} pthread)
endif()

target_link_libraries(${TARGET_NAME} Qt5::Core Qt5::Network Qt5::Xml Qt5::Sql Wt::Wt Wt::HTTP ${OpenCV_LIBS} ${TRITON_CLIENT_LIBS})



