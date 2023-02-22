Name: @PLUGIN_NAME@
Version: @VERSION@
Release: @RELEASE@%{?dist}
Summary: Text plugin using pango in separated thread for OBS Studio
License: GPLv2+

Source0: %{name}-%{version}.tar.bz2
Requires: obs-studio >= @OBS_VERSION@
BuildRequires: cmake, gcc, gcc-c++
BuildRequires: obs-studio-devel
BuildRequires: pango pango-devel

%description
Text plugin for OBS Studio using pango with mark-up option.

%prep
%autosetup -p1

%build
%{cmake} -DLINUX_PORTABLE=OFF -DLINUX_RPATH=OFF
%{cmake_build}

%install
%{cmake_install}

%files
%{_libdir}/obs-plugins/%{name}.so
%{_datadir}/obs/obs-plugins/%{name}/
