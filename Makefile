# Only use with nmake!

.PHONY: all

!IF [python -c "import os; list(map(lambda x: print('src/'+x+' ', end=''), filter(lambda x: x.endswith('.cpp'), os.listdir('src'))))" >cpp.txt] == 0
CPPFILES=\
!include cpp.txt
OBJFILES=$(subst src/,obj\,$(subst .cpp,.obj,$(CPPFILES)))
!IF [del /f cpp.txt] != 0
!ENDIF
!ENDIF

MAKEFILE=Makefile
CPP=cl
LD=link
CPPFLAGS=/Iinclude \
	/DSDL_MAIN_HANDLED \
	/D_CRT_SECURE_NO_WARNINGS \
	/std:c++17 \
	/Od \
	/W4 \
	/WX \
	/EHsc \
	/wd4458 /wd4201
LDFLAGS=/LIBPATH:lib \
	SDL2main.lib \
	SDL2.lib \
	SDL2_ttf.lib \
	SDL2_image.lib \
	glew32.lib \
	opengl32.lib

EXE=octree.exe

all: $(EXE)

clean:
	del /f /q obj\* octree.exe

rebuild: clean all

$(EXE): $(OBJFILES) $(MAKEFILE)
	$(LD) $(LDFLAGS) /out:$@ $(OBJFILES)

{src\}.cpp{obj\}.obj:
	$(CPP) $(CPPFLAGS) /c /Fo:$@ $< 
