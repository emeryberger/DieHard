import java.io.*;
import java.net.*;

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
            = this.one.getInputStream();
            OutputStream out
            = this.two.getOutputStream();
            
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
