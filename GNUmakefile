# Only use with GNU make!

CLANG_CPP=clang++
CLANG_LD=clang++
CLANG_CPPFLAGS=-Iinclude \
	-Wall \
	-Wextra \
	-Werror \
	-DSDL_MAIN_HANDLED \
	-D_CRT_SECURE_NO_WARNINGS \
	-std=c++17 \
	-O0 \
	-fno-exceptions \
	-g
CLANG_LDFLAGS=-Llib \
	-lSDL2main \
	-lSDL2 \
	-lSDL2_ttf \
	-lSDL2_image \
	-lglew32 \
	-lopengl32 \
	-mwindows \
	-g

CPP=$(CLANG_CPP)
LD=$(CLANG_LD)
CPPFLAGS=$(CLANG_CPPFLAGS)
LDFLAGS=$(CLANG_LDFLAGS)
MAKEFILE=GNUmakefile

SRC=src
OBJ=obj
CXXFILES=$(wildcard $(SRC)/*.cpp)
OBJFILES=$(patsubst $(SRC)/%.cpp,$(OBJ)/%.o,$(CXXFILES))

.PHONY: clean all

all: octree.exe

octree.exe: $(OBJFILES) $(MAKEFILE)
	$(LD) $(OBJFILES) $(LDFLAGS) -o $@

clean:
	del /f /q $(OBJ)\* octree.exe

$(OBJ)/%.o: $(SRC)/%.cpp $(MAKEFILE)
	$(CPP) $(CPPFLAGS) -o $@ -c $<


