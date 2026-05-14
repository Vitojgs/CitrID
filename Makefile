# Compiladores
CXX = g++

# Flags e Bibliotecas (Otimizado para o teu MSYS2)
CXXFLAGS = -O3 -Wall
INCLUDES = -I. -I/mingw64/include/opencv4
LIBS = -L/mingw64/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_videoio -lopencv_video

# Ficheiros (Todos na mesma pasta ou em lib/)
TARGET = main.exe
SRCS = main.cpp lib/vc.c lib/tracking.cpp lib/utils.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(INCLUDES) $(LIBS) -mconsole

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)