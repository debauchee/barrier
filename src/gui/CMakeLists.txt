cmake_minimum_required (VERSION 3.4)

find_package (Qt5 COMPONENTS Core Widgets Network)
set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_INCLUDE_CURRENT_DIR ON)

file (GLOB GUI_SOURCE_FILES src/*.cpp src/*.h)
file (GLOB GUI_UI_FILES src/*.ui)

if (WIN32)
    set (GUI_RC_FILES res/win/Barrier.rc)
endif()

add_executable (barrier WIN32
    ${GUI_SOURCE_FILES}
    ${GUI_UI_FILES}
    ${GUI_RC_FILES}
    res/Barrier.qrc
)

include_directories (./src)

target_compile_definitions (barrier PRIVATE -DBARRIER_VERSION_STAGE="${BARRIER_VERSION_STAGE}")
target_compile_definitions (barrier PRIVATE -DBARRIER_REVISION="${BARRIER_REVISION}")

if (WIN32)
    include_directories ($ENV{BONJOUR_SDK_HOME}/Include)
    find_library (DNSSD_LIB dnssd.lib
                  HINTS ENV BONJOUR_SDK_HOME
                  PATH_SUFFIXES "Lib/x64")
    set_target_properties (barrier PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT")
    target_link_libraries (barrier ${DNSSD_LIB})
elseif (APPLE)
    find_library(APPSERVICES_LIB ApplicationServices)
    target_link_libraries(barrier ${APPSERVICES_LIB})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR
        ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_libraries (barrier dns_sd)
endif()

if (HAVE_X11)
    target_link_libraries (barrier X11)
endif()

target_link_libraries (barrier common)
target_link_libraries (barrier Qt5::Core Qt5::Widgets Qt5::Network)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    install (TARGETS barrier DESTINATION ${BARRIER_BUNDLE_BINARY_DIR})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    install (TARGETS barrier DESTINATION bin)
endif()
