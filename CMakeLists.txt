cmake_minimum_required(VERSION 2.8)

project(obs-text-pthread)
set(CMAKE_PROJECT_VERSION 1.0.1) # TODO: update cmake to 3.12 or later and put to 'project'

set(PLUGIN_AUTHOR "Norihiro Kamae")
set(MACOS_BUNDLEID "net.nagater.obs-text-pthread")
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

# TAKE NOTE: No need to edit things past this point

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT OBS_OUTPUT_DIR)
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
else()
	set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/plugins/obs-text-pango/cmake/")
endif()

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
	pkg_search_module(Pango pango)
	pkg_search_module(Cairo cairo)
	pkg_search_module(PangoCairo pangocairo)
else()
	find_package(Pango REQUIRED)
	find_package(Cairo REQUIRED)
	find_package(PangoCairo REQUIRED)
endif()

find_package(PNG)

if (WIN32 OR APPLE)
include(external/FindLibObs.cmake)
endif()

find_package(LibObs REQUIRED)

link_directories(
	${Pango_LIBRARY_DIRS}
	${Cairo_LIBRARY_DIRS}
	${PangoCairo_LIBRARY_DIRS}
)

configure_file(
    src/plugin-macros.h.in
    ../src/plugin-macros.generated.h
)
configure_file(
    installer/installer-macOS.pkgproj.in
    ../installer/installer-macOS.generated.pkgproj
)
configure_file(
    installer/installer-Windows.iss.in
    ../installer/installer-Windows.generated.iss
)

configure_file(
    ci/ci_includes.sh.in
    ../ci/ci_includes.generated.sh
)
configure_file(
    ci/ci_includes.cmd.in
    ../ci/ci_includes.generated.cmd
)

set(PLUGIN_SOURCES
	src/obs-text-pthread-main.c
	src/obs-text-pthread-thread.c
)

set(PLUGIN_HEADERS
	src/plugin-macros.generated.h
	src/obs-text-pthread.h
)

# --- Platform-independent build settings ---
add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES} ${PLUGIN_HEADERS})

include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	${GLib_INCLUDE_DIRS}
	${Pango_INCLUDE_DIRS}
	${Cairo_INCLUDE_DIRS}
	${PangoCairo_INCLUDE_DIRS}
	${PNG_INCLUDE_DIRS}
)

message("GLib_LIBRARY=${GLib_LIBRARY}")
message("Pango_INCLUDE_DIRS=${Pango_INCLUDE_DIRS}")
message("Pango_LIBRARIES=${Pango_LIBRARIES}")
message("Pango_LIBRARY_DIRS=${Pango_LIBRARY_DIRS}")
message("Cairo_INCLUDE_DIRS=${Cairo_INCLUDE_DIRS}")
message("Cairo_LIBRARIES=${Cairo_LIBRARIES}")
message("Cairo_LIBRARY_DIRS=${Cairo_LIBRARY_DIRS}")
message("PangoCairo_INCLUDE_DIRS=${PangoCairo_INCLUDE_DIRS}")
message("PangoCairo_LIBRARIES=${PangoCairo_LIBRARIES}")
message("PangoCairo_LIBRARY_DIRS=${PangoCairo_LIBRARY_DIRS}")

target_link_libraries(${CMAKE_PROJECT_NAME}
	libobs
	${OBS_LIBRARIES}
	${Pango_LIBRARIES}
	${Cairo_LIBRARIES}
	${PangoCairo_LIBRARIES}
	${PNG_LIBRARIES}
)

# --- End of section ---

# --- Windows-specific build settings and tasks ---
if(WIN32)
	if(NOT DEFINED OBS_FRONTEND_LIB)
		set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
		message(FATAL_ERROR "Could not find OBS Frontend API\'s library !")
	endif()

    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4-)
    endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(ARCH_NAME "64bit")
		set(OBS_BUILDDIR_ARCH "build64")
	else()
		set(ARCH_NAME "32bit")
		set(OBS_BUILDDIR_ARCH "build32")
	endif()

	include_directories(
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	)

	target_link_libraries(${CMAKE_PROJECT_NAME}
		"${OBS_FRONTEND_LIB}"
    )

	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        # If config is Release or RelWithDebInfo, package release files
        COMMAND if $<CONFIG:Release>==1 OR $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Release>==1 OR $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Release>==1 OR $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # If config is RelWithDebInfo, copy the pdb file
        COMMAND if $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # Copy to obs-studio dev environment for immediate testing
        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )
    )
	# --- End of sub-section ---

endif()
# --- End of section ---

if(UNIX)
	# Because my primary development target is Linux.
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -Wall")
endif()

# --- Linux-specific build settings and tasks ---
if(UNIX AND NOT APPLE)
	include(GNUInstallDirs)

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} obs-frontend-api)

	file(GLOB data_files data/locale/*.ini data/*.effect)

	install(TARGETS ${CMAKE_PROJECT_NAME}
		LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins")

	install(FILES ${data_files}
		DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/")
endif()
# --- End of section ---

# -- OS X specific build settings and tasks --
if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	#target_link_libraries(${CMAKE_PROJECT_NAME} "${OBS_FRONTEND_LIB}")
endif()
# -- End of section --
