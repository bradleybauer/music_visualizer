# FileWatcher

## Description

FileWatcher is a C++ wrapper for OS file monitoring systems. Currently
it uses Win32 ReadDirectoryChangesW for monitoring changes in Windows,
and inotify in linux. OSX is supported via kqueue and directory scans.


## TODO

 * Create a pure directory scan based fallback mode.
 * Optimize the kqueue implementation.
 * Thorough UnitTest
 * Add proper Unicode support.


## Compiling

Use premake5 to generate a project for your build system.


## SimpleDemo

To run the demo, create a directory relative to the execution directory
called "test". Start SimpleDemo, then create/change/delete files inside
"test". If "test" does not exist when SimpleDemo starts, it will throw
an exception and exit.


## OgreDemo

Check the OgreDemo directory for an example integration with Ogre.


## Caveats

When some programs write data in Win32, they will generate both an Add,
and a Modify event. This is likely because the program is actually using
two separate calls to write its data.

Because of the time it takes to write the data to the file, it may be
necessary in some cases to wait a few milliseconds after the event to be
able to safely access the file's contents.


## Credits
Originally written by James Wynn
Contact: james@jameswynn.com

Fixes and Buffered/AsyncFileWatcher by Ian Diaz
Contact: shadowndacorner@gmail.com

James Wynn's original code can be located at:
http://simplefilewatcher.googlecode.com
