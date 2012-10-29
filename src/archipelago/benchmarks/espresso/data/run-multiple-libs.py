#!/usr/bin/python

import sys, os.path
from getopt   import gnu_getopt
from datetime import datetime
from commands import getstatusoutput
from os.path  import isfile, isdir, exists, join
from os       import mkdir, listdir

LIB_PATH=None
INPUT=None
OUT_PATH=None
EXEC="espresso-custom "
EXEC_PATH=" ../" + EXEC
TIME_STAMP=None
TIME="/usr/bin/time -o "
RUNS=0

def print_usage():
    print "Usage: run-multiple-libs -l <library path> -i <input> [ -t ] [-n <runs> ][ -o <output path> ]"
#end

def parse_args():
    global LIB_PATH, OUT_PATH, INPUT, TIME_STAMP
    
    arglist, tmp = gnu_getopt(sys.argv[1:], "l:i:to:n:")

    #parse getopt args
    for opt in arglist:
        if opt[0] == "-t":
            TIME_STAMP = "-" + datetime.now().strftime("%Y_%m_%d_%H:%M:%S")
        elif opt[0] == "-l":
            if isdir(opt[1]): LIB_PATH = opt[1]
            else: print opt[1] + " does not exist!"
        elif opt[0] == "-i":
            if isfile(opt[1]): INPUT = " " + opt[1]
            else: print opt[1] + " does not exist!"
        elif opt[0] == "-o":
            OUT_PATH = opt[1]
            #TODO: check if already exists
        elif opt[0] == "-n":
            RUNS = eval(opt[1])
            
    #check for mandatory args
    if (LIB_PATH == None) or (INPUT == None):
        print_usage()
        sys.exit(-1)
        
    #create outdir if needed
    if OUT_PATH == None:
        OUT_PATH = os.path.split(INPUT)[1]
        
    if TIME_STAMP != None:
        OUT_PATH = OUT_PATH + TIME_STAMP
    else:
        TIME_STAMP = ""
        
    if not exists(OUT_PATH):
        mkdir(OUT_PATH)      
#end

def do_single_run(lib, outfile):
    getstatusoutput(TIME + outfile + EXEC_PATH + lib + INPUT);
    
def islib(path):
    return path.endswith(".so")
#end

def get_postfix(path):
    return path[3:-3]
#end

def create_out_file(lib):
    return join(OUT_PATH, "espresso_" + get_postfix(lib) + TIME_STAMP + ".time")
#end

def create_out_file_mr(lib, run):
    return "%s/espresso_%s%s_run_%d.time" % (OUT_PATH, get_postfix(lib), TIME_STAMP, run)
#end

#main
parse_args()

for lib in listdir(LIB_PATH):
    if islib(lib):
        if (RUNS == 0):
            do_single_run(join(LIB_PATH, lib), create_out_file(lib))
        else:
            for i in range(RUNS):
                do_single_run(join(LIB_PATH, lib), create_out_file_mr(lib, run))
        
