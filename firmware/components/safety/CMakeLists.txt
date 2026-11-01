# Platform-independent logic, also built on the host (host/CMakeLists.txt).
idf_component_register(SRCS safety.c  INCLUDE_DIRS "include" REQUIRES cu_common hal profile)
