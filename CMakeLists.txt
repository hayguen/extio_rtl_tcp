cmake_minimum_required(VERSION 3.2.0)

# required to enable set_property(TARGET clsocket PROPERTY  MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

project(ExtIO_RTL_TCP)

### build options
# default build type: Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: " ${CMAKE_BUILD_TYPE} " - Version: " ${VERSION} " / " ${LIBVER})

if (NOT MSVC)
    message(ERROR "CMakeLists.txt does only support the MSVC compiler")
endif()


# allow overriding cmake options with standard variables - from a project including this one
cmake_policy(SET CMP0077 NEW)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CLSOCKET_DEP_ONLY ON)
set(CLSOCKET_EXAMPLES OFF)
add_subdirectory(clsocket)



# set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc /W3")
# get_filename_component(SDKPATH ${CMAKE_LINKER} DIRECTORY)
# find_program(LIBEXE lib HINTS ${SDKPATH} REQUIRED)

add_library(ExtIO_RTL_TCP SHARED
    src/ExtIO_RTL.cpp
    src/ExtIO_RTL.h
    src/ExtIO_RTL.rc
    src/LC_ExtIO_Types.h
    src/config_file.cpp
    src/config_file.h
    src/dllmain.cpp
    src/exports.def
    src/resource.h
    src/targetver.h
)
set_target_properties(ExtIO_RTL_TCP PROPERTIES PREFIX "")
set_property(TARGET ExtIO_RTL_TCP PROPERTY CXX_STANDARD 17)
set_property(TARGET ExtIO_RTL_TCP PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ExtIO_RTL_TCP PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded")
# set_property(TARGET ExtIO_RTL_TCP PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:>")
target_compile_definitions(ExtIO_RTL_TCP PUBLIC _CRT_SECURE_NO_WARNINGS)
# target_compile_definitions(ExtIO_RTL_TCP PRIVATE CLSOCKET_STATIC_LIB)

target_include_directories(ExtIO_RTL_TCP PRIVATE
    clsocket/src
    tomlplusplus/include
    librtlsdr/include
)

# override clsocket's runtime library
# set_property(TARGET clsocket_static PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# set_property(TARGET clsocket_static PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:>")
# set_property(TARGET clsocket_shared PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# set_property(TARGET clsocket_shared PROPERTY  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:>")

target_link_libraries(ExtIO_RTL_TCP PRIVATE clsocket_static)


# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

# file(GLOB SRC "*.cpp" extio.def resource.rc manifest.xml)
# include_directories("." "../Core")
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

# target_link_directories(ExtIO_SDDC PUBLIC "${LIBFFTW_LIBRARY_DIRS}")
# target_link_libraries(ExtIO_SDDC PUBLIC Setupapi.lib)

