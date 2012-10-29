#!/usr/bin/python

#imports
from ftplib    import FTP
from threading import Thread
from time      import clock
from random    import randint
from sys       import argv, exit

#globals
clients = []

#client class
class Client(Thread):
    def __init__(self, tid, host, port, user, password, r):
        Thread.__init__(self)
        self.tid        = tid
        self.requests   = r
        self.time       = 0.0 #total time spent
        self.size       = 0
        self.curr_size  = 0
        self.host       = host
        self.port       = port
        self.user       = user
        self.password   = password
        self.connection = FTP()
    #end

    def request(self, url):     
        self.connection.retrbinary('RETR ' + url, self.receive)
    #end
    
    def receive(self, string):
        self.curr_size = self.curr_size + len(string)
    #end
    
    def make_url(self):
        return "/tmp/ftp/dir%05d/class%d_%d" % (randint(0, 699), randint(0, 3), randint(0, 8))
    #end    

    def run(self):
        self.connection.connect(self.host, self.port)
        self.connection.login  (self.user, self.password)
        
        for i in range(self.requests):
            
            self.curr_size = 0
            url            = self.make_url() #get next URL
            
            start_time = clock();   
            self.request(url)
            end_time   = clock();
            
            self.time = self.time + (end_time - start_time)
            self.size = self.size + self.curr_size
            
        self.connection.close()
    #end
    
    def throughput(self):
        return float(self.size)/(self.time * 1024)
    #end
    
    def latency(self):
        return float(self.time)/self.requests
    #end
    
    def total_time(self):
        return self.time
    #end
    
    def total_size(self):
        return float(self.size) / 1024
    #end
    def report(self):
        print "| %6d | %17.1f | %10.3f | %22.1f |" % (self.tid, self.total_size(), self.total_time(), self.throughput())
    #end

def print_header():
    print "--------------------------------------------------------------------"
    print "| Client | Received (Kbytes) | Time (sec) | Throughput (Kbyte/sec) |"
    print "--------------------------------------------------------------------"
#end

def print_total(size, time, throughput, latency):
    print "--------------------------------------------------------------------"
    print "| Total: | %17.1f | %10.3f | %22.1f |" % (size, time, throughput)
    print "--------------------------------------------------------------------"
#end

#def main:

if len(argv) < 6:
    print "Usage: ftp_client.py <clients> <requests> <host> <port> <user> [password]"
    exit(-1)

# parse command line args
num_clients  = int(argv[1])
num_requests = int(argv[2])
g_host       =     argv[3]
g_port       = int(argv[4])
user         =     argv[5]
password     =     argv[6]

for i in range(num_clients):
    clients.append(Client(i, g_host, g_port, user, password, num_requests))

total_start = clock()

for i in range(num_clients):
    clients[i].start()

for i in range(num_clients):
    if clients[i].isAlive():
        clients[i].join()

total_end = clock()

total_time     = total_end - total_start
total_size     = 0
total_requests = num_clients * num_requests

print_header()

for i in range(num_clients):
#    total_time = total_time + clients[i].total_time()
    total_size = total_size + clients[i].total_size()
    clients[i].report()


print_total(total_size, total_time, float(total_size)/total_time, float(total_time)/total_requests)
#end
