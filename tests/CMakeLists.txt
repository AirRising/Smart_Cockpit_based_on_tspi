if(SMART_COCKPIT_USE_QT6)
  find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
  set(QT_TEST_PKG "Qt6")
else()
  find_package(Qt5 5.12 REQUIRED COMPONENTS Test)
  set(QT_TEST_PKG "Qt5")
endif()

add_executable(tst_canframeparser tst_canframeparser.cpp)
target_link_libraries(tst_canframeparser PRIVATE smart_cockpit_core ${QT_TEST_PKG}::Test)
add_test(NAME tst_canframeparser COMMAND tst_canframeparser)

add_executable(tst_climateservice tst_climateservice.cpp)
target_link_libraries(tst_climateservice PRIVATE smart_cockpit_core ${QT_TEST_PKG}::Test)
add_test(NAME tst_climateservice COMMAND tst_climateservice)
set_tests_properties(tst_climateservice PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ScreenManager page-priority / preemption logic (headless).
add_executable(tst_screenmanager tst_screenmanager.cpp)
target_link_libraries(tst_screenmanager PRIVATE smart_cockpit_core ${QT_TEST_PKG}::Test)
add_test(NAME tst_screenmanager COMMAND tst_screenmanager)
set_tests_properties(tst_screenmanager PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# UI smoke tests: build the real pages, drive them through a CarService and
# render each one offscreen. No board / CAN bus required.
add_executable(tst_ui tst_ui.cpp)
target_link_libraries(tst_ui PRIVATE smart_cockpit_core ${QT_TEST_PKG}::Test)
add_test(NAME tst_ui COMMAND tst_ui)
set_tests_properties(tst_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
