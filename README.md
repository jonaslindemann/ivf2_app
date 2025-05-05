# Ivf2 template application

This is a template application for the Ivf2 framework. It is a simple web application that demonstrates the use of the Ivf2 framework and its features.

## Configure and build the application (Windows)

To configure the application you need to run CMake with the following options:

```bash
cmake --preset windows
```

To configure a debug build, use the following command:

```bash
cmake --preset windows-debug
```

To build a release version of the application, use the following command:

```bash
cmake --build --preset release
```

To build a debug version of the application, use the following command:

```bash
cmake --build --preset debug
```

There is also a windows batch file that can be used to build the application. To use it, run the following command:

```bash
build_windows.bat [debug|release] [path\to\ivf2]
```

This will build the application in the specified mode (debug or release) and use the specified path to the Ivf2 framework. If no path is specified, it will use the default path which is `..\ivf2`.

## Configure and build the application (Linux)

To configure the application you need to run CMake with the following options:

```bash
cmake --preset linux
```
To configure a debug build, use the following command:

```bash
cmake --preset linux-debug
```
To build a release version of the application, use the following command:

```bash
cmake --build --preset release
```
To build a debug version of the application, use the following command:

```bash
cmake --build --preset debug
```
