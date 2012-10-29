# Roboop Makefile for GNU g++
# $Id: roboop.gnu,v 1.2 2004/05/17 15:43:34 emery Exp $

CC = g++
CFLAGS = -DNDEBUG -O3 -c -D bool_LIB -I ./source\
         -I ./newmat -I /usr/include/g++-include 
LIBS = -L./ -lroboop -lnewmat -lm
AR = ar

All: demo bench libnewmat.a libroboop.a


Depdemo = libroboop.a libnewmat.a demo.o

demo: $(Depdemo)
	$(CC) demo.o -o  demo $(LIBS)

demo.o :  source/demo.cpp
	$(CC) $(CFLAGS) -o $@ $?

Deprobooplib = \
   gnugraph.o   comp_dq.o   comp_dqp.o   delta_t.o\
   dynamics.o   homogen.o   kinemat.o    robot.o\
   sensitiv.o   utils.o

libroboop.a : $(Deprobooplib)
	rm -f libroboop.a
	$(AR) rc libroboop.a $(Deprobooplib)

gnugraph.o :  source/gnugraph.cpp
	$(CC) $(CFLAGS) -o $@ $?

comp_dq.o :  source/comp_dq.cpp
	$(CC) $(CFLAGS) -o $@ $?

comp_dqp.o :  source/comp_dqp.cpp
	$(CC) $(CFLAGS) -o $@ $?

delta_t.o :  source/delta_t.cpp
	$(CC) $(CFLAGS) -o $@ $?

dynamics.o :  source/dynamics.cpp
	$(CC) $(CFLAGS) -o $@ $?

homogen.o :  source/homogen.cpp
	$(CC) $(CFLAGS) -o $@ $?

kinemat.o :  source/kinemat.cpp
	$(CC) $(CFLAGS) -o $@ $?

robot.o :  source/robot.cpp
	$(CC) $(CFLAGS) -o $@ $?

sensitiv.o :  source/sensitiv.cpp
	$(CC) $(CFLAGS) -o $@ $?

utils.o :  source/utils.cpp
	$(CC) $(CFLAGS) -o $@ $?

Depnewmatlib = \
   bandmat.o    cholesky.o   evalue.o     fft.o\
   hholder.o    jacobi.o     myexcept.o   newmat1.o\
   newmat2.o    newmat3.o    newmat4.o    newmat5.o\
   newmat6.o    newmat7.o    newmat8.o    newmat9.o\
   newmatex.o   newmatnl.o   newmatrm.o   solution.o\
   sort.o       submat.o     svd.o

libnewmat.a: $(Depnewmatlib)
	rm -f libnewmat.a
	$(AR) rc libnewmat.a $(Depnewmatlib)

bandmat.o :  newmat/bandmat.cpp
	$(CC) $(CFLAGS) -o $@ $?

cholesky.o :  newmat/cholesky.cpp
	$(CC) $(CFLAGS) -o $@ $?

evalue.o :  newmat/evalue.cpp
	$(CC) $(CFLAGS) -o $@ $?

fft.o :  newmat/fft.cpp
	$(CC) $(CFLAGS) -o $@ $?

hholder.o :  newmat/hholder.cpp
	$(CC) $(CFLAGS) -o $@ $?

jacobi.o :  newmat/jacobi.cpp
	$(CC) $(CFLAGS) -o $@ $?

myexcept.o :  newmat/myexcept.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat1.o :  newmat/newmat1.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat2.o :  newmat/newmat2.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat3.o :  newmat/newmat3.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat4.o :  newmat/newmat4.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat5.o :  newmat/newmat5.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat6.o :  newmat/newmat6.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat7.o :  newmat/newmat7.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat8.o :  newmat/newmat8.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmat9.o :  newmat/newmat9.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmatex.o :  newmat/newmatex.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmatnl.o :  newmat/newmatnl.cpp
	$(CC) $(CFLAGS) -o $@ $?

newmatrm.o :  newmat/newmatrm.cpp
	$(CC) $(CFLAGS) -o $@ $?

solution.o :  newmat/solution.cpp
	$(CC) $(CFLAGS) -o $@ $?

sort.o :  newmat/sort.cpp
	$(CC) $(CFLAGS) -o $@ $?

submat.o :  newmat/submat.cpp
	$(CC) $(CFLAGS) -o $@ $?

svd.o :  newmat/svd.cpp
	$(CC) $(CFLAGS) -o $@ $?

Depbench =  libroboop.a libnewmat.a bench.o

bench : $(Depbench)
	$(CC) bench.o -o bench $(LIBS)

bench.o :  source/bench.cpp
	$(CC) $(CFLAGS) -o $@ $?

clean:
	rm *.o *.a

veryclean: clean
	rm demo bench


