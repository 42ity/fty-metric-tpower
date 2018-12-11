#
#    fty-metric-tpower - 42ity component for power metrics computation
#
#    Copyright (C) 2014 - 2018 Eaton
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# To build with draft APIs, use "--with drafts" in rpmbuild for local builds or add
#   Macros:
#   %_with_drafts 1
# at the BOTTOM of the OBS prjconf
%bcond_with drafts
%if %{with drafts}
%define DRAFTS yes
%else
%define DRAFTS no
%endif
%define SYSTEMD_UNIT_DIR %(pkg-config --variable=systemdsystemunitdir systemd)
Name:           fty-metric-tpower
Version:        1.0.0
Release:        1
Summary:        42ity component for power metrics computation
License:        GPL-2.0+
URL:            https://42ity.org
Source0:        %{name}-%{version}.tar.gz
Group:          System/Libraries
# Note: ghostscript is required by graphviz which is required by
#       asciidoc. On Fedora 24 the ghostscript dependencies cannot
#       be resolved automatically. Thus add working dependency here!
BuildRequires:  ghostscript
BuildRequires:  asciidoc
BuildRequires:  automake
BuildRequires:  autoconf
BuildRequires:  libtool
BuildRequires:  pkgconfig
BuildRequires:  systemd-devel
BuildRequires:  systemd
%{?systemd_requires}
BuildRequires:  xmlto
BuildRequires:  gcc-c++
BuildRequires:  libsodium-devel
BuildRequires:  zeromq-devel
BuildRequires:  czmq-devel
BuildRequires:  malamute-devel
BuildRequires:  cxxtools-devel
BuildRequires:  log4cplus-devel
BuildRequires:  fty-common-logging-devel
BuildRequires:  fty-proto-devel
BuildRequires:  openssl-devel
BuildRequires:  fty-common-devel
BuildRequires:  fty-common-mlm-devel
BuildRequires:  tntdb-devel
BuildRequires:  fty-common-db-devel
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
fty-metric-tpower 42ity component for power metrics computation.

%package -n libfty_metric_tpower0
Group:          System/Libraries
Summary:        42ity component for power metrics computation shared library

%description -n libfty_metric_tpower0
This package contains shared library for fty-metric-tpower: 42ity component for power metrics computation

%post -n libfty_metric_tpower0 -p /sbin/ldconfig
%postun -n libfty_metric_tpower0 -p /sbin/ldconfig

%files -n libfty_metric_tpower0
%defattr(-,root,root)
%doc COPYING
%{_libdir}/libfty_metric_tpower.so.*

%package devel
Summary:        42ity component for power metrics computation
Group:          System/Libraries
Requires:       libfty_metric_tpower0 = %{version}
Requires:       libsodium-devel
Requires:       zeromq-devel
Requires:       czmq-devel
Requires:       malamute-devel
Requires:       cxxtools-devel
Requires:       log4cplus-devel
Requires:       fty-common-logging-devel
Requires:       fty-proto-devel
Requires:       openssl-devel
Requires:       fty-common-devel
Requires:       fty-common-mlm-devel
Requires:       tntdb-devel
Requires:       fty-common-db-devel

%description devel
42ity component for power metrics computation development tools
This package contains development files for fty-metric-tpower: 42ity component for power metrics computation

%files devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libfty_metric_tpower.so
%{_libdir}/pkgconfig/libfty_metric_tpower.pc
%{_mandir}/man3/*
%{_mandir}/man7/*

%prep

%setup -q

%build
sh autogen.sh
%{configure} --enable-drafts=%{DRAFTS} --with-systemd-units
make %{_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

# remove static libraries
find %{buildroot} -name '*.a' | xargs rm -f
find %{buildroot} -name '*.la' | xargs rm -f

%files
%defattr(-,root,root)
%doc README.md
%doc COPYING
%{_bindir}/fty-metric-tpower
%{_mandir}/man1/fty-metric-tpower*
%config(noreplace) %{_sysconfdir}/fty-metric-tpower/fty-metric-tpower.cfg
%{SYSTEMD_UNIT_DIR}/fty-metric-tpower.service
%dir %{_sysconfdir}/fty-metric-tpower
%if 0%{?suse_version} > 1315
%post
%systemd_post fty-metric-tpower.service
%preun
%systemd_preun fty-metric-tpower.service
%postun
%systemd_postun_with_restart fty-metric-tpower.service
%endif

%changelog
