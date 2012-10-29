#!/usr/bin/python

import sys, os.path
from getopt   import gnu_getopt
from datetime import datetime
from commands import getstatusoutput
from os.path  import isfile, isdir, exists, join
from os       import mkdir, listdir

LIB_PATH=""
INPUT=None
OUT_PATH=None
RUNS = 0
EXEC="espresso-custom "
EXEC_PATH=" ../" + EXEC
TIME_STAMP=None
TIME="/usr/bin/time -o "

def print_usage():
    print "Usage: run-multiple -l <library path> -i <input> -n <number of runs> [ -t ] [ -o <output path> ]"
#end

def parse_args():
    global LIB_PATH, OUT_PATH, INPUT, TIME_STAMP, RUNS
    
    arglist, tmp = gnu_getopt(sys.argv[1:], "n:l:i:to:")

    #parse getopt args
    for opt in arglist:
        if opt[0] == "-t":
            TIME_STAMP = "-" + datetime.now().strftime("%Y_%m_%d_%H:%M:%S")
        elif opt[0] == "-l":
            if isfile(opt[1]): LIB_PATH = " "+ opt[1]
            else: print opt[1] + " does not exist!"
        elif opt[0] == "-i":
            if isfile(opt[1]): INPUT = opt[1]
            else: print opt[1] + " does not exist!"
        elif opt[0] == "-o":
            OUT_PATH = opt[1]
            #TODO: check if already exists
        elif opt[0] == "-n":
            RUNS = eval(opt[1])
            
    #check for mandatory args
    if (INPUT == None) or (RUNS == 0):
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
    getstatusoutput(TIME + outfile + EXEC_PATH + INPUT + lib)
    
def create_out_file(run):
    return "%s/espresso%s_%d.time" % (OUT_PATH, TIME_STAMP, run)
#end

#main
parse_args()

for run in range(RUNS):
    do_single_run(LIB_PATH, create_out_file(run))
        
