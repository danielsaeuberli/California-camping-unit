# Host build: logic, tests and the LVGL simulator (SPEC 3.4). Same sources as the firmware.
cmake_minimum_required(VERSION 3.20)
project(camping_unit_host C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(COMP ${ROOT}/firmware/components)

option(CU_SIM "Build the LVGL/SDL simulator" ON)

include(FetchContent)
# Pinned versions (keep in step with firmware/main/idf_component.yml).
FetchContent_Declare(unity GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git GIT_TAG v2.6.0 GIT_SHALLOW TRUE)
FetchContent_Declare(cjson GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git GIT_TAG v1.7.18 GIT_SHALLOW TRUE)
FetchContent_GetProperties(unity)
if(NOT unity_POPULATED)
  FetchContent_Populate(unity)
endif()
FetchContent_GetProperties(cjson)
if(NOT cjson_POPULATED)
  FetchContent_Populate(cjson)
endif()

add_library(cjson STATIC ${cjson_SOURCE_DIR}/cJSON.c)
target_include_directories(cjson PUBLIC ${cjson_SOURCE_DIR})

# Platform-independent logic: identical to the ESP-IDF components.
set(LOGIC_COMPONENTS cu_common profile signals modules safety state_bus control)
set(LOGIC_SRC "")
set(LOGIC_INC ${COMP}/hal/include)
foreach(c ${LOGIC_COMPONENTS})
  file(GLOB s ${COMP}/${c}/*.c)
  list(APPEND LOGIC_SRC ${s})
  list(APPEND LOGIC_INC ${COMP}/${c}/include)
endforeach()
add_library(cu_logic STATIC ${LOGIC_SRC} ${CMAKE_CURRENT_LIST_DIR}/sim/hal_sim.c)
target_include_directories(cu_logic PUBLIC ${LOGIC_INC} ${CMAKE_CURRENT_LIST_DIR}/sim)
target_link_libraries(cu_logic PUBLIC cjson m)
target_compile_options(cu_logic PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)

# Tests (tests/unit, tests/safety), one executable per file.
enable_testing()
add_library(unity STATIC ${unity_SOURCE_DIR}/src/unity.c)
target_include_directories(unity PUBLIC ${unity_SOURCE_DIR}/src)
target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
add_library(test_support STATIC ${ROOT}/tests/support.c)
target_include_directories(test_support PUBLIC ${ROOT}/tests)
target_link_libraries(test_support PUBLIC cu_logic unity)
target_compile_definitions(test_support PUBLIC CU_PROFILE_DIR="${ROOT}/profiles")
file(GLOB TEST_FILES ${ROOT}/tests/unit/test_*.c ${ROOT}/tests/safety/test_*.c)
foreach(f ${TEST_FILES})
  get_filename_component(name ${f} NAME_WE)
  add_executable(${name} ${f})
  target_link_libraries(${name} test_support)
  target_compile_options(${name} PRIVATE -Wall -Wextra -Werror -Wno-unused-parameter)
  add_test(NAME ${name} COMMAND ${name})
endforeach()

# i18n: ui/i18n/de.json -> C table (no texts in C code, SPEC 9).
set(I18N_C ${CMAKE_CURRENT_BINARY_DIR}/i18n/i18n_de.c)
set(I18N_H ${CMAKE_CURRENT_BINARY_DIR}/i18n/i18n.h)
add_custom_command(
  OUTPUT ${I18N_C} ${I18N_H}
  COMMAND python3 ${ROOT}/tools/i18n_gen.py ${ROOT}/ui/i18n/de.json ${ROOT}/ui/i18n/en.json ${CMAKE_CURRENT_BINARY_DIR}/i18n
  DEPENDS ${ROOT}/tools/i18n_gen.py ${ROOT}/ui/i18n/de.json ${ROOT}/ui/i18n/en.json)
add_library(cu_i18n STATIC ${I18N_C})
target_include_directories(cu_i18n PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/i18n)
add_test(NAME i18n_keys COMMAND python3 ${ROOT}/tools/i18n_gen.py --check ${ROOT}/ui/i18n/de.json ${ROOT}/ui/i18n/en.json ${ROOT}/firmware/components/ui)

if(CU_SIM)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED sdl2)
  FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git GIT_TAG v9.2.2 GIT_SHALLOW TRUE)
  set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/sim/lv_conf.h CACHE STRING "" FORCE)
  set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
  set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)
  set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(lvgl)
  target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/sim)

  file(GLOB UI_SRC ${COMP}/ui/*.c ${ROOT}/ui/fonts/*.c)
  add_executable(camping_sim ${CMAKE_CURRENT_LIST_DIR}/sim/sim_main.c ${UI_SRC})
  target_include_directories(camping_sim PRIVATE ${COMP}/ui/include ${ROOT}/ui/fonts)
  target_link_libraries(camping_sim cu_logic cu_i18n lvgl ${SDL2_LIBRARIES} m)
  target_compile_definitions(camping_sim PRIVATE CU_PROFILE_DIR="${ROOT}/profiles")
endif()
