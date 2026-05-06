# Makefile para o projeto VCTP
# Instituto Politécnico do Cávado e do Ave - 2025/2026
# Compatível com Linux e Windows (via MSYS2/Cygwin)

# Detetar sistema operativo
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    EXE_EXT := .exe
    RM := del /Q
    COPY := copy
    MKDIR := mkdir
    SEP := \\
else
    DETECTED_OS := Linux
    EXE_EXT :=
    RM := rm -f
    COPY := cp
    MKDIR := mkdir -p
    SEP := /
endif

# Compiladores
CXX = g++
CC = gcc

# Flags do compilador
CXXFLAGS = -Wall -Wextra -std=c++17
CFLAGS = -Wall -Wextra

# OpenCV flags (via pkg-config no Linux)
ifeq ($(DETECTED_OS),Linux)
    OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4 2>/dev/null || echo "")
    OPENCV_LIBS := $(shell pkg-config --libs opencv4 2>/dev/null || echo "")
endif

# Diretórios
SRC_DIR = TrabPratico
LIB_DIR = lib

# Ficheiros
MAIN_SRC = $(SRC_DIR)/main.cpp
LIB_SRC = $(LIB_DIR)/vc.c
TARGET = trabpratico$(EXE_EXT)

# Include path
INCLUDES = -I $(LIB_DIR)

# Libraries
ifeq ($(DETECTED_OS),Linux)
    LIBS = $(OPENCV_LIBS) -lm
endif

# Targets
.PHONY: all clean run docker-build docker-run help detect-os

all: detect-os $(TARGET)

detect-os:
	@echo "Sistema detectado: $(DETECTED_OS)"

# Linux: compilar com gcc + g++
ifeq ($(DETECTED_OS),Linux)
$(TARGET): $(MAIN_SRC) $(LIB_SRC)
	$(CC) $(CFLAGS) -c -o vc.o $(LIB_SRC) $(INCLUDES) $(OPENCV_CFLAGS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(MAIN_SRC) vc.o $(INCLUDES) $(OPENCV_CFLAGS) $(LIBS)
	rm -f vc.o
endif

# Windows: compilar com MinGW/MSVC (OpenCV precisa de estar configurado)
ifeq ($(DETECTED_OS),Windows)
$(TARGET): $(MAIN_SRC) $(LIB_SRC)
	@echo "Para Windows, use o VSCode ou o Makefile.windows"
	@echo "Ou configure manualmente o OpenCV no Windows"
endif

clean:
	$(RM) $(TARGET) vc.o 2>/dev/null || true

run: $(TARGET)
ifeq ($(DETECTED_OS),Linux)
	cd $(SRC_DIR) && ../$(TARGET)
endif
ifeq ($(DETECTED_OS),Windows)
	cd $(SRC_DIR) && ..\\$(TARGET)
endif

docker-build:
	docker build -t vctp .

docker-run: docker-build
	docker run -it --rm -v $$(pwd):/project vctp

help:
	@echo "Targets disponíveis:"
	@echo "  make           - Compilar o projeto"
	@echo "  make clean     - Remover executável"
	@echo "  make run       - Compilar e executar"
	@echo "  make docker-build - Criar imagem Docker"
	@echo "  make docker-run   - Criar e executar contentor Docker"
	@echo "  make help      - Mostrar esta ajuda"
	@echo ""
	@echo "Sistema operativo detectado: $(DETECTED_OS)"
	@echo ""
	@echo "No Windows, use os ficheiros do VSCode ou o Makefile.windows"