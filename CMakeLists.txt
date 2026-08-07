cmake_minimum_required(VERSION 3.16)

project(smart_cockpit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(BUILD_TESTING "Build unit tests" ON)
option(SMART_COCKPIT_USE_QT6 "Build against Qt6 instead of Qt5" OFF)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)

if(SMART_COCKPIT_USE_QT6)
  set(QT_PKG "Qt6")
  find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets DBus)
else()
  set(QT_PKG "Qt5")
  find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets DBus)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Core library (all non-main code; shared by the app and the unit tests)
# ---------------------------------------------------------------------------
add_library(smart_cockpit_core STATIC
  src/core/CanFrameParser.cpp
  src/core/CanManager.cpp
  src/core/CarService.cpp
  src/core/HealthMonitor.cpp
  src/logging/AsyncLogger.cpp
  src/media/BlueZScanner.cpp
  src/media/MediaService.cpp
  src/camera/CameraService.cpp
  src/camera/VideoWidget.cpp
  src/ui/ScreenManager.cpp
  src/ui/Theme.cpp
  src/ui/GaugeWidget.cpp
  src/ui/InstrumentPage.cpp
  src/ui/ClimatePage.cpp
  src/ui/MediaPage.cpp
  src/ui/ReverseCameraPage.cpp
  src/ui/MainWindow.cpp
)

target_include_directories(smart_cockpit_core PUBLIC
    src
    src/core
    src/logging
    src/media
    src/camera
    src/ui
)

target_link_libraries(smart_cockpit_core PUBLIC
  ${QT_PKG}::Widgets
  ${QT_PKG}::DBus
  PkgConfig::GST
  Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(smart_cockpit_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
add_executable(smart-cockpit src/main.cpp)
target_link_libraries(smart-cockpit PRIVATE smart_cockpit_core)

install(TARGETS smart-cockpit RUNTIME DESTINATION bin)
install(FILES deployment/smart-cockpit.service
        DESTINATION lib/systemd/system)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
