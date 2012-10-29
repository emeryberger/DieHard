!include <ntwin32.mak>

.SUFFIXES: .cpp


everything:   tmt.exe example.exe nl_ex.exe sl_ex.exe garch.exe test_exc.exe

.cpp.obj:
	      cl -c -W3 -Ox  $*.cpp

OBJ_LIB = bandmat.obj cholesky.obj evalue.obj fft.obj hholder.obj     \
  jacobi.obj myexcept.obj newmat1.obj newmat2.obj newmat3.obj         \
  newmat4.obj newmat5.obj newmat6.obj newmat7.obj newmat8.obj         \
  newmat9.obj newmatex.obj newmatnl.obj newmatrm.obj solution.obj     \
  sort.obj submat.obj svd.obj

OBJ_T = tmt.obj tmt1.obj tmt2.obj tmt3.obj tmt4.obj tmt5.obj tmt6.obj      \
  tmt7.obj tmt8.obj tmt9.obj tmta.obj tmtb.obj tmtc.obj tmtd.obj tmte.obj  \
  tmtf.obj tmtg.obj tmth.obj tmti.obj tmtj.obj tmtk.obj

tmt.exe:    	$(OBJ_T) $(OBJ_LIB)
                link -OUT:tmt.exe $(conlflags) $(conlibs) $(OBJ_T) $(OBJ_LIB)

OBJ_E = example.obj

example.exe:    $(OBJ_E) $(OBJ_LIB)
                link -OUT:example.exe $(conlflags) $(conlibs) $(OBJ_E) $(OBJ_LIB)

OBJ_N = nl_ex.obj

nl_ex.exe:    	$(OBJ_N) $(OBJ_LIB)
                link -OUT:nl_ex.exe $(conlflags) $(conlibs) $(OBJ_N) $(OBJ_LIB)

OBJ_S = sl_ex.obj

sl_ex.exe:    	$(OBJ_S) $(OBJ_LIB)
                link -OUT:sl_ex.exe $(conlflags) $(conlibs) $(OBJ_S) $(OBJ_LIB)

OBJ_G = garch.obj

garch.exe:      $(OBJ_G) $(OBJ_LIB)
                link -OUT:garch.exe $(conlflags) $(conlibs) $(OBJ_G) $(OBJ_LIB)

OBJ_X = test_exc.obj

test_exc.exe:	$(OBJ_X) $(OBJ_LIB)
		link -OUT:test_exc.exe $(conlflags) $(conlibs) $(OBJ_X) $(OBJ_LIB)	

newmatxx = include.h newmat.h boolean.h myexcept.h

myexcept.obj:   include.h boolean.h myexcept.h myexcept.cpp

newmatex.obj:   $(newmatxx) newmatex.cpp

newmatnl.obj:   $(newmatxx) newmatnl.h newmatap.h

example.obj:    $(newmatxx) newmatap.h example.cpp

cholesky.obj:   $(newmatxx) cholesky.cpp

evalue.obj:     $(newmatxx) newmatrm.h precisio.h evalue.cpp

fft.obj:        $(newmatxx) newmatap.h fft.cpp

hholder.obj:    $(newmatxx) newmatap.h hholder.cpp

jacobi.obj:     $(newmatxx) precisio.h newmatrm.h jacobi.cpp

bandmat.obj:    $(newmatxx) newmatrc.h controlw.h bandmat.cpp

newmat1.obj:    $(newmatxx) newmat1.cpp

newmat2.obj:    $(newmatxx) newmatrc.h controlw.h newmat2.cpp

newmat3.obj:    $(newmatxx) newmatrc.h controlw.h newmat3.cpp

newmat4.obj:    $(newmatxx) newmatrc.h controlw.h newmat4.cpp

newmat5.obj:    $(newmatxx) newmatrc.h controlw.h newmat5.cpp

newmat6.obj:    $(newmatxx) newmatrc.h controlw.h newmat6.cpp

newmat7.obj:    $(newmatxx) newmatrc.h controlw.h newmat7.cpp

newmat8.obj:    $(newmatxx) newmatap.h newmat8.cpp

newmat9.obj:    $(newmatxx) newmatrc.h controlw.h newmatio.h newmat9.cpp

newmatrm.obj:   $(newmatxx) newmatrm.h newmatrm.cpp

sort.obj:       $(newmatxx) newmatap.h sort.cpp

submat.obj:     $(newmatxx) newmatrc.h controlw.h submat.cpp

svd.obj:        $(newmatxx) newmatrm.h precisio.h svd.cpp

tmt.obj:        $(newmatxx) newmatap.h tmt.cpp 

tmt1.obj:       $(newmatxx) newmatap.h tmt1.cpp 

tmt2.obj:       $(newmatxx) newmatap.h tmt2.cpp 

tmt3.obj:       $(newmatxx) newmatap.h tmt3.cpp 

tmt4.obj:       $(newmatxx) newmatap.h tmt4.cpp 

tmt5.obj:       $(newmatxx) newmatap.h tmt5.cpp 

tmt6.obj:       $(newmatxx) newmatap.h tmt6.cpp 

tmt7.obj:       $(newmatxx) newmatap.h tmt7.cpp 

tmt8.obj:       $(newmatxx) newmatap.h tmt8.cpp 

tmt9.obj:       $(newmatxx) newmatap.h tmt9.cpp 

tmta.obj:       $(newmatxx) newmatap.h tmta.cpp 

tmtb.obj:       $(newmatxx) newmatap.h tmtb.cpp 

tmtc.obj:       $(newmatxx) newmatap.h tmtc.cpp 

tmtd.obj:       $(newmatxx) newmatap.h tmtd.cpp 

tmte.obj:       $(newmatxx) newmatap.h tmte.cpp 

tmtf.obj:       $(newmatxx) newmatap.h tmtf.cpp 

tmtg.obj:       $(newmatxx) newmatap.h tmtg.cpp 

tmth.obj:       $(newmatxx) newmatap.h tmth.cpp

tmti.obj:       $(newmatxx) newmatap.h tmti.cpp

tmtj.obj:       $(newmatxx) newmatap.h tmtj.cpp

tmtk.obj:       $(newmatxx) newmatap.h tmtk.cpp

nl_ex.obj:      $(newmatxx) newmatap.h newmatnl.h nl_ex.cpp

sl_ex.obj:      include.h boolean.h myexcept.h sl_ex.cpp

solution.obj:   include.h boolean.h myexcept.h solution.cpp

garch.obj:      $(newmatxx) newmatap.h newmatnl.h garch.cpp

test_exc.obj:   $(newmatxx) test_exc.cpp
