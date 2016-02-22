Format:         1.0
Source:         tpower
Version:        0.1.0-1
Binary:         libtpower0, tpower-dev
Architecture:   any all
Maintainer:     John Doe <John.Doe@example.com>
Standards-Version: 3.9.5
Build-Depends: bison, debhelper (>= 8),
    pkg-config,
    automake,
    autoconf,
    libtool,
    libzmq4-dev,
    uuid-dev,
    libczmq-dev,
    libmlm-dev,
    libbiosproto-dev,
    libcxxtools-dev,
    dh-autoreconf

Package-List:
 libtpower0 deb net optional arch=any
 tpower-dev deb libdevel optional arch=any

