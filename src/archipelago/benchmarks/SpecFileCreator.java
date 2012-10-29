import java.util.Random;
import java.io.FileOutputStream;
import java.io.BufferedOutputStream;
import java.io.OutputStream;
import java.io.IOException;

/**
 * SpecFileCreator:<br>
 * Create a set of random files matching the SPECWeb99 benchmark
 * SpecWeb defines a set of size classes: 1-1k, 1k-10k, 10k-100k,100k-1MB
 * Every directory in the SpecWeb benchmark contains ten files for each
 * class, where each file is of a random size drawn from the class range
 * <p>
 * This class implements the creation of one such directory.
 * <p>
 * Usage: java SpecFileCreator <dir-name><br>
 * For example:<br>
 * <code>for x in 0 1 2 3 4 5 6 7 8 9; do<br>
 * for y in 0 1 2 3 4 5 6 7 8 9; do<br>
 * java SpecFileCreator 0${x}${y}
 * done;
 * done</code><br>
 * Will create 100 directories of random files.  Note that each directory
 * is on the order of 10MB, so 100*10MB = 1GB.
 * <b>You have been warned...</b>
 * @author Brendan Burns
 **/
class SpecFileCreator {
    // Minimum size for each class
    public static int[] CLASS_MIN=new int[] {1,1024,10*1024,100*1024};
    // Maximum size for each class
    public static int[] CLASS_MAX=new int[]
	{1024,10*1024,100*1024,1024*1024};
    
    /**
     * Create a random file of size <code>size</code>
     * @param r Random generator
     * @param out Where to write
     * @param size number of bytes
     **/
    public static void createFile(Random r, OutputStream out, int size)
        throws IOException
    {
        for (int i=0;i<size;i++) {
            out.write(r.nextInt(256));
        }
        out.flush();
    }
    
    /**
     * Create a file of random bytes
     * @param r The random generator
     * @param file The file name
     * @param size The number of bits
     **/
    public static void createFile(Random r, String file, int size)
        throws IOException
    {
        createFile(r, new BufferedOutputStream(new
					       FileOutputStream(file)),
                   size);
    }

    /**
     * Create a random file of bytes
     * @param r The random generator
     * @param dir The directory
     * @param clss The class number
     * @param number The number for the file
     **/
    public static void createFile(Random r, String dir, int clss, int
				  number)
        throws IOException
    {
        int size =
	    CLASS_MIN[clss]+r.nextInt(CLASS_MAX[clss]-CLASS_MIN[clss]);
        String name = dir+"/class"+clss+"_"+number;
        createFile(r, name, size);
    }

    /**
     * Create a set of random files for a size class
     * @param r The random generator
     * @param dir The directory
     * @param clss The class number
     **/
    public static void createClassFiles(Random r, String dir, int clss)
        throws IOException
    {
        for (int i=0;i<9;i++) {
            createFile(r, dir, clss, i);
        }
    }

    /**
     * Create a complete directory of random files
     * @param r The random generator
     * @param dir The directory name
     **/
    public static void createFiles(Random r, String dir)
        throws IOException
    {
        for (int i=0;i<CLASS_MIN.length;i++) {
            createClassFiles(r,dir, i);
        }
    }

    /**
     * Main routine
     * usage: java SpecFileGenerator <directory-name>
     **/
    public static void main(String[] args)
        throws IOException
    {
	if(args.length < 1)
	    System.out.println("Usage: java SpecFileCreator <directory-name>");
	else {
	    Random r = new Random();
	    createFiles(r, args[0]);
	}
    }
}
