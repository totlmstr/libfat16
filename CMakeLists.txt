project(FAT16)
cmake_minimum_required(VERSION 3.2)

option(BUILD_EXAMPLES "Build the examples project as well" ON)

add_library(FAT16
    include/fat16/fat16.h
    src/fat16.cpp)

target_include_directories(FAT16 PUBLIC include)

if(WIN32)
    add_definitions(/D "_CRT_SECURE_NO_WARNINGS"
                    /D "_CRT_NONSTDC_NO_WARNINGS")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT BUILD_EXAMPLES)
    set(CMAKE_CXX_STANDARD 11)
else()
    include("${PROJECT_SOURCE_DIR}/cmake/GetStandard.cmake")
    get_standard_for_build()
    if(CPP17_SUPPORTED)
        set(STANDARD_NUM 17)
    elseif(CPP14_SUPPORTED)
        set(STANDARD_NUM 14)
    else()
        set(STANDARD_NUM 11)
    endif()
    if(WIN32)
        if(${STANDARD_NUM} GREATER 11)
            add_compile_options("${STANDARD_FLAG}")
        endif()
    else()
        list(APPEND CMAKE_CXX_FLAGS "${STANDARD_FLAG}")
    endif()

    set(CMAKE_CXX_STANDARD ${STANDARD_NUM})

    add_executable(FAT16_EXTRACT MACOSX_BUNDLE examples/extract.cpp)
    target_link_libraries(FAT16_EXTRACT PRIVATE FAT16)
    if(STANDARD_NUM GREATER 11)
        target_compile_definitions(FAT16_EXTRACT PRIVATE COMPILE_CPP${STANDARD_NUM}=1)
        if(UNIX)
            target_link_libraries(FAT16_EXTRACT PRIVATE stdc++fs)
        endif()
        target_compile_features(FAT16_EXTRACT PRIVATE cxx_std_${STANDARD_NUM})
    else()
        find_package(Boost COMPONENTS filesystem system REQUIRED)
        include_directories(${Boost_INCLUDE_DIRS})
        target_link_libraries(FAT16_EXTRACT PRIVATE ${Boost_LIBRARIES})
    endif()
endif()
