# - Find UniMRCP Client and/or Server libraries
# This module finds if UniMRCP is installed or available in its source dir
# and determines where the include files and libraries are.
#
# The module will optionally accept the COMPONENTS argument. If no COMPONENTS
# are specified, then the find module will default to finding only the Client
# library. If one or more COMPONENTS are specified, the module will attempt to
# find them. The only valid components are Client and Server. Finding
# the Server is not supported yet. If the COMPONENTS argument is not given,
# the module will attempt to find only the Client.
#
# This code sets the following variables:
#
#  UNIMRCP_FOUND            - have the UniMRCP libs been found
#  UNIMRCP_INCLUDE_DIRS     - paths to where required headers are located
#  UNIMRCP_DEFINES          - flags to define to compile with UniMRCP
#  UNIMRCP_CLIENT_LIBRARIES - path to the UniMRCP client libraries
#  UNIMRCP_SERVER_LIBRARIES - path to the UniMRCP server libraries
#  UNIMRCP_LIBRARIES        - path to all the UniMRCP libraries
#  UNIMRCP_VERSION_STRING   - version of the UniMRCP found
#
# The UNIMRCP_STATIC variable can be used to specify whether to prefer
# static version of UniMRCP libraries.
# You need to set this variable before calling find_package(UniMRCP).
#
# If you'd like to specify the installation of UniMRCP to use, you should modify
# the following cache variables:
#  UNIMRCP_CLIENT_LIBRARY  - path to the UniMRCP client libraries
#  UNIMRCP_SERVER_LIBRARY  - path to the UniMRCP server libraries
#  UNIMRCP_INCLUDE_DIR     - path to where uni_version.h is found
# If UniMRCP not installed, it can be used from the source directory:
#  UNIMRCP_SOURCE_DIR      - path to compiled UniMRCP source directory

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


set (UNIMRCP_VALID_COMPONENTS CLIENT SERVER)
set (UNIMRCP_REQUIRED_VARS UNIMRCP_LIBRARIES UNIMRCP_INCLUDE_DIRS)
if (UniMRCP_FIND_COMPONENTS)
	foreach (comp IN LISTS UniMRCP_FIND_COMPONENTS)
		string (TOUPPER "${comp}" ucomp)
		if (ucomp STREQUAL "SERVER")
			message (FATAL_ERROR "Finding UniMRCP Server not implemented yet.")
		endif (ucomp STREQUAL "SERVER")
		list (FIND UNIMRCP_VALID_COMPONENTS "${ucomp}" i)
		if (${i} LESS 0)
			message (FATAL_ERROR "\"${comp}\" is not a valid UniMRCP component.")
		else (${i} LESS 0)
			list (APPEND uni_comps "${comp}")
			if (UniMRCP_FIND_REQUIRED_${comp})
				set (UNIMRCP_REQUIRED_VARS ${UNIMRCP_REQUIRED_VARS}
					UNIMRCP_${ucomp}_LIBRARIES UNIMRCP_${ucomp}_INCLUDE_DIRS)
			endif (UniMRCP_FIND_REQUIRED_${comp})
		endif (${i} LESS 0)
	endforeach (comp)
else (UniMRCP_FIND_COMPONENTS)
	set (uni_comps CLIENT)
	set (UNIMRCP_REQUIRED_VARS ${UNIMRCP_REQUIRED_VARS} UNIMRCP_CLIENT_LIBRARIES)
endif (UniMRCP_FIND_COMPONENTS)

# Visual Studio builds static libraries only
if (NOT DEFINED UNIMRCP_STATIC AND MSVC)
	set (UNIMRCP_STATIC ON)
endif (NOT DEFINED UNIMRCP_STATIC AND MSVC)
option (UNIMRCP_STATIC "Try to find and link static UniMRCP libraries" ${UNIMRCP_STATIC})
mark_as_advanced (UNIMRCP_STATIC)

option (UNIMRCP_LONG_REQUEST_ID "Allow very long MRCP request ID. UniMRCP libraries must be compiled compatibly!")
mark_as_advanced (UNIMRCP_LONG_REQUEST_ID)

include (SelectLibraryConfigurations)

# Try to find library specified by ${libnames}
# in ${hints} and put its path to ${var}_LIBRARY and ${var}_LIBRARY_DEBUG,
# and set ${var}_LIBRARIES similarly to CMake's select_library_configurations macro.
# For 32bit configurations, "/x64/" is replaced with "/".
function (find_libs var libnames hints)
	if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
		string (REGEX REPLACE "[\\\\/][xX]64[\\\\/]" "/" hints "${hints}")
	endif (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	string (REPLACE "/LibR" "/LibD" hints_debug "${hints}")
	string (REPLACE "/Release" "/Debug" hints_debug "${hints_debug}")
	find_library (${var}_LIBRARY
		NAMES ${libnames}
		HINTS ${hints})
	find_library (${var}_LIBRARY_DEBUG
		NAMES ${libnames}
		HINTS ${hints_debug})
	mark_as_advanced (${var}_LIBRARY ${var}_LIBRARY_DEBUG)
	if (${var}_LIBRARY AND ${var}_LIBRARY_DEBUG AND
			NOT (${var}_LIBRARY STREQUAL ${var}_LIBRARY_DEBUG) AND
			(CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE))
		set (${var}_LIBRARIES optimized ${${var}_LIBRARY} debug ${${var}_LIBRARY_DEBUG} PARENT_SCOPE)
	elseif (${var}_LIBRARY)
		set (${var}_LIBRARIES ${${var}_LIBRARY} PARENT_SCOPE)
	elseif (${var}_LIBRARY_DEBUG)
		set (${var}_LIBRARIES ${${var}_LIBRARY_DEBUG} PARENT_SCOPE)
	else ()
		set (${var}_LIBRARIES ${var}_LIBRARY-NOTFOUND PARENT_SCOPE)
	endif ()
endfunction (find_libs)

# Visual Studio statically linked component depends on additional libs,
# i.e. the component's static library does not contain all the objects,
# they are contained in other libraries.
set (UNI_CLIENT_LIBNAMES aprtoolkit mrcpclient mpf mrcp mrcpsofiasip mrcpunirtsp mrcpv2transport mrcpsignaling unirtsp)
set (UNI_SERVER_LIBNAMES)
macro (find_msvc_lib lib)
	find_libs (UNIMRCP_${lib} "${lib};lib${lib}" "${_uni_hints}")
	if (UNIMRCP_${lib}_LIBRARIES)
		set (UNIMRCP_${ucomp}_LIBRARIES ${UNIMRCP_${ucomp}_LIBRARIES}
			${UNIMRCP_${lib}_LIBRARIES})
	else (UNIMRCP_${lib}_LIBRARIES)
		message ("UniMRCP warning: ${ucomp} library ${lib} not found")
	endif (UNIMRCP_${lib}_LIBRARIES)
endmacro (find_msvc_lib)

macro (find_static_lib ucomp)
	set (_uni_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
	if (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
	else (WIN32)
		set (CMAKE_FIND_LIBRARY_SUFFIXES .a)
	endif (WIN32)
	set (_uni_hints)
	string (TOLOWER "${ucomp}" lcomp)
	if (UNIMRCP_SOURCE_DIR)
		set (_uni_hints ${_uni_hints} "${UNIMRCP_SOURCE_DIR}/lib"
			"${UNIMRCP_SOURCE_DIR}/x64/Release/lib"
			"${UNIMRCP_SOURCE_DIR}/platforms/libunimrcp-${lcomp}/.libs")
	endif (UNIMRCP_SOURCE_DIR)
	set (_uni_hints ${_uni_hints} /usr/local/unimrcp/lib)
	find_libs (UNIMRCP_${ucomp} "unimrcp${lcomp};libunimrcp${lcomp}" "${_uni_hints}")
	if (MSVC AND UNI_${ucomp}_LIBNAMES)
		foreach (lib IN LISTS UNI_${ucomp}_LIBNAMES)
			find_msvc_lib (${lib})
		endforeach (lib)
	endif (MSVC AND UNI_${ucomp}_LIBNAMES)
	set (CMAKE_FIND_LIBRARY_SUFFIXES ${_uni_CMAKE_FIND_LIBRARY_SUFFIXES})
endmacro (find_static_lib)

macro (find_dynamic_lib ucomp)
	set (_uni_hints)
	string (TOLOWER "${ucomp}" lcomp)
	if (UNIMRCP_SOURCE_DIR)
		set (_uni_hints ${_uni_hints} "${UNIMRCP_SOURCE_DIR}/lib"
			"${UNIMRCP_SOURCE_DIR}/x64/Release/lib"
			"${UNIMRCP_SOURCE_DIR}/platforms/libunimrcp-${lcomp}/.libs")
	endif (UNIMRCP_SOURCE_DIR)
	set (_uni_hints ${_uni_hints} /usr/local/unimrcp/lib)
	find_libs (UNIMRCP_${ucomp} "unimrcp${lcomp};libunimrcp${lcomp}" "${_uni_hints}")
endmacro (find_dynamic_lib)

include (FindPackageMessage)

set (_uni_hhints /usr/local/unimrcp/include)
if (UNIMRCP_SOURCE_DIR)
	set (_uni_hhints ${_uni_hhints} ${UNIMRCP_SOURCE_DIR}/include
		"${UNIMRCP_SOURCE_DIR}/libs/mrcp-client/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mrcp-server/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mrcp/message/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mrcp/resources/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mrcp/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mrcpv2-transport/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mrcp-signaling/include"
		"${UNIMRCP_SOURCE_DIR}/libs/mpf/include"
		"${UNIMRCP_SOURCE_DIR}/libs/apr-toolkit/include"
		"${UNIMRCP_SOURCE_DIR}/platforms/libunimrcp-client/include"
		"${UNIMRCP_SOURCE_DIR}/platforms/libunimrcp-server/include"
		"${UNIMRCP_SOURCE_DIR}/build")
endif (UNIMRCP_SOURCE_DIR)
find_path (UNIMRCP_INCLUDE_DIR uni_version.h
	HINTS ${_uni_hhints})
mark_as_advanced (UNIMRCP_INCLUDE_DIR)
set (UNI_CLIENT_HEADERS unimrcp_client apt mpf mrcp mrcp_recog_header mrcp_message mrcp_application mrcp_sig_types mrcp_connection)
set (UNI_SERVER_HEADERS)
set (UNIMRCP_LIBRARIES)
foreach (comp IN LISTS uni_comps)
	string (TOUPPER "${comp}" ucomp)
	if (UNIMRCP_STATIC)
		find_static_lib (${ucomp})
		if (NOT UNIMRCP_${ucomp}_LIBRARIES)
			find_package_message (UniMRCP "Static UniMRCP ${ucomp} library not found, trying dynamic"
				"[${UNIMRCP_${ucomp}_LIBRARY}][${UNIMRCP_INCLUDE_DIR}][${UNIMRCP_STATIC}]")
			find_dynamic_lib (${ucomp})
		endif (NOT UNIMRCP_${ucomp}_LIBRARIES)
		set (UNIMRCP_DEFINES -DMRCP_STATIC_LIB -DMPF_STATIC_LIB -DAPT_STATIC_LIB)
		if (UNIMRCP_${ucomp}_LIBRARIES AND WIN32)
			set (UNIMRCP_${ucomp}_LIBRARIES ${UNIMRCP_${ucomp}_LIBRARIES}
				ws2_32 mswsock winmm rpcrt4 iphlpapi)
		endif (UNIMRCP_${ucomp}_LIBRARIES AND WIN32)
	else (UNIMRCP_STATIC)
		find_dynamic_lib (${ucomp})
		if (NOT UNIMRCP_${ucomp}_LIBRARIES)
			find_package_message (UniMRCP "Dynamic UniMRCP ${ucomp} library not found, trying static"
				"[${UNIMRCP_${ucomp}_LIBRARY}][${UNIMRCP_INCLUDE_DIR}][${UNIMRCP_STATIC}]")
			find_static_lib (${ucomp})
		endif (NOT UNIMRCP_${ucomp}_LIBRARIES)
		set (UNIMRCP_DEFINES)
	endif (UNIMRCP_STATIC)
	if (UNIMRCP_${ucomp}_LIBRARIES)
		set (UNIMRCP_LIBRARIES ${UNIMRCP_LIBRARIES} ${UNIMRCP_${ucomp}_LIBRARIES})
	endif (UNIMRCP_${ucomp}_LIBRARIES)

	set (UNIMRCP_${ucomp}_INCLUDE_DIRS ${UNIMRCP_INCLUDE_DIR})
	foreach (head IN LISTS UNI_${ucomp}_HEADERS)
		find_path (UNIMRCP_INC_${head} ${head}.h
			HINTS ${_uni_hhints})
		mark_as_advanced (UNIMRCP_INC_${head})
		if (UNIMRCP_INC_${head})
			set (UNIMRCP_${ucomp}_INCLUDE_DIRS ${UNIMRCP_${ucomp}_INCLUDE_DIRS}
				${UNIMRCP_INC_${head}})
		else (UNIMRCP_INC_${head})
			message ("UniMRCP warning: ${ucomp} ${head}.h not found")
		endif (UNIMRCP_INC_${head})
	endforeach (head)
	if (UNIMRCP_${ucomp}_INCLUDE_DIRS)
		list (REMOVE_DUPLICATES UNIMRCP_${ucomp}_INCLUDE_DIRS)
	endif (UNIMRCP_${ucomp}_INCLUDE_DIRS)
	set (UNIMRCP_INCLUDE_DIRS ${UNIMRCP_INCLUDE_DIRS} ${UNIMRCP_${ucomp}_INCLUDE_DIRS})

	if (UNIMRCP_${ucomp}_LIBRARIES AND UNIMRCP_${ucomp}_INCLUDE_DIRS)
		set (UniMRCP_${comp}_FOUND TRUE)
	endif (UNIMRCP_${ucomp}_LIBRARIES AND UNIMRCP_${ucomp}_INCLUDE_DIRS)
endforeach (comp)
if (UNIMRCP_INCLUDE_DIRS)
	list (REMOVE_DUPLICATES UNIMRCP_INCLUDE_DIRS)
endif (UNIMRCP_INCLUDE_DIRS)
if (UNIMRCP_LONG_REQUEST_ID)
	set (UNIMRCP_DEFINES ${UNIMRCP_DEFINES} -DTOO_LONG_MRCP_REQUEST_ID)
endif (UNIMRCP_LONG_REQUEST_ID)

if (UNIMRCP_INCLUDE_DIR)
	file (STRINGS "${UNIMRCP_INCLUDE_DIR}/uni_version.h" _uni_ver
		REGEX "^#define[ \t]+UNI_[ACHIJMNOPRT]+_VERSION[ \t]+[0-9]+")
	string (REGEX REPLACE ".*[ \t]UNI_MAJOR_VERSION[ \t]+([0-9]+).*" "\\1" _uni_major "${_uni_ver}")
	string (REGEX REPLACE ".*[ \t]UNI_MINOR_VERSION[ \t]+([0-9]+).*" "\\1" _uni_minor "${_uni_ver}")
	string (REGEX REPLACE ".*[ \t]UNI_PATCH_VERSION[ \t]+([0-9]+).*" "\\1" _uni_patch "${_uni_ver}")
	set (UNIMRCP_VERSION_STRING "${_uni_major}.${_uni_minor}.${_uni_patch}")

	file (STRINGS "${UNIMRCP_INCLUDE_DIR}/uni_revision.h" _uni_rev
		REGEX "^#define[ \t]+UNI_REVISION[ \t]+[0-9]+")
	if (_uni_rev)
		string (REGEX REPLACE ".*[ \t]UNI_REVISION[ \t]+([0-9]+).*" "\\1" _uni_rev "${_uni_rev}")
		set (UNIMRCP_VERSION_STRING "${UNIMRCP_VERSION_STRING}.${_uni_rev}")
	endif (_uni_rev)
endif (UNIMRCP_INCLUDE_DIR)

unset (UNIMRCP_FOUND)
if (CMAKE_VERSION VERSION_GREATER "2.8")
	set ($uni_HANDLE_COMPONENTS "HANDLE_COMPONENTS")
endif (CMAKE_VERSION VERSION_GREATER "2.8")
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (UniMRCP
	REQUIRED_VARS ${UNIMRCP_REQUIRED_VARS}
	VERSION_VAR UNIMRCP_VERSION_STRING
	${uni_HANDLE_COMPONENTS})
