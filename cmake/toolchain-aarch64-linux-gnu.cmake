# Cross-compilation toolchain for the TaisanPi RK3566 (aarch64, Debian 11/12).
#
# Usage:
#   export SDK_SYSROOT=/opt/taisanpi/sysroot      # Debian rootfs for aarch64
#   export SDK_QT_ROOT=/opt/taisanpi/qt5          # Qt for aarch64 (optional)
#   cmake -B build-arm \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake \
#         -DSMART_COCKPIT_USE_QT6=OFF
#
# SDK_SYSROOT must contain:
#   usr/include, usr/lib/aarch64-linux-gnu, usr/lib/aarch64-linux-gnu/pkgconfig,
#   usr/share/pkgconfig, usr/lib/aarch64-linux-gnu/gstreamer-1.0

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(TOOLCHAIN_PREFIX aarch64-linux-gnu)
set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)

if(NOT DEFINED ENV{SDK_SYSROOT})
  message(FATAL_ERROR "SDK_SYSROOT is not set. Export it to the aarch64 Debian rootfs.")
endif()
set(CMAKE_SYSROOT $ENV{SDK_SYSROOT})

# Qt installed into the sysroot, or provided separately via SDK_QT_ROOT.
if(DEFINED ENV{SDK_QT_ROOT})
  set(CMAKE_FIND_ROOT_PATH $ENV{SDK_SYSROOT} $ENV{SDK_QT_ROOT})
else()
  set(CMAKE_FIND_ROOT_PATH $ENV{SDK_SYSROOT})
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Let pkg-config find the target's .pc files inside the sysroot.
set(ENV{PKG_CONFIG_LIBDIR}
    "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")

set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu")
