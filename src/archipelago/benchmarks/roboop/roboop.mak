# Roboop Makefile for Borland C++ 
# $Id: roboop.mak,v 1.1 2003/10/17 02:39:00 emery Exp $
#
.AUTODEPEND


#
# Borland C++ tools
#
CC   = bcc32 
LINK = TLink32
AR   = TLib

#
# Options
#
CFLAGS = -R -v -vi -H- -Inewmat;source

#
# Dependency List
#
deproboop = \
   demo.exe\
   bench.exe

roboop : $(deproboop)
  echo MakeNode 

depdemodexe = \
   roboop.lib\
   newmat.lib\
   source\demo.cpp

demo.exe : $(depdemodexe)
    $(CC)  $(CFLAGS) -e$@ source\demo.cpp roboop.lib newmat.lib

deproboopdlib = \
   gnugraph.obj\
   comp_dq.obj\
   comp_dqp.obj\
   delta_t.obj\
   dynamics.obj\
   homogen.obj\
   kinemat.obj\
   robot.obj\
   sensitiv.obj\
   utils.obj

roboop.lib : $(deproboopdlib)
  $(AR) $< @&&|
 -+gnugraph.obj &
-+comp_dq.obj &
-+comp_dqp.obj &
-+delta_t.obj &
-+dynamics.obj &
-+homogen.obj &
-+kinemat.obj &
-+robot.obj &
-+sensitiv.obj &
-+utils.obj
|

gnugraph.obj :  source\gnugraph.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

comp_dq.obj :  source\comp_dq.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

comp_dqp.obj :  source\comp_dqp.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

delta_t.obj :  source\delta_t.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

dynamics.obj :  source\dynamics.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

homogen.obj :  source\homogen.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

kinemat.obj :  source\kinemat.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

robot.obj :  source\robot.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

sensitiv.obj :  source\sensitiv.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

utils.obj :  source\utils.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

depnewmatdlib = \
   bandmat.obj\
   cholesky.obj\
   evalue.obj\
   fft.obj\
   hholder.obj\
   jacobi.obj\
   myexcept.obj\
   newmat1.obj\
   newmat2.obj\
   newmat3.obj\
   newmat4.obj\
   newmat5.obj\
   newmat6.obj\
   newmat7.obj\
   newmat8.obj\
   newmat9.obj\
   newmatex.obj\
   newmatnl.obj\
   newmatrm.obj\
   solution.obj\
   sort.obj\
   submat.obj\
   svd.obj

newmat.lib : $(depnewmatdlib)
  $(AR) /P32 $< @&&|
 -+bandmat.obj &
-+cholesky.obj &
-+evalue.obj &
-+fft.obj &
-+hholder.obj &
-+jacobi.obj &
-+myexcept.obj &
-+newmat1.obj &
-+newmat2.obj &
-+newmat3.obj &
-+newmat4.obj &
-+newmat5.obj &
-+newmat6.obj &
-+newmat7.obj &
-+newmat8.obj &
-+newmat9.obj &
-+newmatex.obj &
-+newmatnl.obj &
-+newmatrm.obj &
-+solution.obj &
-+sort.obj &
-+submat.obj &
-+svd.obj
|

bandmat.obj :  newmat\bandmat.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

cholesky.obj :  newmat\cholesky.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

evalue.obj :  newmat\evalue.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

fft.obj :  newmat\fft.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

hholder.obj :  newmat\hholder.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

jacobi.obj :  newmat\jacobi.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

myexcept.obj :  newmat\myexcept.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat1.obj :  newmat\newmat1.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat2.obj :  newmat\newmat2.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat3.obj :  newmat\newmat3.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat4.obj :  newmat\newmat4.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat5.obj :  newmat\newmat5.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat6.obj :  newmat\newmat6.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat7.obj :  newmat\newmat7.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat8.obj :  newmat\newmat8.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmat9.obj :  newmat\newmat9.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmatex.obj :  newmat\newmatex.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmatnl.obj :  newmat\newmatnl.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

newmatrm.obj :  newmat\newmatrm.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

solution.obj :  newmat\solution.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

sort.obj :  newmat\sort.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

submat.obj :  newmat\submat.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

svd.obj :  newmat\svd.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

demo.obj :  source\demo.cpp
  $(CC)  $(CFLAGS) -c -o$@ $?

depbenchdexe = \
   newmat.lib\
   roboop.lib\
   source\bench.cpp

bench.exe : $(depbenchdexe)
    $(CC)  $(CFLAGS) -e$@ source\bench.cpp newmat.lib roboop.lib

clean:
  del *.obj
  del *.lib

veryclean: clean
  del *.exe


