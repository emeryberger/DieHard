import java.io.*;
import java.net.*;

class ProxyOnUp extends Thread {
    
    ProxyOnUp (String serverName,
	       int serverPort,
	       int clientPort,
	       int pid) {
        super ("ProxyOnUp");
        this.serverName = serverName;
        this.serverPort = serverPort;
        this.clientPort = clientPort;
        this.toServer = null;
        this.localProxy = null;
	//        this.numReplicas = numReplicas;
        
        try {
            this.localProxy = new ServerSocket (clientPort);
        } catch (UnknownHostException e) {
            System.err.println (e);
            return;
        } catch (IOException e) {
            System.err.println (e);
            return;
        }
    }
    
    public void run() {
        
        // Wait for connections on the given port, and then spawn the
        // reader-writer threads that copy input from the client to
        // the server, and vice versa.
        
        while (true) {
            try {
                
                this.toServer = new Socket (this.serverName, this.serverPort);
                System.out.println ("Awaiting a connection on port " + this.clientPort);
                
                Socket serviceSocket = this.localProxy.accept();
                
                System.out.println ("Got one.");
    
//              BroadcastReads b1 = new BroadcastReads(serviceSocket, xxx);
                
                ReadWrite r1 = new ReadWrite (serviceSocket, this.toServer);
                ReadWrite r2 = new ReadWrite (this.toServer, serviceSocket);
                
                r1.start();
                r2.start();
                
                try {
                    r1.join();
                    r2.join();
                } catch (InterruptedException e) {
                    return;
                }
                
            } catch (IOException e) {
                System.err.println (e);
                return;
            }
        }
    }
    
    protected void finalize () {
        try {
            this.localProxy.close();
            this.toServer.close();
        } catch (IOException e) {
            return;
        }
    }
    
    private String serverName;
    private int serverPort;
    private int clientPort;
    private int numReplicas;
    
    private Socket toServer;
    private ServerSocket localProxy;
    
}
