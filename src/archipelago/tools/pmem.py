#!/usr/bin/python

from subprocess import Popen, STDOUT, PIPE
from sys import argv, exit, stdout
from os import environ
from copy import copy 
from time import sleep

ps_args = [ "ps", "h", "-o", "pid,ppid,vsz,rsz,sz", "-C" ]
VM_Size=0
RS_Size=1
CR_Size=2
Parent_ID=3
StdOut=0

def get_execname(full_name):
    a = full_name.split("/")
    a.reverse()
    return a[0]

def extend_environment(old_env, new_strings) :

    new_env = copy(old_env) #copy the environment

    for s in new_strings: #extend it
        (key, value) = s.split("=")
        new_env[key] = value

    return new_env #done

def parse_args() :
    
    if len(argv) < 2 :
        print "usage: pmem.py [-o outfile] [-e env string] [-f format] [-p pid file] <program> [args...]"
        exit()
                
    index = 1;

    outfile = stdout
    myenv = environ
    format = "virtual %uk, resident %uk" #, core %uk, ppid %u"
    pidfile = None
    pname = None
    args = None
    
    while index < len(argv) :

        if argv[index] == "-o" : #check for output file
            outfile = open(argv[index + 1], "w+")
            index += 2
            continue
    
        if argv[index] == "-e" : #check the preloaded libs
            myenv = extend_environment(myenv, [ argv[index + 1] ]);
            index += 2
            continue
    
        if argv[index] == "-f" : #check for new format
            format = argv[index + 1]
            index += 2
            continue

        if argv[index] == "-p" : #check if pid of the child needs to be saved
            pidfile = open(argv[index + 1], "w+")
            index += 2
            continue
        
        pname = argv[index]  #whatever remains, is the program name 
        args  = argv[index:] #and its arguments

        break #done

    if pname == None:
        print "usage: pmem.py [-o outfile] [-e env string] [-f format] [-p pid file] <program> [args...]"
        exit()

    return (pname, args, myenv, pidfile, outfile, format)

def start_child(pname, args, myenv):
    proc = Popen(args,
                 executable = pname,
#                 stdout     = open("%s.out" % get_execname(pname), "w+"),
#                 stderr     = STDOUT,
                 env        = myenv)

    ps_args.append(get_execname(pname))
    return proc

def update_memusage(mem_usage):
    
    out = Popen(ps_args, stdout = PIPE).communicate()[StdOut].split("\n") 
    
    for p in out: #parse every line of output

        if p == "":
            continue
        
        (pid, ppid, vmsz, rssz, crsz) = map(int, p.split())

        if pid in mem_usage.keys(): #known process
            mem_usage[pid] = (max(vmsz, mem_usage[pid][VM_Size]),
                              max(rssz, mem_usage[pid][RS_Size]),
                              max(crsz, mem_usage[pid][CR_Size]),
                              mem_usage[pid][Parent_ID])
        elif ppid in mem_usage.keys(): #child of a known process
            mem_usage[pid] = (vmsz, rssz, crsz, ppid)

    return mem_usage
    
def report_memusage(mem_usage, outfile, format):
    vsz = 0
    rsz = 0
    
    for k in mem_usage.keys():
        vsz += mem_usage[k][VM_Size]
        rsz += mem_usage[k][RS_Size]
    #    outfile.write("pid %u, ppid %u " % (k, mem_usage[k][Parent_ID]))
    #    outfile.write(format % (mem_usage[k][VM_Size], mem_usage[k][RS_Size]))
    #    outfile.write("\n")

    outfile.write(format % (vsz, rsz))
    outfile.write("\n")

    if outfile != stdout:
        outfile.close()
    
def main():

    (pname, args, myenv, pidfile, outfile, format) = parse_args()
    
    proc = start_child(pname, args, myenv)

    if pidfile != None:
        pidfile.write(str(proc.pid))
        pidfile.close()

    mem_usage = { proc.pid : (0, 0, 0, -1) }

    while proc.poll() == None:
        mem_usage = update_memusage(mem_usage)
        #sleep(1)

    report_memusage(mem_usage, outfile, format)

main()    


