# John Simko
# ENGI 3350 Lab 4

export

# default targets
all:  Lab4Main.gcc.exe Lab4Worker.gcc.exe

md5c.o: md5c.c
	gcc -c md5c.c -std=c99


zmqDir = "C:\Program Files\ZeroMQ 4.0.4"
			
zmqincLoc = $(zmqDir)"\include"

zmqLibLoc = $(zmqDir)"\bin"

Lab4Main.gcc.exe: Lab4Main.c
	gcc $^ -std=c99 -I $(zmqincLoc) -L $(zmqLibLoc) -llibzmq-v120-mt-4_0_4 -o $@

Lab4Worker.gcc.exe: Lab4Worker.c md5c.o
	gcc $^ -std=c99 -I $(zmqincLoc) -L $(zmqLibLoc) -llibzmq-v120-mt-4_0_4 -o $@


clean:
	rm -f *.o *.obj *.exe *.dll *.csv *.lib *.exp *.pdf
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
	
# gcc unoptimized build
#lab2Un.gcc.dll: uut.c
#	gcc $^ -shared -std=c99 -o $@

	
# gcc optimized build
#lab2Op.gcc.dll: uut.c
#	gcc $^ -shared -std=c99 -o $@ -O3

# batch used to configure Microsoft's compiler (MSCV)
#msenv = "$$VS140COMNTOOLS\..\..\VC\vcvarsall.bat"

# used to call Microsoft compiler, CL.exe
#mscc = cmd //C $(msenv) amd64 \&\& cl //LD

# microsoft unoptimized build
#lab2Un.ms.dll: uut.c
#	eval $$mscc $^ //Fe$@ //nologo

# microsoft optimized build
#lab2Op.ms.dll: uut.c
#	eval $$mscc $^ //Fe$@ //Ox //nologo

# batch file for configuring intel's compiler
#ienv = "$$ICPP_COMPILER16\bin\ipsxe-comp-vars.bat"

# used to call intel compiler, ICL.exe
#icc = cmd //C $(ienv) intel64 vs2015 quiet \&\& icl //LD //Qstd=c99

# intel compiler unoptimized
#lab2Un.int.dll: uut.c
#	eval $$icc $^ -o $@ //nologo

# intel compiler optimized
#lab2Op.int.dll: uut.c
#	eval $$icc $^ -o $@ //O3 //nologo

#convtest.exe: convtest.c
#	gcc $^ -std=c99 -Wall -o $@

#timetest.exe: timetest.c
#	gcc $^ -std=c99 -Wall -O0 -o $@

#runTimeTest: timetest.exe
#	./$^

#RPlots.pdf: Times.r Times.csv
#	"C:/Program Files/R/R-3.2.3/bin/x64/RScript.exe" $^
		
# used to clean out all outputs.

