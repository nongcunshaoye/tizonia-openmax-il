## Building from source ##

To build and install from source, follow these steps (Ubuntu 16.04 is assumed,
but should work on other recent Debian-based distros).

### Dependencies ###

To install all the development dependencies, the easiest way is to use the
'tizonia-dev-build' tool. This script lives under the 'tools' directory and
maintains an up-to-date list of all the packages that are required in a
Debian-compatible system to build Tizonia from source.

> NOTE: The following command installs Mopidy's 'libspotify-dev' package, the
> 'gmusicapi', 'soundcloud', 'pafy' and 'youtube-dl python packages, among
> other things.

```bash

    # Setup the following environment variables
    $ export TIZONIA_REPO_DIR=/path/to/tizonia/repo # (e.g. /home/juan/work/tizonia)
    $ export TIZONIA_INSTALL_DIR=/path/to/install/basedir # (e.g. /home/juan/temp)
    $ export PATH=$TIZONIA_REPO_DIR/tools:$PATH

    $ cd $TIZONIA_REPO_DIR/tools


    # Install everything needed to build Tizonia on a Debian/Ubuntu system
    $ ./tizonia-dev-build --deps

```

### Building ###

After all the dependencies have been installed, build and install the Tizonia
OpenMAX IL framework, the IL plugins and the 'tizonia' command-line
application:

```bash

   $ cd $TIZONIA_REPO_DIR/tools

   # Configure all Tizonia sub-projects with 'release' flags, build and
   # install.
   $ ./tizonia-dev-build --release --install

   or

   # Configure with 'debug' flags, build and install.
   $ ./tizonia-dev-build --debug --install

```

Alternatively, from the top of Tizonia's repo, one can also do the familiar:

```bash

    $ autoreconf -ifs
    $ ./configure
    $ make
    $ make install

```

### Tizonia's configuration file ###

Copy *tizonia.conf* into the user's config folder:

```bash

    $ mkdir -p $HOME/.config/tizonia \
        && cp config/src/tizonia.conf $HOME/.config/tizonia

```

### Resource Manager's D-BUS service activation file (optional) ###

OpenMAX IL Resource Management is present but disabled by default. In case this
is to be used (prior to that, needs to be explicitly enabled in tizonia.conf),
copy the Resource Manager's D-BUS activation file to some place where it can be
found by the DBUS services. E.g:

```bash

    $ mkdir -p ~/.local/share/dbus-1/services \
        && cp rm/tizrmd/dbus/com.aratelia.tiz.rm.service ~/.local/share/dbus-1/services

```

### Known issues ###

The `tizonia` player app makes heavy use the the the
[Boost Meta State Machine (MSM)](http://www.boost.org/doc/libs/1_55_0/libs/msm/doc/HTML/index.html)
library by Christophe Henry (MSM is in turn based on
[Boost MPL](http://www.boost.org/doc/libs/1_56_0/libs/mpl/doc/index.html)).

MSM is used to generate a number of state machines that control the tunneled
OpenMAX IL components for the various playback uses cases. The state machines
are quite large and MSM is known for not being easy on the compilers. Building
the `tizonia` command-line app in 'debug' configuration (with debug symbols,
etc) requires quite a bit of RAM.

You may see GCC crashing like below; simply keep running `make -j1` or `make
-j1 install` until the application is fully built (it will finish eventually,
given the sufficient amount RAM). An alternative to that is to build in
'release' mode.

```bash

Making all in src
  CXX      tizonia-tizplayapp.o
  CXX      tizonia-main.o
  CXX      tizonia-tizomxutil.o
  CXX      tizonia-tizprogramopts.o
  CXX      tizonia-tizgraphutil.o
  CXX      tizonia-tizgraphcback.o
  CXX      tizonia-tizprobe.o
  CXX      tizonia-tizdaemon.o
  CXX      tizonia-tizplaylist.o
  CXX      tizonia-tizgraphfactory.o
  CXX      tizonia-tizgraphmgrcmd.o
  CXX      tizonia-tizgraphmgrops.o
  CXX      tizonia-tizgraphmgrfsm.o
  CXX      tizonia-tizgraphmgr.o
  CXX      tizonia-tizdecgraphmgr.o
g++: internal compiler error: Killed (program cc1plus)
Please submit a full bug report,
with preprocessed source if appropriate.
See <file:///usr/share/doc/gcc-4.8/README.Bugs> for instructions.
make[2]: *** [tizonia-tizplayapp.o] Error 4
make[2]: *** Waiting for unfinished jobs....
g++: internal compiler error: Killed (program cc1plus)
Please submit a full bug report,
with preprocessed source if appropriate.
See <file:///usr/share/doc/gcc-4.8/README.Bugs> for instructions.

```
