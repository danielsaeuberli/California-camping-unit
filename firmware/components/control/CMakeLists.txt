# Platform-independent logic, also built on the host (host/CMakeLists.txt).
idf_component_register(SRCS control.c  INCLUDE_DIRS "include" REQUIRES cu_common hal profile signals modules safety state_bus)
