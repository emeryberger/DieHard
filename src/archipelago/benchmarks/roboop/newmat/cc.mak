CXX = CC
CXXFLAGS = -O2 

everything:   libnewmat.a tmt example nl_ex sl_ex garch test_exc


%.o:          %.cpp
	      rm -f -i $*.cxx
	      ln $*.cpp $*.cxx  
	      $(CXX) $(CXXFLAGS) -c $*.cxx
	      rm $*.cxx  

OBJ_LIB = bandmat.o cholesky.o evalue.o fft.o hholder.o     \
  jacobi.o myexcept.o newmat1.o newmat2.o newmat3.o         \
  newmat4.o newmat5.o newmat6.o newmat7.o newmat8.o         \
  newmat9.o newmatex.o newmatnl.o newmatrm.o solution.o     \
  sort.o submat.o svd.o

libnewmat.a:	$(OBJ_LIB)
	        $(AR) cr $@ $(OBJ_LIB)
	        ranlib $@

OBJ_T = tmt.o tmt1.o tmt2.o tmt3.o tmt4.o tmt5.o tmt6.o    \
  tmt7.o tmt8.o tmt9.o tmta.o tmtb.o tmtc.o tmtd.o tmte.o  \
  tmtf.o tmtg.o tmth.o tmti.o tmtj.o tmtk.o

tmt:    	$(OBJ_T) libnewmat.a
	        $(CXX) -o $@ $(OBJ_T) -L. -lnewmat -lm

OBJ_E = example.o

example:    	$(OBJ_E) libnewmat.a
	        $(CXX) -o $@ $(OBJ_E) -L. -lnewmat -lm

OBJ_N = nl_ex.o

nl_ex:    	$(OBJ_N) libnewmat.a
	        $(CXX) -o $@ $(OBJ_N) -L. -lnewmat -lm

OBJ_S = sl_ex.o

sl_ex:    	$(OBJ_S) libnewmat.a
	        $(CXX) -o $@ $(OBJ_S) -L. -lnewmat -lm

OBJ_G = garch.o

garch:          $(OBJ_G) libnewmat.a
	        $(CXX) -o $@ $(OBJ_G) -L. -lnewmat -lm

OBJ_X = test_exc.o

test_exc:	$(OBJ_X) libnewmat.a
		$(CXX) -o $@ $(OBJ_X) -L. -lnewmat -lm	

newmatxx = include.h newmat.h boolean.h myexcept.h

myexcept.o:   include.h boolean.h myexcept.h myexcept.cpp

newmatex.o:   $(newmatxx) newmatex.cpp

newmatnl.o:   $(newmatxx) newmatnl.h newmatap.h

example.o:    $(newmatxx) newmatap.h example.cpp

cholesky.o:   $(newmatxx) cholesky.cpp

evalue.o:     $(newmatxx) newmatrm.h precisio.h evalue.cpp

fft.o:        $(newmatxx) newmatap.h fft.cpp

hholder.o:    $(newmatxx) newmatap.h hholder.cpp

jacobi.o:     $(newmatxx) precisio.h newmatrm.h jacobi.cpp

bandmat.o:    $(newmatxx) newmatrc.h controlw.h bandmat.cpp

newmat1.o:    $(newmatxx) newmat1.cpp

newmat2.o:    $(newmatxx) newmatrc.h controlw.h newmat2.cpp

newmat3.o:    $(newmatxx) newmatrc.h controlw.h newmat3.cpp

newmat4.o:    $(newmatxx) newmatrc.h controlw.h newmat4.cpp

newmat5.o:    $(newmatxx) newmatrc.h controlw.h newmat5.cpp

newmat6.o:    $(newmatxx) newmatrc.h controlw.h newmat6.cpp

newmat7.o:    $(newmatxx) newmatrc.h controlw.h newmat7.cpp

newmat8.o:    $(newmatxx) newmatap.h newmat8.cpp

newmat9.o:    $(newmatxx) newmatrc.h controlw.h newmatio.h newmat9.cpp

newmatrm.o:   $(newmatxx) newmatrm.h newmatrm.cpp

sort.o:       $(newmatxx) newmatap.h sort.cpp

submat.o:     $(newmatxx) newmatrc.h controlw.h submat.cpp

svd.o:        $(newmatxx) newmatrm.h precisio.h svd.cpp

tmt.o:        $(newmatxx) newmatap.h tmt.cpp 

tmt1.o:       $(newmatxx) newmatap.h tmt1.cpp 

tmt2.o:       $(newmatxx) newmatap.h tmt2.cpp 

tmt3.o:       $(newmatxx) newmatap.h tmt3.cpp 

tmt4.o:       $(newmatxx) newmatap.h tmt4.cpp 

tmt5.o:       $(newmatxx) newmatap.h tmt5.cpp 

tmt6.o:       $(newmatxx) newmatap.h tmt6.cpp 

tmt7.o:       $(newmatxx) newmatap.h tmt7.cpp 

tmt8.o:       $(newmatxx) newmatap.h tmt8.cpp 

tmt9.o:       $(newmatxx) newmatap.h tmt9.cpp 

tmta.o:       $(newmatxx) newmatap.h tmta.cpp 

tmtb.o:       $(newmatxx) newmatap.h tmtb.cpp 

tmtc.o:       $(newmatxx) newmatap.h tmtc.cpp 

tmtd.o:       $(newmatxx) newmatap.h tmtd.cpp 

tmte.o:       $(newmatxx) newmatap.h tmte.cpp 

tmtf.o:       $(newmatxx) newmatap.h tmtf.cpp 

tmtg.o:       $(newmatxx) newmatap.h tmtg.cpp 

tmth.o:       $(newmatxx) newmatap.h tmth.cpp

tmti.o:       $(newmatxx) newmatap.h tmti.cpp

tmtj.o:       $(newmatxx) newmatap.h tmtj.cpp

tmtk.o:       $(newmatxx) newmatap.h tmtk.cpp

nl_ex.o:      $(newmatxx) newmatap.h newmatnl.h nl_ex.cpp

sl_ex.o:      include.h boolean.h myexcept.h sl_ex.cpp

solution.o:   include.h boolean.h myexcept.h solution.cpp

garch.o:      $(newmatxx) newmatap.h newmatnl.h garch.cpp

test_exc.o:   $(newmatxx) test_exc.cpp
