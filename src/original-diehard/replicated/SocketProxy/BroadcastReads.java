import java.io.*;
import java.net.*;

class BroadcastReads extends Thread {
    
    BroadcastReads (Socket fromServer, Socket[] toClients) {
        this.fromServer = fromServer;
        this.toClients = toClients;
    }
    
    public void run() {
	// Read from the server, and broadcast to the client sockets.
        try {
            InputStream in = this.fromServer.getInputStream();
            OutputStream out[] = new OutputStream[this.toClients.length];
            for (int i = 0; i < this.toClients.length; i++) {
                out[i] = this.toClients[i].getOutputStream();
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
                for (int i = 0; i < this.toClients.length; i++) {
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
