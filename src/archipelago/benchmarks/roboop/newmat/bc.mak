.AUTODEPEND

TASM = TASM
TLIB = tlib
TLINK = tlink
LIBPATH = F:\BC5\LIB
INCLUDEPATH = F:\BC5\INCLUDE

CCX = bcc -ml -wpro -weas -wpre -I$(INCLUDEPATH) -D

CC = bcc -W- -v- -H- -x- -3 -N -Og -Oi -Ov -vGd- -vGt- -ml -f -I$(INCLUDEPATH)

everything: tmt.exe example.exe nl_ex.exe sl_ex.exe garch.exe test_exc.exe


.cpp.obj:
   $(CC) -c {$< }

OBJ_LIB = bandmat.obj cholesky.obj evalue.obj fft.obj hholder.obj      \
   jacobi.obj myexcept.obj newmat1.obj newmat2.obj newmat3.obj         \
   newmat4.obj newmat5.obj newmat6.obj newmat7.obj newmat8.obj         \
   newmat9.obj newmatex.obj newmatnl.obj newmatrm.obj solution.obj     \
   sort.obj submat.obj svd.obj

newmat.lib: $(OBJ_LIB)
   erase newmat.lib
   $(TLIB) newmat.lib /P32 +newmat1 +newmat2 +newmat3 +newmat4 +newmat5 +newmat6 
   $(TLIB) newmat.lib /P32+newmat7 +newmat8 +newmat9 +svd +fft +hholder +sort 
   $(TLIB) newmat.lib /P32+cholesky +submat +jacobi +evalue +bandmat +myexcept
   $(TLIB) newmat.lib /P32+solution +newmatnl+newmatrm+newmatex 

OBJ_T = tmt.obj tmt1.obj tmt2.obj tmt3.obj tmt4.obj tmt5.obj tmt6.obj       \
   tmt7.obj tmt8.obj tmt9.obj tmta.obj tmtb.obj tmtc.obj tmtd.obj tmte.obj  \
   tmtf.obj tmtg.obj tmth.obj tmti.obj tmtj.obj tmtk.obj


tmt.exe: newmat.lib $(OBJ_T)
   $(TLINK) /x/L$(LIBPATH) -Tde @&&|
c0l.obj+tmt.obj+tmt1.obj+tmt2.obj+tmt3.obj+tmt4.obj+tmt5.obj+tmt6.obj+
   tmt7.obj+tmt8.obj+tmt9.obj+tmta.obj+tmtb.obj+tmtc.obj+tmtd.obj+tmte.obj+
   tmtf.obj+tmtg.obj+tmth.obj+tmti.obj+tmtj.obj+tmtk.obj+newmat.lib
   tmt
      # no map file
   bidsl.lib+emu.lib+mathl.lib+cl.lib

|


example.exe: newmat.lib example.obj
   $(TLINK) /x/L$(LIBPATH) -Tde @&&|
c0l.obj+example.obj+newmat.lib
   example
      # no map file
   bidsl.lib+emu.lib+mathl.lib+cl.lib

|


nl_ex.exe: newmat.lib nl_ex.obj
   $(TLINK) /x/L$(LIBPATH) -Tde @&&|
c0l.obj+nl_ex.obj+newmat.lib
   nl_ex
      # no map file
   bidsl.lib+emu.lib+mathl.lib+cl.lib

|


sl_ex.exe: newmat.lib sl_ex.obj
   $(TLINK) /x/L$(LIBPATH) -Tde @&&|
c0l.obj+sl_ex.obj+newmat.lib
   sl_ex
      # no map file
   bidsl.lib+emu.lib+mathl.lib+cl.lib

|


garch.exe: newmat.lib garch.obj
   $(TLINK) /x/L$(LIBPATH) -Tde @&&|
c0l.obj+garch.obj+newmat.lib
   garch
      # no map file
   bidsl.lib+emu.lib+mathl.lib+cl.lib

|

test_exc.exe: newmat.lib test_exc.obj
   $(TLINK) /x/L$(LIBPATH) -Tde @&&|
c0l.obj+test_exc.obj+newmat.lib
   test_exc
      # no map file
   bidsl.lib+emu.lib+mathl.lib+cl.lib

|


bandmat.obj: bandmat.cpp 

cholesky.obj: cholesky.cpp 

evalue.obj: evalue.cpp 

myexcept.obj: myexcept.cpp 

fft.obj: fft.cpp 

hholder.obj: hholder.cpp 

jacobi.obj: jacobi.cpp 

newmat1.obj: newmat1.cpp 

newmat2.obj: newmat2.cpp 

newmat3.obj: newmat3.cpp 

newmat4.obj: newmat4.cpp 

newmat5.obj: newmat5.cpp 

newmat6.obj: newmat6.cpp 

newmat7.obj: newmat7.cpp 

newmat8.obj: newmat8.cpp 

newmat9.obj: newmat9.cpp 

newmatex.obj: newmatex.cpp 

newmatrm.obj: newmatrm.cpp 

newmatnl.obj: newmatnl.cpp 

sort.obj: sort.cpp 

submat.obj: submat.cpp 

svd.obj: svd.cpp 

example.obj: example.cpp 

tmt.obj: tmt.cpp 

tmt1.obj: tmt1.cpp 

tmt2.obj: tmt2.cpp 

tmt3.obj: tmt3.cpp 

tmt4.obj: tmt4.cpp 

tmt5.obj: tmt5.cpp 

tmt6.obj: tmt6.cpp 

tmt7.obj: tmt7.cpp 

tmt8.obj: tmt8.cpp 

tmt9.obj: tmt9.cpp 

tmta.obj: tmta.cpp 

tmtb.obj: tmtb.cpp 

tmtc.obj: tmtc.cpp 

tmtd.obj: tmtd.cpp 

tmte.obj: tmte.cpp 

tmtf.obj: tmtf.cpp 

tmtg.obj: tmtg.cpp 

tmth.obj: tmth.cpp

tmti.obj: tmti.cpp

tmtj.obj: tmtj.cpp

tmtk.obj: tmtk.cpp

nl_ex.obj: nl_ex.cpp

sl_ex.obj: sl_ex.cpp

solution.obj: solution.cpp

garch.obj: garch.cpp

test_exc.obj: test_exc.cpp


