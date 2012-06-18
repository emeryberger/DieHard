import java.io.*;
import java.net.*;

/*
 
 The SocketProxy server.
 
 */

class BroadcastReads extends Thread {
    
    BroadcastReads (Socket fromServer, Socket[] toClients) {
        this.fromServer = fromServer;
        this.toClients = toClients;
    }
    
    public void run() {
        try {
            InputStream in = fromServer.getInputStream();
            OutputStream out[] = new OutputStream[toClients.length];
            for (int i = 0; i < toClients.length; i++) {
                out[i] = toClients[i].getOutputStream();
            }
            byte[] buffer = new byte[bufferSize];
            int bytesRead = 0;
            while (true) {
                synchronized (in) {
                    bytesRead = in.read (buffer);
                    if (bytesRead == -1) {
                        break;
                    }
                }
                for (int i = 0; i < toClients.length; i++) {
                    synchronized (out[i]) {
                        // String msg = new String (buffer);
                        // System.out.println (msg);
                        out[i].write (buffer, 0, bytesRead);
                    }
                }                
            }
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }
    }
    
    // The maximum to read (& then write) from a socket.
    private static final int bufferSize = 8192;
    
    private Socket fromServer;
    private Socket[] toClients;
}


class ReadWrite extends Thread {
    
    ReadWrite (Socket one, Socket two) {
        super ("ReadWrite");
        System.out.println ("ReadWrite " + one + ", " + two);
        this.one = one;
        this.two = two;
    }
    
    public void run() {
        
        int buffersRead = 0;
        
        // Repeatedly read from the input stream and write to the
        // output stream until the input stream is emptied.
        
        try {
            InputStream in
            = one.getInputStream();
            OutputStream out
            = two.getOutputStream();
            
            byte[] buffer = new byte[bufferSize];
            int bytesRead = 0;
            
            while (true) {
                synchronized (in) {
                    bytesRead = in.read (buffer);
                    if (bytesRead == -1) {
                        break;
                    }
                }
                synchronized (out) {
                    // String msg = new String (buffer);
                    // System.out.println (msg);
                    out.write (buffer, 0, bytesRead);
                }
                buffersRead++;
            }
        } catch (IOException e) {
            e.printStackTrace();
            return;
        }
    }
    
    // The maximum to read (& then write) from a socket.
    private static final int bufferSize = 8192;
    
    private Socket one;
    private Socket two;
    
}


class ProxyOnUp extends Thread {
    
    ProxyOnUp (String serverName, int serverPort, int clientPort, int numReplicas) {
        super ("ProxyOnUp");
        this.serverName = serverName;
        this.serverPort = serverPort;
        this.clientPort = clientPort;
        this.toServer = null;
        this.localProxy = null;
        this.numReplicas = numReplicas;
        
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
                
                this.toServer = new Socket (serverName, serverPort);
                System.out.println ("Awaiting a connection on port " + clientPort);
                
                Socket serviceSocket = localProxy.accept();
                
                System.out.println ("Got one.");
    
//              BroadcastReads b1 = new BroadcastReads(serviceSocket, xxx);
                
                ReadWrite r1 = new ReadWrite (serviceSocket, toServer);
                ReadWrite r2 = new ReadWrite (toServer, serviceSocket);
                
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
            localProxy.close();
            toServer.close();
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


public class SocketProxy {
    
    // The port number of the socket proxy service.
    private final static int servicePortNumber = 4444;
    
    // The next port number to be used as a proxy.
    private static int nextPortNumber = servicePortNumber + 1;
    
    public static void main (String args[])
    {
        ServerSocket serverSocket = null;
        try {
            serverSocket = new ServerSocket (servicePortNumber);
        } catch (IOException e) {
            System.err.println (e);
            System.exit(-1);
        }
        while (true) {
            int numReplicas = 0;
            try {
                
                // Wait for a connection to our server.  Once we get
                // one, read the IP address and port number off that
                // socket. Then open a proxy connection to that IP
                // address and port from a local socket on a new
                // port. We finally send back the proxy's port number
                // to the client.
                
                Socket serviceSocket = serverSocket.accept();
                
                DataInputStream input =
                    new DataInputStream (serviceSocket.getInputStream());
                
                BufferedReader r
                = new BufferedReader (new InputStreamReader (input));
                
                // Read the name of the target server.
                
                String serverName = r.readLine();
                System.out.println (serverName);
                
                // Read the name of the target port number.
                
                String portNumber = r.readLine();
                System.out.println (portNumber);
                
                // Find out how many broadcast replicas to create.
                String replicas = r.readLine();
                System.out.println ("Sending to " + replicas + " replicas.");
                
                numReplicas = Integer.parseInt(replicas);
                
                // Set up a proxy connection.
                
                ProxyOnUp proxy = 
                    new ProxyOnUp (serverName, 
                            Integer.parseInt(portNumber),
                            nextPortNumber,
                            numReplicas);
                
                proxy.start();
                
                // Tell the client the port number that connects to
                // the proxy.
                
                PrintStream output = 
                    new PrintStream (serviceSocket.getOutputStream());
                
                output.println (nextPortNumber);
                
                // We're done; close the service socket and wait for
                // the next connection.
                
                serviceSocket.close();
                
            } catch (IOException e) {
                e.printStackTrace();
            }
            nextPortNumber += numReplicas;
        }
    }
}
