import java.io.File;
import java.io.IOException;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.util.concurrent.Semaphore;

class UniSynth
{
    // UniMRCP client root directory
    private static String ROOT_DIR = "../../../../trunk";
    // UniMRCP profile to use for communication with server
    private static final String MRCP_PROFILE = "uni1";
    // File to write synthesized text to (ROOT_DIR/data/PCM_OUT_FILE)
    private static final String PCM_OUT_FILE = "UniSynth.pcm";

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
    private static class UniSynthLogger extends UniMRCPLogger
    {
        public boolean Log(String file, long line, UniMRCPLogPriority priority, String message)
        {
            System.out.println("  " + message);
            return true;
        }
    }


    private static class UniSynthSession extends UniMRCPClientSession
    {
        public UniSynthSession(UniMRCPClient client, String profile)
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


    private static class UniSynthStream extends UniMRCPStreamTx
    {
        // Set to true upon SPEAK's IN-PROGRESS
        public boolean started = false;
        // Output file descriptor
        private FileOutputStream f = null;
        // Buffer to copy audio data to
        private byte[] buf = null;

        public UniSynthStream(String pcmFile) throws FileNotFoundException
        {
            f = new FileOutputStream(pcmFile);
        }

        // Called when an audio frame arrives
        public boolean WriteFrame()
        {
            if (!started)
                return false;
            int frameSize = (int) GetDataSize();
            if (frameSize <= 0)
                return true;
            if (buf == null)
                buf = new byte[frameSize];
            GetData(buf);
            try {
                f.write(buf);
            } catch (IOException ex) {
            }
            return true;
        }
    }


    private static class UniSynthTermination extends UniMRCPAudioTermination
    {
        public UniSynthStream stream;

        public UniSynthTermination(UniMRCPClientSession sess, String outFile) throws FileNotFoundException
        {
            super(sess);
            this.stream = new UniSynthStream(outFile);
        }

        public UniMRCPStreamTx OnStreamOpenTx(boolean enabled, short payload_type, String name, String format, short channels, long freq)
        {
            // Configure outgoing stream here
            return stream;
        }
    }


    private static class UniSynthChannel extends UniMRCPSynthesizerChannel
    {
        private UniSynthTermination term;
        private Semaphore sem;
        private String text;

        public UniSynthChannel(UniSynthSession sess, UniSynthTermination term, Semaphore sem, String text)
        {
            super(sess, term);
            this.term = term;
            this.sem = sem;
            this.text = text;
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
            // Start processing here
            UniMRCPSynthesizerMessage msg = CreateMessage(UniMRCPSynthesizerMethod.SYNTHESIZER_SPEAK);
            msg.setContent_type("text/plain");
            msg.SetBody(text);
            return msg.Send();
        }

        // Response or event from the server arrived
        public boolean OnMessageReceive(UniMRCPSynthesizerMessage message)
        {
            // Analyze message, update your application state and reply messages here
            if (message.GetMsgType() == UniMRCPMessageType.RESPONSE) {
                if (message.GetStatusCode() != UniMRCPStatusCode.CODE_SUCCESS)
                    return Fail("SPEAK request failed: " + message.GetStatusCode());
                if (message.GetRequestState() != UniMRCPRequestState.STATE_INPROGRESS)
                    return Fail("Failed to start SPEAK processing");
                // Start writing audio to the file
                term.stream.started = true;
                return true;  // Does not actually matter
            }
            if (message.GetMsgType() != UniMRCPMessageType.EVENT)
                return Fail("Unexpected message from the server");
            if (message.GetEventID() == UniMRCPSynthesizerEvent.SYNTHESIZER_SPEAK_COMPLETE) {
                System.out.println(String.format("Speak complete: %s %s",
                    message.getCompletion_cause(), message.getCompletion_reason()));
                term.stream.started = false;
                sem.release();
                return true;  // Does not actually matter
            }
            return Fail("Unknown message received");
        }
    }


    public static void main(String argv[])
    {
        if (argv.length < 1)
        {
            System.out.println("Usage:");
            System.out.println("\tUniSynth \"This is a synthetic voice.\"");
            System.exit(1);
        }
        String text = "";
        for (int i = 0; i < argv.length; i++) {
            if (i > 0) text += " ";
            text += argv[i].trim();
        }
        String outfile = String.format("%s/data/%s", ROOT_DIR, PCM_OUT_FILE);

        int[] major = {0}, minor = {0}, patch = {0};
        UniMRCPClient.WrapperVersion(major, minor, patch);
        System.out.println("This is a sample Java UniMRCP client synthesizer scenario.");
        System.out.println(String.format("Wrapper version: %d.%d.%d", major[0], minor[0], patch[0]));
        System.out.println(String.format("Use client configuration from %s/conf/unimrcpclient.xml", ROOT_DIR));
        System.out.println(String.format("Use profile %s", MRCP_PROFILE));
        System.out.println(String.format("Synthesize text: `%s'", text));
        System.out.println(String.format("Write output to file: %s", outfile));
        System.out.println();
        System.out.println("Press enter to start the session...");
        try {
            System.in.read();
        } catch (IOException ex) {
        }

        UniSynthLogger logger = new UniSynthLogger();
        try {
            // Initialize platform first
            UniMRCPClient.StaticInitialize(logger, UniMRCPLogPriority.DEBUG);
        } catch (Exception ex) {
            System.out.println("Unable to initialize platform: " + ex.getMessage());
            System.exit(1);
        }

        UniMRCPClient client = null;
        UniSynthSession sess = null;
        UniSynthChannel chan = null;
        try {
            // Create and start client in a root dir
            client = new UniMRCPClient(ROOT_DIR, true);
            // Create a session using MRCP profile MRCP_PROFILE
            sess = new UniSynthSession(client, MRCP_PROFILE);
            // Create audio termination with capabilities
            UniSynthTermination term = new UniSynthTermination(sess, outfile);
            term.AddCapability("LPCM", UniMRCPSampleRate.SAMPLE_RATE_8000);
            // Semaphore to wait for completion
            Semaphore sem = new Semaphore(0);
            // Add signaling channel (and start processing in OnAdd method
            chan = new UniSynthChannel(sess, term, sem, text);

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
