# CMakeLists.txt para VCTP
# Instituto Politécnico do Cávado e do Ave - 2025/2026
# Compatível com Windows e Linux

cmake_minimum_required(VERSION 3.10)
project(VCTP LANGUAGES C CXX)

# Definir padrão C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detetar SO
if(WIN32)
    message(STATUS "Sistema: Windows")
    set(EXE_EXT ".exe")
else()
    message(STATUS "Sistema: Linux")
    set(EXE_EXT "")
endif()

# OpenCV - automático no Linux, manual no Windows
if(WIN32)
    # Windows: usa OPENCV_DIR ou caminho padrão
    set(OPENCV_DIR "C:/opencv/build/x64/mingw" CACHE PATH "OpenCV directory")
    set(OpenCV_INCLUDE_DIR "${OPENCV_DIR}/include")
    set(OpenCV_LIB_DIR "${OPENCV_DIR}/lib")
    include_directories(${OpenCV_INCLUDE_DIR})
    link_directories(${OpenCV_LIB_DIR})
    set(OpenCV_LIBS opencv_world4900)
else()
    # Linux: usa pkg-config
    find_package(OpenCV REQUIRED)
    include_directories(${OpenCV_INCLUDE_DIRS})
    set(OpenCV_LIBS ${OpenCV_LIBS})
endif()

# Fonte
set(SOURCES
    TrabPratico/main.cpp
    lib/vc.c
)

# Executável
add_executable(trabpratico${EXE_EXT} ${SOURCES})

# Libraries
target_link_libraries(trabpratico ${OpenCV_LIBS} m)

# Include directories
target_include_directories(trabpratico PRIVATE lib)

# Mensagem final
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")
message(STATUS "Build complete! Executável: trabpratico${EXE_EXT}")