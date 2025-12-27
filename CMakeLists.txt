cmake_minimum_required(VERSION 3.16)
project(mpv_player VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC ON)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)

# Find MPV using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

# Add executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/mpvwidget.cpp
    src/mpvwidget.h
    src/controlbar.cpp
    src/controlbar.h
)

# resource embedding
qt6_add_resources(RES_FILES resources.qrc)
target_sources(mpv_player PRIVATE ${RES_FILES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGLWidgets
    PkgConfig::MPV
    OpenGL::GL
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
# --------------------------------------------------------
# Install target and resources
# --------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS mpv_player
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES resources.qrc
    DESTINATION ${CMAKE_INSTALL_DATADIR}/mpv_player
)
