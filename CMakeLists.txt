cmake_minimum_required(VERSION 3.12)

project(obs-text-pthread VERSION 2.0.3)

set(PLUGIN_AUTHOR "Norihiro Kamae")
set(MACOS_BUNDLEID "net.nagater.obs-text-pthread")
set(MACOS_PACKAGE_UUID "652BAFE7-E356-4A3F-9505-F89615E3A44A")
set(MACOS_INSTALLER_UUID "A4F0E91E-D759-472D-81D5-D1B0CE2CC2D8")
set(PLUGIN_URL "https://obsproject.com/forum/resources/pthread-text.1287/")
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

# TAKE NOTE: No need to edit things past this point

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
	pkg_search_module(Pango pango)
	pkg_search_module(Cairo cairo)
	pkg_search_module(PangoCairo pangocairo)
	pkg_search_module(PNG libpng)
else()
	find_package(Pango REQUIRED)
	find_package(Cairo REQUIRED)
	find_package(PangoCairo REQUIRED)
	find_package(PNG)
endif()

link_directories(
	${Pango_LIBRARY_DIRS}
	${Cairo_LIBRARY_DIRS}
	${PangoCairo_LIBRARY_DIRS}
)

include_directories(
	${GLib_INCLUDE_DIRS}
	${Pango_INCLUDE_DIRS}
	${Cairo_INCLUDE_DIRS}
	${PangoCairo_INCLUDE_DIRS}
)

if(PNG_FOUND)
	include_directories(${PNG_INCLUDE_DIRS})
	link_directories(${PNG_LIBRARY_DIRS})
endif()

find_package(libobs REQUIRED)
include(cmake/ObsPluginHelpers.cmake)

configure_file(
    src/plugin-macros.h.in
    ../src/plugin-macros.generated.h
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

add_library(${CMAKE_PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

target_link_libraries(${CMAKE_PROJECT_NAME}
	OBS::libobs
	${OBS_LIBRARIES}
	${Pango_LIBRARIES}
	${Cairo_LIBRARIES}
	${PangoCairo_LIBRARIES}
)

if(PNG_FOUND)
	target_link_libraries(${CMAKE_PROJECT_NAME} ${PNG_LIBRARIES})
endif()

if(OS_WINDOWS)
	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
	if (MSVC)
		add_definitions(/MP /d2FH4-)
	endif()

	target_link_libraries(${CMAKE_PROJECT_NAME} OBS::w32-pthreads)
endif()

if(OS_LINUX)
	target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -g -Wall -Wextra)
	target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,-z,defs)
	target_link_libraries(${CMAKE_PROJECT_NAME} m)
endif()

if(OS_MACOS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
endif()

setup_plugin_target(${CMAKE_PROJECT_NAME})

configure_file(installer/installer-macOS.pkgproj.in installer-macOS.generated.pkgproj)
