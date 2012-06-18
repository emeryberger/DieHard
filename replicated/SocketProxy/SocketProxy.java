import java.io.*;
import java.net.*;
import java.util.*;

/*
 
 The SocketProxy server.
 
 */


public class SocketProxy {
    
    // The port number of the socket proxy service.
    private final static int servicePortNumber = 4444;
    
    // The next port number to be used as a proxy.
    private static int nextPortNumber = servicePortNumber + 1;
    
    private static int numReplicas;
    
    private static HashMap<Client, Integer> visitCount
    = new HashMap<Client, Integer> ();
    
    private static void handleConnection (Socket sock)
    {
        // Read the IP address and port number off that
        // socket. Then open a proxy connection to that IP
        // address and port from a local socket on a new
        // port. We finally send back the proxy's port number
        // to the client.
        
        try {
            
            DataInputStream input =
                new DataInputStream (sock.getInputStream());
            
            BufferedReader r
            = new BufferedReader (new InputStreamReader (input));
            
            // Read the name of the target server.
            
            String serverName = r.readLine();
            System.out.println (serverName);
            
            // Read the name of the target port number.
            
            String portNumberStr = r.readLine();
            System.out.println (portNumberStr);
            int portNumber = Integer.parseInt (portNumberStr);
            
            // Finally, read the process id of the client.

            String pidStr = r.readLine();
            System.out.println ("Sending to " + pidStr + ".");
            int pid = Integer.parseInt(pidStr);

            // Now we add this client's connection to the map.
            
            Client cl = new Client (serverName, portNumber, pid);
            Integer value = visitCount.get (cl);
            if (value != null) {
                // Now this will be the (nth+1) connection from this process.
                value = new Integer (value + 1);
            } else {
                // This is the first attempt to connect to this server/port
                // from this process.
                value = new Integer (1);
            }
            visitCount.put (cl, value);
                        
            System.out.println ("client count = " + visitCount.get(cl));
            
            // Set up a proxy connection.
            
            ProxyOnUp proxy = 
                new ProxyOnUp (serverName, 
                        portNumber,
                        nextPortNumber,
                        pid);
            proxy.start();
            
            // Tell the client the port number that connects to
            // the proxy.
            
            PrintStream output = 
                new PrintStream (sock.getOutputStream());
            
            output.println (nextPortNumber);
            
            // We're done; close the service socket and wait for
            // the next connection.
            
            sock.close();
            
        } catch (IOException e) {
            e.printStackTrace();
        }
        nextPortNumber++;
    }
    
    public static void main (String args[])
    {
        System.out.println ("Yeah baby.");
        if (args.length < 1) {
            System.out.println ("How many replicas, dude?");
            System.exit (-1);
        }
        numReplicas = Integer.parseInt (args[0]);
        System.out.println ("reps = " + numReplicas);
        
        ServerSocket serverSocket = null;
        try {
            serverSocket = new ServerSocket (servicePortNumber);
        } catch (IOException e) {
            System.err.println (e);
            System.exit(-1);
        }
        while (true) {
            try {
                // Wait for a connection to our server.
                Socket serviceSocket = serverSocket.accept();
                handleConnection (serviceSocket);
            } catch (IOException e) {
                //
            }
        }
    }
    
}
