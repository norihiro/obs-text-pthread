cmake_minimum_required(VERSION 3.12)

project(obs-text-pthread VERSION 2.1.0
	LANGUAGES C
)

set(PLUGIN_AUTHOR "Norihiro Kamae")
set(MACOS_BUNDLEID "net.nagater.obs-text-pthread")
set(MACOS_PACKAGE_UUID "652BAFE7-E356-4A3F-9505-F89615E3A44A")
set(MACOS_INSTALLER_UUID "A4F0E91E-D759-472D-81D5-D1B0CE2CC2D8")
set(PLUGIN_URL "https://obsproject.com/forum/resources/pthread-text.1287/")
set(LINUX_MAINTAINER_EMAIL "norihiro@nagater.net")

option(ENABLE_COVERAGE "Enable coverage option for GCC" OFF)

# TAKE NOTE: No need to edit things past this point

# In case you need C++
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
	pkg_check_modules(PangoCairo REQUIRED IMPORTED_TARGET pangocairo)
	pkg_check_modules(PNG QUIET IMPORTED_TARGET libpng)
	add_library(PangoCairo ALIAS PkgConfig::PangoCairo)
	if(PNG_FOUND)
		add_library(PNG ALIAS PkgConfig::PNG)
	endif()
else()
	find_package(Pango REQUIRED)
	find_package(Cairo REQUIRED)
	target_link_libraries(PangoCairo INTERFACE Pango::Pango Cairo::Cairo)
endif()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	find_package(libobs REQUIRED)
	include(cmake/ObsPluginHelpers.cmake)
endif()

configure_file(
    src/plugin-macros.h.in
    plugin-macros.generated.h
)

set(PLUGIN_SOURCES
	src/obs-text-pthread-main.c
	src/obs-text-pthread-thread.c
)

add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

target_link_libraries(${PROJECT_NAME}
	OBS::libobs
	PangoCairo
)

target_include_directories(${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
)

if(PNG_FOUND)
	target_link_libraries(${PROJECT_NAME} PNG)
endif()

if(OS_WINDOWS)
	# Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
	if (MSVC)
		add_definitions(/MP /d2FH4-)
	endif()

	target_link_libraries(${PROJECT_NAME} OBS::w32-pthreads)
endif()

if(OS_LINUX)
	target_compile_options(${PROJECT_NAME} PRIVATE -g -Wall -Wextra)
	target_link_options(${PROJECT_NAME} PRIVATE -Wl,-z,defs)
	target_link_libraries(${PROJECT_NAME} m)

	if(ENABLE_COVERAGE)
		target_compile_options(${PROJECT_NAME} PRIVATE -coverage)
		target_link_options(${PROJECT_NAME} PRIVATE -coverage)
	endif()
endif()

if(OS_MACOS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fvisibility=default")

	set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
	set(MACOSX_PLUGIN_GUI_IDENTIFIER "${MACOS_BUNDLEID}")
	set(MACOSX_PLUGIN_BUNDLE_VERSION "${PROJECT_VERSION}")
	set(MACOSX_PLUGIN_SHORT_VERSION_STRING "1")
endif()

setup_plugin_target(${PROJECT_NAME})


if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
	configure_file(
	    ci/ci_includes.sh.in
	    ci/ci_includes.generated.sh
	)

	configure_file(
		installer/installer-macOS.pkgproj.in
		installer-macOS.generated.pkgproj
	)

	configure_file(
		ci/ci_includes.cmd.in
		ci/ci_includes.generated.cmd
	)
	configure_file(
	    installer/installer-Windows.iss.in
	    installer-Windows.generated.iss
	)
endif()
