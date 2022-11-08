cmake_minimum_required(VERSION 3.10)
project(ctmo_cdh_test VERSION 2.2.0)

option(BUILD_TESTS "Build all tests." OFF) # Makes boolean 'test' available.

if (${BUILD_TESTS})

    include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/compile_options.txt)

     # Download and unpack googletest at configure time
    configure_file(CMakeLists.txt.in ${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
    if (result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif ()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
    if (result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif ()

    # Prevent overriding the parent project's compiler/linker
    # settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
            ${CMAKE_BINARY_DIR}/googletest-build
            EXCLUDE_FROM_ALL)

    # The gtest/gtest_main targets carry header search path
    # dependencies automatically when using CMake 2.8.11 or
    # later. Otherwise we have to add them here ourselves.
    if (CMAKE_VERSION VERSION_LESS 2.8.11)
        include_directories("${gtest_SOURCE_DIR}/include")
    endif ()
    
    set(TESTS
            ABC_H0Tests
            ABC_ModelTests
            FillingAndDoccTests
            Fourier_DCATests
            FourierTests
            GreenBinningTests
            GreenMatTests
            GreenTauTests
            HybFMAndTLocTests
            IOTests
            MarkovChainSquare2x2Tests
            MarkovChainTests
            UtilitiesTests
            SelfConsistencyTests
            MarkovChainTriangle2x2Tests
            MarkovChainTriangle2x2_SLMC_Tests
            MatrixTests
            ObservablesTests
            UTensorSimpleTests
            VerticesSimpleTests
            )

    find_package(LAPACK REQUIRED)
    find_package(Boost REQUIRED COMPONENTS mpi serialization filesystem system program_options)
    set(LIBRARIES_TESTS ${LAPACK_LIBRARIES} ${Boost_LIBRARIES} snappy armadillo gtest gtest_main)
    


    foreach (test ${TESTS})
        list(APPEND ${test}_src ${test}.cpp)
        add_executable(${test}
                ${${test}_src}
                )
    endforeach ()

    foreach (test ${TESTS})
        target_link_libraries(${test} PRIVATE 
                            ${LIBRARIES_TESTS} 
                            compile_options
                            )
        target_include_directories(${test} PUBLIC 
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/ctmo/deps/spdlog/include>     
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/ctmo/deps/nlohmann_json>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
            $<INSTALL_INTERFACE:include>
            $<INSTALL_INTERFACE:include/ctmo/deps>
        )

        add_test(NAME ${test} COMMAND ./${test})
    endforeach ()

endif ()


#--------------------end Testing ----------------------------------------------------------

