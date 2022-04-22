set(CONCURRENCPP_ROOT_DIR "${frs_SOURCE_DIR}/contrib/concurrencpp")
if(NOT EXISTS "${CONCURRENCPP_ROOT_DIR}/CMakeLists.txt")
  message(FATAL_ERROR " submodule contrib/concurrencpp is missing. To fix try run: \n git submodule update --init --recursive")
endif()

set(BUILD_SHARED_LIBS "FALSE")
add_subdirectory("${CONCURRENCPP_ROOT_DIR}" "${frs_BINARY_DIR}/contrib/concurrencpp")
add_library(concurrencpp_library INTERFACE)

target_link_libraries(concurrencpp_library INTERFACE
  concurrencpp::concurrencpp
)

get_target_property(CONCURRENCPP_INCLUDE_DIR concurrencpp::concurrencpp INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories (concurrencpp_library SYSTEM BEFORE INTERFACE ${CONCURRENCPP_INCLUDE_DIR})
