#CC=g++
#CCFLAGS=-g -pipe
JAVAC=javac

SpecFileCreator.class: SpecFileCreator.java
	${JAVAC} SpecFileCreator.java

working_set: working_set.sh SpecFileCreator.class
	./working_set.sh .jail/http
	ln -s http .jail/ftp

#http_client: http_client.cpp
#	${CC} ${CCFLAGS} -o $@ http_client.cpp -lrt

#working_set: working_set.sh SpecFileCreator.class
#	./working_set.sh jail/http


#all: http_client working_set

clean: #clean_working_set
	rm -fr SpecFileCreator.class #jail/http/dir*

