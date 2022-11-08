cmake_minimum_required(VERSION 3.10)

option(BUILD_MPI "build with mpi" OFF)
option(BUILD_TESTS OFF)

project(ctmo_cdh VERSION 2.2.0)

include(${CMAKE_SOURCE_DIR}/cmake/compile_options.txt)

#-------------MPI----------------
message(STATUS "CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")

if (${BUILD_MPI})
    include(FindMPI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVEMPI")
    set(CMAKE_CXX_COMPILER ${MPI_CXX_COMPILER})
    message(STATUS "Switch to compiler: ${CMAKE_CXX_COMPILER}")
endif ()

#-------------DEPENDENCIES-----------------

# LAPACK (note: LAPACK implementation can be chosen by the user with -DBLA_VENDOR=...)
find_package(LAPACK REQUIRED)
message(STATUS "LAPACK LIBS = ${LAPACK_LIBRARIES}")

#Boost
find_package(Boost REQUIRED COMPONENTS mpi serialization filesystem system program_options)
message(STATUS "BOOST LIBS = ${Boost_LIBRARIES}")

#Armadillo
find_package(Armadillo REQUIRED)
message(STATUS "Armadillo LIBS = ${ARMADILLO_LIBRARIES}")

#Snappy (optional)
find_package(Snappy REQUIRED)
include(${CMAKE_SOURCE_DIR}/cmake/FindSnappy.cmake)
message(STATUS "Snappy LIBS = ${SNAPPY_LIBRARIES}")

# Link to exec
set(LIBRARIES_EXEC
    ${LAPACK_LIBRARIES}
    ${Boost_LIBRARIES}
    ${SNAPPY_LIBRARIES}
    ${ARMADILLO_LIBRARIES}
)

################################

# Executables

################################
set(EXECECUTABLES
        ctmo
        ctmo_dca
        ctmo_slmc
        )

foreach (executable ${EXECECUTABLES})
    list(APPEND ${executable}_src ${executable}.cpp)
    add_executable(${executable}
            ${${executable}_src}
            )
    message(STATUS ${${executable}_src})
endforeach ()

foreach (executable ${EXECECUTABLES})
    target_link_libraries(${executable} PRIVATE 
                          ${LIBRARIES_EXEC} 
                          compile_options
                          )
    target_include_directories(${executable} PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/ctmo/deps/spdlog/include>     
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/ctmo/deps/nlohmann_json>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/ctmo/deps>
    )

    install(TARGETS ${executable} EXPORT ${CMAKE_PROJECT_NAME}Targets
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            RUNTIME DESTINATION bin
            INCLUDES DESTINATION include
            )
endforeach ()

install(DIRECTORY ../include/ctmo
        DESTINATION include
        COMPONENT Devel
)


#--------------------end Executables ----------------------------------------------------------


