set(CMAKE_CXX_STANDARD 17)
set(BUILD_STATIC "ON")
set(MYSQLCONNCPP_ROOT_DIR "${frs_SOURCE_DIR}/contrib/mysql-connector-cpp")
if(NOT EXISTS "${MYSQLCONNCPP_ROOT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR " submodule contrib/mysql-connector-cpp is missing. To fix try run: \n git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS "FALSE")
add_subdirectory("${MYSQLCONNCPP_ROOT_DIR}" "${frs_BINARY_DIR}/contrib/mysql-connector-cpp")
add_library(mysqlconncpp_library INTERFACE)

target_link_libraries(mysqlconncpp_library INTERFACE
  connector
)

get_target_property(MYSQLCONNCPP_INCLUDE_DIR connector INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (mysqlconncpp_library SYSTEM BEFORE INTERFACE ${MYSQLCONNCPP_INCLUDE_DIR} ${MYSQLCONNCPP_ROOT_DIR}/common)
