class Client {
    
    Client (String serverName,
            int portNumber,
            int pid) {
        this.serverName = serverName;
        this.portNumber = portNumber;
        this.pid = pid;
    }
    
    public int hashCode () {
        int v = (this.portNumber + this.pid);
        return v;
    }

    public boolean equals (Object o) {
        // System.out.println ("Comparing " + this + " to " + o);
        if (o == this) {
            return true;
        }
        if (!(o instanceof Client)) {
            return false;
        }
        Client other = (Client) o;
        return (other.serverName.equals(this.serverName))
        && (other.portNumber == this.portNumber)
        && (other.pid == this.pid);
    }

    private String serverName;
    private int portNumber;
    private int pid;
    
}

