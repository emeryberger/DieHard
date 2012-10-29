#!/usr/bin/python

#imports
from httplib   import HTTPConnection, HTTPResponse, OK
from threading import Thread
from time      import clock
from random    import randint
from sys       import argv, exit

#globals
clients = []

#client class
class Client (Thread):
    def __init__(self, tid, host, port, r):
        Thread.__init__(self)
        self.tid = tid
        self.requests = r
        self.time = 0.0
        self.size = 0
        self.connection = HTTPConnection(host, port)
        
    def request(self, url): #just read everything from the client; will be timed!
        self.connection.request("GET", url)
        response = self.connection.getresponse()
        data = response.read()
        return (response, data)
                    

    def compute_size(self, resp, data):
        header_size = 0

        for h in resp.getheaders():
            header_size = header_size + len(h[0]) + len(h[1]) + 4 # for ": " and \r\n

        header_size = header_size + 2 #for \r\n that separates header from the body

        return header_size + len(data)

    def make_url(self):
        return "/dir%05d/class%d_%d" % (randint(0, 699), randint(0, 3), randint(0, 8))
        

    def run(self):
        self.connection.connect()
        
        for i in range(self.requests): #for evert request

            url         = self.make_url() #get next URL
            
            start_time  = clock();   
            (res, data) = self.request(url)
            end_time    = clock();

            if res.status == OK:
                self.time = self.time + (end_time - start_time)
                self.size = self.size + self.compute_size(res, data)
                
        self.connection.close()

    def throughput(self):
        return float(self.size)/(self.time * 1024)

    def latency(self):
        return float(self.time)/self.requests

    def total_time(self):
        return self.time

    def total_size(self):
        return float(self.size) / 1024

    def report(self):
              print "| %6d | %17.1f | %10.3f | %22.1f |" % (self.tid + 1, self.total_size(), self.total_time(), self.throughput())
#        print "| %6d | %17.1f | %10.3f | %22.1f | %13.3f |" % (self.tid + 1, self.total_size(), self.total_time(), self.throughput(), self.latency())


def print_header():
    print "--------------------------------------------------------------------"
    print "| Client | Received (Kbytes) | Time (sec) | Throughput (Kbyte/sec) |"
    print "--------------------------------------------------------------------"

#    print "------------------------------------------------------------------------------------"
#    print "| Client | Received (Kbytes) | Time (sec) | Throughput (Kbyte/sec) | Latency (sec) |"
#    print "------------------------------------------------------------------------------------"

def print_total(size, time, throughput, latency):
    print "--------------------------------------------------------------------"
    print "| Total: | %17.1f | %10.3f | %22.1f |" % (size, time, throughput)#, latency)
    print "--------------------------------------------------------------------"
    
#main

if len(argv) <> 5:
    print "Usage: http_client <clients> <requests> <host> <port>"
    exit(-1)

# parse command line args
num_clients  = int(argv[1])
num_requests = int(argv[2])
g_host       =     argv[3]
g_port       = int(argv[4])

for i in range(num_clients):
    clients.append(Client(i, g_host, g_port, num_requests))

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
  #  total_time = total_time + clients[i].total_time()
    total_size = total_size + clients[i].total_size()
    clients[i].report()


print_total(total_size, total_time, float(total_size)/total_time, float(total_time)/total_requests)
