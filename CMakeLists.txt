cmake_minimum_required(VERSION 3.7)

include(cmake/HunterGate.cmake)
HunterGate(
        URL "https://github.com/ruslo/hunter/archive/v0.19.26.tar.gz"
        SHA1 "362b3c2fd8f0f50fc03cec5105be8fd4376ea886"
)
include(ExternalProject)

option(KEEP_SML_AT_LATEST "ensure that SML it at lastest master on each build" ON)
set(sml_update_command)
if (NOT KEEP_SML_AT_LATEST)
    set(sml_update_command UPDATE_COMMAND "")
endif ()


project(goblinz)
set(CMAKE_CXX_STANDARD 14)

#
# check out github.com/ruslo  projects hunter, sugar and polly - probably the greatest leaps forward in cmakery ever
#
hunter_add_package(Sugar)
include(${SUGAR_ROOT}/cmake/Sugar)
include(sugar_files)
include(sugar_include)

#
# doxygen?
#
find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
    include(sugar_doxygen_generate)
endif ()

#
# load up dependencies
#
hunter_add_package(Boost COMPONENTS system)
find_package(Boost CONFIG COMPONENTS system)
find_package(Threads)

#
# load up sml
#
set_property(DIRECTORY PROPERTY EP_PREFIX ${CMAKE_BINARY_DIR}/_deps)
externalproject_add(BoostSML
        GIT_REPOSITORY https://github.com/boost-experimental/sml.git
        GIT_TAG master
        ${sml_update_command}
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND "${CMAKE_COMMAND}" -E copy_directory <SOURCE_DIR>/include/boost/ <INSTALL_DIR>/include/boost
        )
add_library(boost_sml INTERFACE)
add_dependencies(boost_sml BoostSML)
externalproject_get_property(BoostSML INSTALL_DIR)
target_include_directories(boost_sml SYSTEM INTERFACE ${INSTALL_DIR}/include)
target_link_libraries(boost_sml INTERFACE Boost::boost)
add_library(Boost::sml ALIAS boost_sml)


sugar_files(SOURCE_FILES main.cpp)
add_executable(goblinz ${SOURCE_FILES})
target_link_libraries(goblinz Boost::system Boost::sml ${CMAKE_THREAD_LIBS_INIT})

if (DOXYGEN_FOUND)
    sugar_doxygen_generate(DEVELOPER TARGET goblinz DOXYTARGET gobliz-doc DOXYFILE ${SUGAR_ROOT}/examples/Doxyfile.in)
endif ()

add_custom_target(everything ALL DEPENDS goblinz)
if (TARGET gobliz-doc)
    add_dependencies(everything gobliz-doc)
endif ()
