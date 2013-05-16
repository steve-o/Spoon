# CMake build script for Velocity Tcl plugin
# x64 Windows Server-only
# 2013/05/08 -- Steven.McCoy@thomsonreuters.com

cmake_minimum_required (VERSION 2.8)

project (Spoon)

set(SPOON_VERSION_MAJOR 2)
set(SPOON_VERSION_MINOR 1)
set(SPOON_VERSION_BUILD 8)

# Boost headers plus built libraries
set(BOOST_ROOT D:/boost_1_53_0)
set(BOOST_LIBRARYDIR ${BOOST_ROOT}/stage/lib)
set(Boost_USE_STATIC_LIBS ON)
find_package (Boost 1.44 COMPONENTS date_time REQUIRED)

# TREP-VA 7.0 SDK
set(VHAYU_ROOT D:/Vhayu-7.0.5)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(VHAYU_BUILD_TYPE "debug")
else (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(VHAYU_BUILD_TYPE "release")
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")
set(VHAYU_INCLUDE_DIRS
	${VHAYU_ROOT}/Engine/TBSDK/Include
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/include
)
set(VHAYU_LIBRARY_DIRS
	${VHAYU_ROOT}/Engine/TBSDK/Lib64/${VHAYU_BUILD_TYPE}
	${VHAYU_ROOT}/Engine/ThirdParty/xerces-c-3.1.1-x86_64-windows-vc-10.0/lib
)
set(VHAYU_XERCES_LIBRARY
	optimized xerces-c_3.lib
	debug xerces-c_3D.lib
)
set(VHAYU_LIBRARIES
	FlexRecord.lib
	PluginFramework.lib
	Utility.lib
	SEDll.lib
	${VHAYU_XERCES_LIBRARY}
)

# Python for source generators
find_package(PythonInterp REQUIRED)

#-----------------------------------------------------------------------------
# force off-tree build

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
message(FATAL_ERROR "CMake generation is not allowed within the source directory!
Remove the CMakeCache.txt file and try again from another folder, e.g.:

   del CMakeCache.txt
   mkdir build
   cd build
   cmake ..
")
endif(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

#-----------------------------------------------------------------------------
# default to Release build

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH  ${CMAKE_BINARY_DIR}/lib)

#-----------------------------------------------------------------------------
# platform specifics

add_definitions(
	-DWIN32
	-DWIN32_LEAN_AND_MEAN
# Windows Server 2008 R2 and Windows 7
	-D_WIN32_WINNT=0x0601
# production release
	-DOFFICIAL_BUILD
)

# SEH Exceptions.
string(REGEX REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

# Parallel make.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

# Optimization flags.
# http://msdn.microsoft.com/en-us/magazine/cc301698.aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "${CMAKE_MODULE_LINKER_FLAGS_RELEASE} /LTCG")

# Disable buffer security check.
# http://msdn.microsoft.com/en-us/library/8dbf701c(v=vs.80).aspx
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GS-")

#-----------------------------------------------------------------------------
# Generated source files

configure_file(
	${CMAKE_SOURCE_DIR}/version_generator.py.in
	${CMAKE_BINARY_DIR}/version_generator.py
	@ONLY
)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/version.cc
	COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_BINARY_DIR}/version_generator.py > ${CMAKE_BINARY_DIR}/version.cc
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	DEPENDS ${CMAKE_BINARY_DIR}/version_generator.py
)
configure_file(
	${CMAKE_SOURCE_DIR}/src/version.hh
	${CMAKE_BINARY_DIR}/version.hh
	COPYONLY
)
configure_file(
	${CMAKE_SOURCE_DIR}/version.rc.in
	${CMAKE_BINARY_DIR}/version.rc
	@ONLY
)

#-----------------------------------------------------------------------------
# source files

set(cxx-sources
	src/config.cc
	src/plugin.cc
	src/tcl.cc
	src/chromium/chromium_switches.cc
	src/chromium/command_line.cc
	src/chromium/logging.cc
	src/chromium/string_piece.cc
	src/chromium/string_split.cc
	src/chromium/string_util.cc
	src/chromium/vlog.cc
	src/chromium/debug/stack_trace.cc
	src/chromium/debug/stack_trace_win.cc
	src/chromium/memory/singleton.cc
	src/chromium/synchronization/lock.cc
	src/chromium/synchronization/lock_impl_win.cc
	${CMAKE_BINARY_DIR}/version.cc
)

set(rc-sources
	${CMAKE_BINARY_DIR}/version.rc
)

include_directories(
	include
	${Boost_INCLUDE_DIRS}
	${VHAYU_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${VHAYU_LIBRARY_DIRS}
)

#-----------------------------------------------------------------------------
# output

add_library(Spoon SHARED ${cxx-sources} ${rc-sources})

target_link_libraries(Spoon
	${Boost_LIBRARIES}
	${VHAYU_LIBRARIES}
	dbghelp.lib
)

set(config
	${CMAKE_CURRENT_SOURCE_DIR}/config/Spoon.xml
	${CMAKE_CURRENT_SOURCE_DIR}/config/date_time_zonespec.csv
)

install (TARGETS Spoon DESTINATION bin)
install (FILES ${config} DESTINATION config)

set (CPACK_SOURCE_IGNORE_FILES "*.lib")

include (InstallRequiredSystemLibraries)
set (CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set (CPACK_PACKAGE_DESCRIPTION_SUMMARY "Velocity Analytics Spoon plugin")
set (CPACK_PACKAGE_VENDOR "Thomson Reuters")
set (CPACK_NSIS_CONTACT "Steven McCoy <Steven.McCoy@thomsonreuters.com>")
# There is a bug in NSI that does not handle full unix paths properly. Make
# sure there is at least one set of four (4) backlasshes.
set (CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\installer.ico")
set (CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\branding.bmp")
set (CPACK_NSIS_COMPRESSOR "/SOLID lzma")
set (CPACK_PACKAGE_VERSION_MAJOR "${SPOON_VERSION_MAJOR}")
set (CPACK_PACKAGE_VERSION_MINOR "${SPOON_VERSION_MINOR}")
set (CPACK_PACKAGE_VERSION_PATCH "${SPOON_VERSION_BUILD}")
include (CPack)


# end of file
