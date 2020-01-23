cmake_minimum_required(VERSION 3.9.4)

project(dprun
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)

if (NOT MSVC)
  add_compile_options(-m32)
  add_compile_options(-Wall)
endif()
add_definitions(-DWIN32_LEAN_AND_MEAN)
add_definitions(-DNOMINMAX)
if (NOT CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DNDEBUG)
endif()

include_directories(include/)

add_library(service-provider SHARED)
target_link_libraries(service-provider
  PRIVATE ole32
          uuid
          ws2_32
)
target_sources(service-provider
  PRIVATE debug.c
          dll/dpsp.c
          dll/main.c
)
set_target_properties(service-provider PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dprun"
    SUFFIX ".dll"
    POSITION_INDEPENDENT_CODE OFF)

add_executable(cli-app)
target_link_libraries(cli-app
  PRIVATE dplayx
          ole32
          uuid
)
target_include_directories(cli-app
  PRIVATE third_party
)
target_sources(cli-app
  PRIVATE debug.c
          cli/dpsp.c
          cli/dpwrap.c
          cli/main.c
          cli/session.c
)
set_target_properties(cli-app PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dprun"
    SUFFIX ".exe"
    POSITION_INDEPENDENT_CODE OFF)

add_executable(enumerate-app)
target_link_libraries(enumerate-app
  PRIVATE dplayx
          ole32
          uuid
)
target_sources(enumerate-app
  PRIVATE debug.c
          cli/dpenumerate.c
          cli/dpwrap.c
)
set_target_properties(enumerate-app PROPERTIES
    PREFIX ""
    OUTPUT_NAME "dpenumerate"
    SUFFIX ".exe"
    POSITION_INDEPENDENT_CODE OFF)

add_custom_target(test-host
  COMMAND wine ./dprun.exe --host
               -p 'Host username'
               --application '{5BFDB060-06A4-11d0-9C4F-00A0C905425E}'
               --service-provider TCPIP -r
)
add_dependencies(test-host service-provider cli-app)
add_custom_target(test-join
  COMMAND wine ./dprun.exe --join `cat dbg_sessid.txt`
               -p 'Join username'
               --application '{5BFDB060-06A4-11d0-9C4F-00A0C905425E}'
               --service-provider TCPIP
               --address INet=127.0.0.1 -r
)
add_dependencies(test-join service-provider cli-app)
