# - CMake configuration file for the UniMRCP wrapper.
#
# Process this file with CMake to produce a Makefile

#=============================================================================
# Copyright 2014 SpeechTech, s.r.o. http://www.speechtech.cz/en
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# $Id$
#=============================================================================

cmake_minimum_required (VERSION 2.8)

project (UniMRCPWrapper)

file (STRINGS "${PROJECT_SOURCE_DIR}/UniMRCP-wrapper-version.h" _uw_ver
	REGEX "^#define[ \t]+UW_[ACHIJMNOPRT]+_VERSION[ \t]+[0-9]+")
string (REGEX REPLACE ".*[ \t]UW_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" UW_VERSION_MAJOR "${_uw_ver}")
string (REGEX REPLACE ".*[ \t]UW_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" UW_VERSION_MINOR "${_uw_ver}")
string (REGEX REPLACE ".*[ \t]UW_PATCH_VERSION[ \t]+([0-9]+).*" "\\1" UW_VERSION_PATCH "${_uw_ver}")
set (UW_VERSION_STRING "${UW_VERSION_MAJOR}.${UW_VERSION_MINOR}.${UW_VERSION_PATCH}")
message (STATUS "Building UniMRCP wrapper version [${UW_VERSION_STRING}]")

option (BUILD_SHARED_LIBS "Build UniMRCpp (and possibly others) as a shared library")
option (CPP_EXPORT "Export C++ interface" OFF)
find_path (UNIMRCP_SOURCE_DIR "build/uni_version.h" PATHS
	"${PROJECT_SOURCE_DIR}/../../trunk/"
	"${PROJECT_SOURCE_DIR}/../../UniMRCP/"
	"${PROJECT_SOURCE_DIR}/../../unimrcp/"
	"${PROJECT_SOURCE_DIR}/../../*/"
	"${PROJECT_SOURCE_DIR}/../")
set (UNIMRCP_SOURCE_DIR "${UNIMRCP_SOURCE_DIR}" CACHE PATH "Path to UniMRCP source directory")
set_property (CACHE UNIMRCP_SOURCE_DIR PROPERTY HELPSTRING "Path to UniMRCP source directory")
set (APR_SOURCE_DIR "${UNIMRCP_SOURCE_DIR}/libs/apr" CACHE PATH "Path to APR source directory")
set (APU_SOURCE_DIR "${UNIMRCP_SOURCE_DIR}/libs/apr-util" CACHE PATH "Path to APR-util source directory")
#if (WIN32)
#	set (APRICONV_DIR "${UNIMRCP_SOURCE_DIR}/libs/apr-iconv" CACHE PATH "Path to APR-iconv source directory")
#endif (WIN32)
set (SOFIA_SOURCE_DIR "${UNIMRCP_SOURCE_DIR}/libs/sofia-sip" CACHE PATH "Path to SofiaSIP source directory")

if (MSVC)

set (PRODUCT_NAME "UniMRCP" CACHE STRING "Product name to embed to DLL VersionInfo")
set (PRODUCT_VERSION "${UW_VERSION_STRING}" CACHE STRING "Product version to embed to DLL VersionInfo (format x.x.x.x)")
if (PRODUCT_VERSION STREQUAL "")
	set (PRODUCT_VERSION "${UW_VERSION_STRING}" CACHE STRING "Product version to embed to DLL VersionInfo (format x.x.x.x)" FORCE)
endif (PRODUCT_VERSION STREQUAL "")
if (PRODUCT_VERSION MATCHES "^[ ]*([0-9]+)(\\.([0-9]+))?(\\.([0-9]+))?(\\.([0-9]+))?[ ]*$")
	if (NOT CMAKE_MATCH_1 STREQUAL "")
		set (PRODVER_DEF "PRODVER_MAJOR=${CMAKE_MATCH_1}")
	endif (NOT CMAKE_MATCH_1 STREQUAL "")
	if (NOT CMAKE_MATCH_3 STREQUAL "")
		set (PRODVER_DEF "${PRODVER_DEF};PRODVER_MINOR=${CMAKE_MATCH_3}")
	endif (NOT CMAKE_MATCH_3 STREQUAL "")
	if (NOT CMAKE_MATCH_5 STREQUAL "")
		set (PRODVER_DEF "${PRODVER_DEF};PRODVER_PATCH=${CMAKE_MATCH_5}")
	endif (NOT CMAKE_MATCH_5 STREQUAL "")
	if (NOT CMAKE_MATCH_7 STREQUAL "")
		set (PRODVER_DEF "${PRODVER_DEF};PRODVER_TWEAK=${CMAKE_MATCH_7}")
	endif (NOT CMAKE_MATCH_7 STREQUAL "")
else ()
	message (SEND_ERROR "Invalid PRODUCT_VERSION: \"${PRODUCT_VERSION}\", use 1.2.3.4 format")
	set (PRODVER_DEF "PRODVER_MAJOR=${UW_VERSION_MAJOR};PRODVER_MINOR=${UW_VERSION_MINOR};PRODVER_PATCH=${UW_VERSION_PATCH}")
endif ()

option (MSVCRT_STATIC "Use static MS Visual C Runtime")
if (MSVCRT_STATIC)
	macro (static_flags flgs)
		foreach (cnf IN ITEMS ${CMAKE_CONFIGURATION_TYPES} "")
			if (NOT cnf STREQUAL "")
				string (TOUPPER ${cnf} cnf)
				set (cnf "_${cnf}")
			endif (NOT cnf STREQUAL "")
			string (REGEX REPLACE " /MD(d?) " " /MT\\1 " res " ${CMAKE_${flgs}_FLAGS${cnf}} ")
			string (STRIP "${res}" res)
			get_property (_hlp CACHE CMAKE_${flgs}_FLAGS${cnf} PROPERTY HELPSTRING)
			set (CMAKE_${flgs}_FLAGS${cnf} "${res}" CACHE STRING ${_hlp} FORCE)
		endforeach (cnf)
	endmacro (static_flags)
	static_flags (C)
	static_flags (CXX)
endif (MSVCRT_STATIC)

foreach (opt IN ITEMS C CXX)
	set (CMAKE_${opt}_FLAGS_RELEASE "${CMAKE_${opt}_FLAGS_RELEASE} /GL")
endforeach (opt)

foreach (opt IN ITEMS EXE MODULE SHARED STATIC)
	set (CMAKE_${opt}_LINKER_FLAGS_RELEASE "${CMAKE_${opt}_LINKER_FLAGS_RELEASE} /LTCG")
endforeach (opt)

endif (MSVC)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/sign.cmake")
	include ("${CMAKE_CURRENT_LIST_DIR}/sign.cmake")
endif (EXISTS "${CMAKE_CURRENT_LIST_DIR}/sign.cmake")

macro (adjust_cflags target)
	if (MSVC)
		set_target_properties (${target} PROPERTIES COMPILE_FLAGS
			"/W4")
	else (MSVC)
		set_target_properties (${target} PROPERTIES COMPILE_FLAGS
			"-Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual -fvisibility=hidden -fvisibility-inlines-hidden")
	endif (MSVC)
endmacro (adjust_cflags)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR})
find_package (APR REQUIRED)
find_package (APRUtil REQUIRED)
find_package (Sofia REQUIRED)
find_package (UniMRCP REQUIRED COMPONENTS Client)

include_directories (
	${PROJECT_SOURCE_DIR}
	${UNIMRCP_INCLUDE_DIRS}
	${APU_INCLUDE_DIRS}
	${APR_INCLUDE_DIRS}
	${SOFIA_INCLUDE_DIRS}
	${SOFIA_PTW32_INCLUDE_DIRS}
)
link_libraries (
	${UNIMRCP_CLIENT_LIBRARIES}
	${APU_LIBRARIES}
	${APR_LIBRARIES}
	${SOFIA_LIBRARIES}
)
set (UW_DEFINES
	${APR_DEFINES}
	${APU_DEFINES}
	${SOFIA_DEFINES}
	${UNIMRCP_DEFINES}
)
if (WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
	set (UW_DEFINES ${UW_DEFINES} -DWIN64 -D_WIN64)
endif (WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
foreach (pvd IN LISTS PRODVER_DEF)
	set (UW_DEFINES ${UW_DEFINES} -D${pvd})
endforeach (pvd)
if (PRODUCT_NAME)
	set (UW_DEFINES ${UW_DEFINES} "-DPRODNAME=${PRODUCT_NAME}")
endif (PRODUCT_NAME)
add_definitions (${UW_DEFINES})

set (WRAPPER_NATIVE
	UniMRCP-wrapper.cpp
	UniMRCP-wrapper.h
	UniMRCP-wrapper-version.h)
if (WIN32)
	set (WRAPPER_NATIVE ${WRAPPER_NATIVE}
		UniMRCP-wrapper.rc)
endif (WIN32)
set (WRAPPER_DEFS)

macro (copy_example file)
	add_custom_command (
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${file}"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/${file}" "${CMAKE_CURRENT_BINARY_DIR}/${file}"
		MAIN_DEPENDENCY "${file}"
		COMMENT "Copying example ${file}"
		VERBATIM)
endmacro (copy_example)


find_package (SWIG)
if (SWIG_FOUND)

include (${SWIG_USE_FILE})
option (SWIG_HACKS "Enable some SWIG hacks. If you experience configuration/generation/building problems, turn this option off." ON)
set (SWIG_CXX_EXTENSION cpp)
if (SWIG_HACKS)
	set (SWIG_CSHARP_EXTRA_FILE_EXTENSION h)
	set (SWIG_PYTHON_EXTRA_FILE_EXTENSION h)
endif (SWIG_HACKS)
set (WRAPPER_BASE UniMRCP-wrapper)
set (WRAPPER_SWIG ${WRAPPER_BASE}.i)
set (WRAPPER_SRC ${WRAPPER_SWIG} ${WRAPPER_NATIVE})
set (CMAKE_SWIG_FLAGS "-nodefaultctor;-nodefaultdtor;-Wall;-Wallkw;-Wextra;${UW_DEFINES}")
option (SAFE_ARRAYS "Use safe array passing (e.g. marshalling instead of pinning where available)" ON)
if (SAFE_ARRAYS)
	set (CMAKE_SWIG_FLAGS ${CMAKE_SWIG_FLAGS} -DSAFE_ARRAYS)
	set (WRAPPER_DEFS ${WRAPPER_DEFS} SAFE_ARRAYS)
endif (SAFE_ARRAYS)
if (CPP_EXPORT)
	set (CMAKE_SWIG_FLAGS ${CMAKE_SWIG_FLAGS} -DUNIMRCP_WRAPPER_EXPORT)
	set (WRAPPER_DEFS ${WRAPPER_DEFS} UNIMRCP_WRAPPER_EXPORT)
endif (CPP_EXPORT)
set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
	CPLUSPLUS ON)

option (WRAP_CSHARP "Build UniMRCP for C# .NET" ON)
if (WRAP_CSHARP)
	set (CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/CSharp/wrapper")
	set (SWIG_MODULE_UniMRCP-NET_EXTRA_DEPS UniMRCP-wrapper.h)
	set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
		SWIG_FLAGS "-dllimport;UniMRCP-NET")
	if (SWIG_HACKS)
		set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
			SWIG_MODULE_NAME "../${WRAPPER_BASE}CSHARP_wrap")
	endif (SWIG_HACKS)
	set (CSHARP_EXAMPLES CSharp/UniSynth.cs CSharp/UniSynth.bat CSharp/UniSynth.sh)
	swig_add_module (UniMRCP-NET CSharp ${WRAPPER_SRC}
		${CSHARP_EXAMPLES})
	source_group (Examples FILES ${CSHARP_EXAMPLES})
	foreach (ex IN LISTS CSHARP_EXAMPLES)
		copy_example (${ex})
	endforeach (ex)
	set (NET_LIBNAME "${CMAKE_SHARED_MODULE_PREFIX}UniMRCP-NET${CMAKE_SHARED_MODULE_SUFFIX}")
	set_target_properties (${SWIG_MODULE_UniMRCP-NET_REAL_NAME} PROPERTIES
		COMPILE_DEFINITIONS "RC_TARGET_PLATFORM=.NET;RC_OUTFILENAME=${NET_LIBNAME};WRAPPER;${WRAPPER_DEFS}"
		LIBRARY_OUTPUT_DIRECTORY CSharp)
	adjust_cflags (${SWIG_MODULE_UniMRCP-NET_REAL_NAME})
	if (SIGN)
		sign_binary (${SWIG_MODULE_UniMRCP-NET_REAL_NAME})
	endif (SIGN)
endif (WRAP_CSHARP)

find_package (PythonLibs)
if (PYTHONLIBS_FOUND)
option (WRAP_PYTHON "Build UniMRCP for Python" ON)
if (WRAP_PYTHON)
	set (CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/Python/wrapper")
	set (SWIG_MODULE_UniMRCP_EXTRA_DEPS UniMRCP-wrapper.h)
	set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
		SWIG_FLAGS "-threads")
	if (SWIG_HACKS)
		set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
			SWIG_MODULE_NAME "../${WRAPPER_BASE}PYTHON_wrap")
	endif (SWIG_HACKS)
	include_directories (${PYTHON_INCLUDE_DIRS})
	set (PYTHON_EXAMPLES Python/UniSynth.py Python/UniSynth.bat)
	swig_add_module (PyUniMRCP Python ${WRAPPER_SRC}
		${PYTHON_EXAMPLES})
	source_group (Examples FILES ${PYTHON_EXAMPLES})
	foreach (ex IN LISTS PYTHON_EXAMPLES)
		copy_example (${ex})
	endforeach (ex)
	if (WIN32)
		set (py_suffix .pyd)
	else (WIN32)
		set (py_suffix ${CMAKE_SHARED_MODULE_SUFFIX})
	endif (WIN32)
	set_target_properties (${SWIG_MODULE_PyUniMRCP_REAL_NAME} PROPERTIES
		COMPILE_DEFINITIONS "RC_TARGET_PLATFORM=Python;RC_OUTFILENAME=_UniMRCP${py_suffix};WRAPPER;${WRAPPER_DEFS}"
		OUTPUT_NAME _UniMRCP
		LIBRARY_OUTPUT_DIRECTORY Python)
	# Workaround for buggy SelectLibraryConfigurations
	if (PYTHON_LIBRARY_RELEASE AND PYTHON_LIBRARY_DEBUG AND NOT PYTHON_LIBRARY_RELEASE STREQUAL PYTHON_LIBRARY_DEBUG)
		swig_link_libraries (PyUniMRCP
			optimized ${PYTHON_LIBRARY_RELEASE} debug ${PYTHON_LIBRARY_DEBUG})
	else ()
		swig_link_libraries (PyUniMRCP ${PYTHON_LIBRARIES})
	endif ()
	adjust_cflags (${SWIG_MODULE_PyUniMRCP_REAL_NAME})
	if (SIGN)
		sign_binary (${SWIG_MODULE_PyUniMRCP_REAL_NAME})
	endif (SIGN)
endif (WRAP_PYTHON)
endif (PYTHONLIBS_FOUND)

find_package (JNI)
if (JNI_INCLUDE_DIRS AND NOT JNI_FOUND)
	message(STATUS "Whole JNI not found, only JNI_INCLUDE_DIRS, "
	               "continuing with fingers crossed.")
endif (JNI_INCLUDE_DIRS AND NOT JNI_FOUND)
if (JNI_INCLUDE_DIRS)
option (WRAP_JAVA "Build UniMRCP for Java" ON)
if (WRAP_JAVA)
	set (CMAKE_SWIG_OUTDIR "${CMAKE_CURRENT_BINARY_DIR}/Java/org/unimrcp/swig")
	set (SWIG_MODULE_JUniMRCP_EXTRA_DEPS UniMRCP-wrapper.h)
	set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
		SWIG_FLAGS "-package;org.unimrcp.swig")
	if (SWIG_HACKS)
		set_source_files_properties (${WRAPPER_SWIG} PROPERTIES
			SWIG_MODULE_NAME "../${WRAPPER_BASE}JAVA_wrap")
	endif (SWIG_HACKS)
	include_directories (${JNI_INCLUDE_DIRS})
	set (JAVA_EXAMPLES Java/UniSynth.java Java/UniSynth.bat Java/UniSynth.sh Java/UniRecog.java Java/UniRecog.bat Java/UniRecog.sh)
	swig_add_module (JUniMRCP Java ${WRAPPER_SRC}
		${JAVA_EXAMPLES})
	source_group (Examples FILES ${JAVA_EXAMPLES})
	foreach (ex IN LISTS JAVA_EXAMPLES)
		copy_example (${ex})
	endforeach (ex)
	if (APPLE)
		set (J_LIBNAME "${CMAKE_SHARED_MODULE_PREFIX}UniMRCP.jnilib")
	else (APPLE)
		set (J_LIBNAME "${CMAKE_SHARED_MODULE_PREFIX}UniMRCP${CMAKE_SHARED_MODULE_SUFFIX}")
	endif (APPLE)
	set_target_properties (${SWIG_MODULE_JUniMRCP_REAL_NAME} PROPERTIES
		COMPILE_DEFINITIONS "RC_TARGET_PLATFORM=Java;RC_OUTFILENAME=${J_LIBNAME};WRAPPER;${WRAPPER_DEFS}"
		OUTPUT_NAME UniMRCP
		LIBRARY_OUTPUT_DIRECTORY Java)
	adjust_cflags (${SWIG_MODULE_JUniMRCP_REAL_NAME})
	if (SIGN)
		sign_binary (${SWIG_MODULE_JUniMRCP_REAL_NAME})
	endif (SIGN)
endif (WRAP_JAVA)
endif (JNI_INCLUDE_DIRS)

endif (SWIG_FOUND)


find_package (Doxygen)
if (DOXYGEN_FOUND)
	add_custom_target (doc
		${DOXYGEN_EXECUTABLE} UniMRCP-wrapper.dox
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Generating API documentation with Doxygen"
		VERBATIM)
endif (DOXYGEN_FOUND)

option (WRAP_CPP "Build UniMRCP high-level C++ library" ON)
if (WRAP_CPP)
	add_library (UniMRCpp ${WRAPPER_NATIVE})
	set (CPP_LIBNAME "${CMAKE_SHARED_MODULE_PREFIX}UniMRCpp${CMAKE_SHARED_MODULE_SUFFIX}")
	set (CPP_WRAPPER_DEFS ${WRAPPER_DEFS})
	set_target_properties (UniMRCpp PROPERTIES
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Cpp"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Cpp"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/Cpp"
		COMPILE_DEFINITIONS "RC_TARGET_PLATFORM=Cpp;RC_OUTFILENAME=${CPP_LIBNAME};${CPP_WRAPPER_DEFS}"
		DEFINE_SYMBOL UNIMRCP_WRAPPER_EXPORT)
	if (WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8) AND NOT BUILD_SHARED_LIBS)
		set_target_properties (UniMRCpp PROPERTIES
			STATIC_LIBRARY_FLAGS "/MACHINE:X64")
	endif (WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8) AND NOT BUILD_SHARED_LIBS)
	add_custom_command (TARGET UniMRCpp POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/UniMRCP-wrapper.h" "${CMAKE_CURRENT_BINARY_DIR}/Cpp/UniMRCP-wrapper.h"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/Cpp/UniSynth.cpp" "${CMAKE_CURRENT_BINARY_DIR}/Cpp/UniSynth.cpp"
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/Cpp/UniSynth.bat" "${CMAKE_CURRENT_BINARY_DIR}/Cpp/UniSynth.bat"
		COMMENT "Copying C++ examples"
		VERBATIM)
	adjust_cflags (UniMRCpp)
	if (SIGN AND BUILD_SHARED_LIBS)
		sign_binary (UniMRCpp)
	endif (SIGN AND BUILD_SHARED_LIBS)

	option (BUILD_CPP_EXAMPLE "Build example C++ application UniSynth" ON)
	if (BUILD_CPP_EXAMPLE)
		add_executable (UniSynth_Cpp
			Cpp/UniSynth.cpp)
		add_dependencies (UniSynth_Cpp UniMRCpp)
		target_link_libraries (UniSynth_Cpp UniMRCpp)
		set_target_properties (UniSynth_Cpp PROPERTIES
			RUNTIME_OUTPUT_DIRECTORY Cpp)
		adjust_cflags (UniSynth_Cpp)
	endif (BUILD_CPP_EXAMPLE)
endif (WRAP_CPP)

option (BUILD_C_EXAMPLE "Build example C application UniSynth" ON)
if (BUILD_C_EXAMPLE)
	add_executable (UniSynth_C
		C/UniSynth.c C/UniSynth.bat)
	set_target_properties (UniSynth_C PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY C)
	source_group (Examples FILES C/UniSynth.bat)
	copy_example (C/UniSynth.bat)
	add_custom_command (TARGET UniSynth_C POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/C/UniSynth.c" "${CMAKE_CURRENT_BINARY_DIR}/C/UniSynth.c"
		COMMENT "Copying example C/UniSynth.c"
		VERBATIM)
	adjust_cflags (UniSynth_C)
endif (BUILD_C_EXAMPLE)
