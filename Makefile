SUFFIXES+=.d

CXX=clang++
LD=clang++
CXXFLAGS=-Iinclude \
	-Wall \
	-Wextra \
	-Werror \
	-DSDL_MAIN_HANDLED \
	-D_CRT_SECURE_NO_WARNINGS \
	-std=c++17 \
	-O0 -g
LDFLAGS=-Llib \
	-lSDL2main \
	-lSDL2 \
	-lSDL2_ttf \
	-lSDL2_image \
	-lglew32 \
	-lopengl32 \
	-mwindows -g

SRC=src
OBJ=obj
CXXFILES=$(wildcard $(SRC)/*.cpp)
OBJFILES=$(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(CXXFILES))

.PHONY: clean all

all: octree.exe

octree.exe: $(OBJFILES) Makefile
	$(LD) $(OBJFILES) $(LDFLAGS) -o $@

clean:
	del /f /q $(OBJ)\* octree.exe

$(OBJ)/%.d: $(SRC)/%.cpp Makefile
	$(CXX) $(CXXFLAGS) -MM -MT '$(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$<)' $< -MF $@

$(OBJ)/%.o: $(SRC)/%.cpp $(OBJ)/%.d Makefile
	$(CXX) $(CXXFLAGS) -o $@ -c $<


