import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.concurrent.Semaphore;
import org.unimrcp.swig.*;

class UniRecog
{
    // UniMRCP client root directory
    private static String ROOT_DIR = "../../../../trunk";
    // UniMRCP profile to use for communication with server
    private static final String MRCP_PROFILE = "uni1";

    // Types of streaming
    private static enum StreamType {
        FRAME_BY_FRAME,
        BUFFERED,
        MEMORY,
        FILE
    }

    // Choose type of streaming to demonstrate
    private static final StreamType streamType = StreamType.FILE;

    private static int err = 0;

    // Load native library
    static {
        System.loadLibrary("UniMRCP");
        // Just detect various directory layout constellations
        if (!(new File(ROOT_DIR)).exists())
            ROOT_DIR = "../../../../UniMRCP";
        if (!(new File(ROOT_DIR)).exists())
            ROOT_DIR = "../../../../unimrcp";
    }

    // User-defined logger
    private static class UniRecogLogger extends UniMRCPLogger
    {
        public boolean Log(String file, long line, UniMRCPLogPriority priority, String message)
        {
            System.out.println("  " + message);
            return true;
        }
    }


    private static class UniRecogSession extends UniMRCPClientSession
    {
        public UniRecogSession(UniMRCPClient client, String profile)
        {
            super(client, profile);
        }

        public boolean OnTerminate(UniMRCPSigStatusCode status)
        {
            System.out.println("Session terminated with code " + status);
            return true;
        }

        public boolean OnTerminateEvent()
        {
            System.out.println("Session terminated unexpectedly");
            return true;
        }
    }

    private static class UniRecogStreamFrames extends UniMRCPStreamRx
    {
        // Set to true upon RECOGNIZE's IN-PROGRESS
        public boolean started = false;
        // Input file descriptor
        private FileInputStream f = null;
        // Buffer to copy audio data to
        private byte[] buf = null;

        public UniRecogStreamFrames(String pcmfile) throws FileNotFoundException
        {
            f = new FileInputStream(pcmfile);
        }

        // Called when an audio frame is needed
        public boolean ReadFrame()
        {
            if (!started)
                return false;
            int frameSize = (int) GetDataSize();
            if (frameSize <= 0)
                return true;
            if (buf == null)
                buf = new byte[frameSize];
            try {
                int bytesRead = f.read(buf);
                if (bytesRead > 0) {
                    Arrays.fill(buf, bytesRead, frameSize, (byte)0);
                    SetData(buf);
                    return true;
                }
            } catch (IOException ex) {
            }
            return false;
        }
    }


    private static class UniRecogTermination extends UniMRCPAudioTermination
    {
        public UniMRCPStreamRx stream;

        public UniRecogTermination(UniMRCPClientSession sess, String pcmfile) throws IOException
        {
            super(sess);
            switch (streamType) {
            case FRAME_BY_FRAME:
                stream = new UniRecogStreamFrames(pcmfile);
                break;
            case BUFFERED:
                stream = new UniMRCPStreamRxBuffered();
                break;
            case MEMORY:
                byte[] data = Files.readAllBytes(Paths.get(pcmfile));
                stream = new UniMRCPStreamRxMemory(data, true, UniMRCPStreamRxMemory.StreamRxMemoryEnd.SRM_NOTHING, true);
                break;
            case FILE:
                stream = new UniMRCPStreamRxFile(pcmfile, 0, UniMRCPStreamRxMemory.StreamRxMemoryEnd.SRM_NOTHING, true);
                break;
            }
        }

        public UniMRCPStreamRx OnStreamOpenRx(boolean enabled, short payload_type, String name, String format, short channels, long freq)
        {
            // Configure incoming stream here
            return this.stream;
        }
    }


    private static class UniRecogChannel extends UniMRCPRecognizerChannel
    {
        private UniRecogTermination term;
        private Semaphore sem;
        private String grammarfile;
        private String inputfile;

        public UniRecogChannel(UniRecogSession sess, UniRecogTermination term, Semaphore sem, String grammarfile, String inputfile)
        {
            super(sess, term);
            this.term = term;
            this.sem = sem;
            this.grammarfile = grammarfile;
            this.inputfile = inputfile;
        }

        // Shorthand for graceful fail: Write message, release semaphore and return false
        private boolean Fail(String msg)
        {
            System.out.println(msg);
            err = 1;
            sem.release();
            return false;
        }

        // MRCP connection established, start communication
        public boolean OnAdd(UniMRCPSigStatusCode status)
        {
            if (status != UniMRCPSigStatusCode.SUCCESS)
                return Fail("Failed to add channel: " + status);

            try {
                // Load grammar content from file
                byte[] content = Files.readAllBytes(Paths.get(this.grammarfile));
                // Start processing here
                UniMRCPRecognizerMessage msg = CreateMessage(UniMRCPRecognizerMethod.RECOGNIZER_RECOGNIZE);
                msg.setContent_type("application/grammar+xml");
                msg.SetBody(content);
                return msg.Send();
            }
            catch (IOException ex) {
                return Fail("Failed to create RECOGNIZE message");
            }
        }

        // Response or event from the server arrived
        public boolean OnMessageReceive(UniMRCPRecognizerMessage message)
        {
            // Analyze message, update your application state and reply messages here
            if (message.GetMsgType() == UniMRCPMessageType.RESPONSE) {
                if (message.GetStatusCode() != UniMRCPStatusCode.CODE_SUCCESS)
                    return Fail("RECOGNIZE request failed: " + message.GetStatusCode());
                if (message.GetRequestState() != UniMRCPRequestState.STATE_INPROGRESS)
                    return Fail("Failed to start RECOGNIZE processing");
                // Start streaming audio from the file
                StartStreaming();
                return true;  // Does not actually matter
            }
            if (message.GetMsgType() != UniMRCPMessageType.EVENT)
                return Fail("Unexpected message from the server");
            if (message.GetEventID() == UniMRCPRecognizerEvent.RECOGNIZER_START_OF_INPUT_EVENT) {
                System.out.println(String.format("Start of input detected"));
                return true;  // Does not actually matter
            }
            if (message.GetEventID() == UniMRCPRecognizerEvent.RECOGNIZER_RECOGNITION_COMPLETE) {
                System.out.println(String.format("Recognition complete: %s %s",
                    message.getCompletion_cause(), message.getCompletion_reason()));
                sem.release();
                return true;  // Does not actually matter
            }
            return Fail("Unknown message received");
        }

        private void StartStreaming()
        {
            switch (streamType) {
            case FRAME_BY_FRAME:
                ((UniRecogStreamFrames)term.stream).started = true;
                break;
            case BUFFERED:
                try {
                    byte[] data = Files.readAllBytes(Paths.get(this.inputfile));
                    ((UniMRCPStreamRxBuffered)term.stream).AddData(data);
                }
                catch (IOException ex) {
                }
                break;
            case MEMORY:
            case FILE:
                ((UniMRCPStreamRxMemory)term.stream).SetPaused(false);
                break;
            }
        }
    }


    public static void main(String argv[])
    {
        if (argv.length < 2)
        {
            System.out.println("Usage:");
            System.out.println("\tUniRecog \"/path/to/grammar/file\" \"/path/to/input/file\"");
            System.exit(1);
        }
        String grammarfile = argv[0].trim();
        String inputfile = argv[1].trim();

        int[] major = {0}, minor = {0}, patch = {0};
        UniMRCPClient.WrapperVersion(major, minor, patch);
        System.out.println("This is a sample Java UniMRCP client recognizer scenario.");
        System.out.println(String.format("Wrapper version: %d.%d.%d", major[0], minor[0], patch[0]));
        System.out.println(String.format("Use client configuration from %s/conf/unimrcpclient.xml", ROOT_DIR));
        System.out.println(String.format("Use profile %s", MRCP_PROFILE));
        System.out.println(String.format("Use recognition grammar file: `%s'", grammarfile));
        System.out.println(String.format("Use recognition input file: `%s'", inputfile));
        System.out.println();
        System.out.println("Press enter to start the session...");
        try {
            System.in.read();
        } catch (IOException ex) {
        }

        UniRecogLogger logger = new UniRecogLogger();
        try {
            // Initialize platform first
            UniMRCPClient.StaticInitialize(logger, UniMRCPLogPriority.DEBUG);
        } catch (Exception ex) {
            System.out.println("Unable to initialize platform: " + ex.getMessage());
            System.exit(1);
        }

        UniMRCPClient client = null;
        UniRecogSession sess = null;
        UniRecogChannel chan = null;
        try {
            // Create and start client in a root dir
            client = new UniMRCPClient(ROOT_DIR, true);
            // Create a session using MRCP profile MRCP_PROFILE
            sess = new UniRecogSession(client, MRCP_PROFILE);
            // Create audio termination with capabilities
            UniRecogTermination term = new UniRecogTermination(sess, inputfile);
            term.AddCapability("LPCM", UniMRCPSampleRate.SAMPLE_RATE_8000);
            // Semaphore to wait for completion
            Semaphore sem = new Semaphore(0);
            // Add signaling channel (and start processing in OnAdd method
            chan = new UniRecogChannel(sess, term, sem, grammarfile, inputfile);

            // Now wait until the processing finishes
            sem.acquire();
        } catch (Exception ex) {
            err = 1;
            System.out.println("An error occured: " + ex.getMessage());
        }

        // Ensure correct destruction order (do not let GC decide)
        if (chan != null) chan.delete();
        if (sess != null) sess.delete();
        if (client != null) client.delete();

        UniMRCPClient.StaticDeinitialize();
        System.out.println("Program finished, memory released. Press enter to exit.");
        try {
            System.in.read();
        } catch (IOException ex) {
        }
        System.exit(err);
    }
}
