Improvement: Migrate CMake build system away from using `find_package(PythonInterpreter)`, deprecated since CMake 3.12. Use `find_package(Python3 COMPONENTS Interpreter)` instead.
