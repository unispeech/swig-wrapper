using System;
using System.IO;
using System.Threading;

class UniRecog
{
    // UniMRCP client root directory
    static string ROOT_DIR = "../../../../trunk";
    // UniMRCP profile to use for communication with server
    const string MRCP_PROFILE = "uni1";
    // File to write Recogesized text to (ROOT_DIR/data/PCM_OUT_FILE)
    const string PCM_OUT_FILE = "UniRecog.pcm";

    // Types of streaming
    private enum StreamType
    {
        FRAME_BY_FRAME,
        BUFFERED,
        MEMORY,
        FILE
    }

    // Choose type of streaming to demonstrate
    private const StreamType streamType = StreamType.FILE;

    static int err = 0;

    // User-defined logger
    class UniRecogLogger : UniMRCPLogger
    {
        // Log messages go here
        public override bool Log(string file, uint line, UniMRCPLogPriority priority, string message)
        {
            Console.WriteLine("  {0}", message);
            return true;
        }
    }


    class UniRecogSession : UniMRCPClientSession
    {
        public UniRecogSession(UniMRCPClient client, string profile) :
            base(client, profile)
        { }

        public override bool OnTerminate(UniMRCPSigStatusCode status)
        {
            Console.WriteLine("Session terminated with code {0}", status);
            return true;
        }

        public override bool OnTerminateEvent()
        {
            Console.WriteLine("Session terminated unexpectedly");
            return true;
        }
    }


    class UniRecogStreamFrames : UniMRCPStreamRx
    {
        // Set to true upon RECOGNIZE's IN-PROGRESS
        public bool started = false;
        // Output file descriptor
        private FileStream f = null;
        // Buffer to copy audio data to
        private byte[] buf = null;

        public UniRecogStreamFrames(string pcmFile)
        {
            f = new FileStream(pcmFile, FileMode.Open, FileAccess.Read);
        }

        // Called when an audio frame is needed
        public override bool ReadFrame()
        {
            if (!started)
                return false;
            uint frameSize = GetDataSize();
            if (frameSize <= 0)
                return true;
            if (buf == null)
                buf = new byte[frameSize];
            int bytesRead = f.Read(buf, 0, (int)frameSize);
            if (bytesRead > 0)
            {
                for (int i = bytesRead; i < buf.Length; ++i)
                    buf[i] = 0;
                SetData(buf);
                return true;
            }
            return false;
        }
    }


    class UniRecogTermination : UniMRCPAudioTermination
    {
        public UniMRCPStreamRx stream;

        public UniRecogTermination(UniMRCPClientSession sess, string pcmFile) :
            base(sess)
        {
            switch (streamType)
            {
                case StreamType.FRAME_BY_FRAME:
                    stream = new UniRecogStreamFrames(pcmFile);
                    break;
                case StreamType.BUFFERED:
                    stream = new UniMRCPStreamRxBuffered();
                    break;
                case StreamType.MEMORY:
                    byte[] data = File.ReadAllBytes(pcmFile);
                    stream = new UniMRCPStreamRxMemory(data, true, UniMRCPStreamRxMemory.StreamRxMemoryEnd.SRM_NOTHING, true);
                    break;
                case StreamType.FILE:
                    stream = new UniMRCPStreamRxFile(pcmFile, 0, UniMRCPStreamRxMemory.StreamRxMemoryEnd.SRM_NOTHING, true);
                    break;
            }
        }

        public override UniMRCPStreamRx OnStreamOpenRx(bool enabled, byte payload_type, string name, string format, byte channels, uint freq)
        {
            // Configure incoing stream here
            return stream;
        }
    }


    class UniRecogChannel : UniMRCPRecognizerChannel
    {
        private UniRecogTermination term;
        private Semaphore sem;
        private string grammarFile;
        private string inputFile;

        public UniRecogChannel(UniRecogSession sess, UniRecogTermination term, Semaphore sem, string grammarFile, string inputFile) :
            base(sess, term)
        {
            this.term = term;
            this.sem = sem;
            this.grammarFile = grammarFile;
            this.inputFile = inputFile;
        }

        // Shorthand for graceful fail: Write message, release semaphore and return false
        private bool Fail(string msg)
        {
            Console.WriteLine(msg);
            err = 1;
            sem.Release();
            return false;
        }

        // MRCP connection established, start communication
        public override bool OnAdd(UniMRCPSigStatusCode status)
        {
            if (status != UniMRCPSigStatusCode.SUCCESS)
                return Fail(String.Format("Failed to add channel: {0}", status));
            // Load grammar content from file
            byte[] content = File.ReadAllBytes(grammarFile);
            // Start processing here
            UniMRCPRecognizerMessage msg = CreateMessage(UniMRCPRecognizerMethod.RECOGNIZER_RECOGNIZE);
            msg.content_type = "application/grammar+xml";
            msg.SetBody(content);
            return msg.Send();
        }

        // Response or event from the server arrived
        public override bool OnMessageReceive(UniMRCPRecognizerMessage message)
        {
            // Analyze message, update your application state and reply messages here
            if (message.GetMsgType() == UniMRCPMessageType.RESPONSE)
            {
                if (message.GetStatusCode() != UniMRCPStatusCode.CODE_SUCCESS)
                    return Fail(String.Format("RECOGNIZE request failed: {0}", message.GetStatusCode()));
                if (message.GetRequestState() != UniMRCPRequestState.STATE_INPROGRESS)
                    return Fail(String.Format("Failed to start RECOGNIZE processing"));
                // Start streaming audio from the file
                StartStreaming();
                return true;  // Does not actually matter
            }
            if (message.GetMsgType() != UniMRCPMessageType.EVENT)
                return Fail("Unexpected message from the server");
            if (message.GetEventID() == UniMRCPRecognizerEvent.RECOGNIZER_START_OF_INPUT_EVENT)
            {
                Console.WriteLine("Start of input detected");
                return true;  // Does not actually matter
            }
            if (message.GetEventID() == UniMRCPRecognizerEvent.RECOGNIZER_RECOGNITION_COMPLETE)
            {
                Console.WriteLine(String.Format("Recognition complete: {0} {1}", message.completion_cause, message.completion_reason));
                sem.Release();
                return true;  // Does not actually matter
            }
            return Fail("Unknown message received");
        }

        private void StartStreaming()
        {
            switch (streamType)
            {
                case StreamType.FRAME_BY_FRAME:
                    ((UniRecogStreamFrames)term.stream).started = true;
                    break;
                case StreamType.BUFFERED:
                    try
                    {
                        byte[] data = File.ReadAllBytes(inputFile);
                        ((UniMRCPStreamRxBuffered)term.stream).AddData(data);
                    }
                    catch (IOException)
                    {
                    }
                    break;
                case StreamType.MEMORY:
                case StreamType.FILE:
                    ((UniMRCPStreamRxMemory)term.stream).SetPaused(false);
                    break;
            }
        }
    }


    static void Main(string[] argv)
    {
        if (argv.Length < 2)
        {
            Console.WriteLine("Usage:");
            Console.WriteLine("\tUniRecog \"/path/to/grammar/file\" \"/path/to/input/file\"");
            Environment.Exit(1);
        }
        // Just detect various directory layout constellations
        if (!Directory.Exists(ROOT_DIR))
            ROOT_DIR = "../../../../UniMRCP";
        if (!Directory.Exists(ROOT_DIR))
            ROOT_DIR = "../../../../unimrcp";
        string grammarFile = argv[0].Trim();
        string inputFile = argv[1].Trim();

        ushort major, minor, patch;
        UniMRCPClient.WrapperVersion(out major, out minor, out patch);
        Console.WriteLine("This is a sample C# UniMRCP client recognizer scenario.");
        Console.WriteLine("Wrapper version: {0}.{1}.{2}", major, minor, patch);
        Console.WriteLine("Use client configuration from {0}/conf/unimrcpclient.xml", ROOT_DIR);
        Console.WriteLine("Use profile {0}", MRCP_PROFILE);
        Console.WriteLine("Use recognition grammar: `{0}'", grammarFile);
        Console.WriteLine("Use recognition input file: `{0}'", inputFile);
        Console.WriteLine();
        Console.WriteLine("Press enter to start the session...");
        Console.ReadKey();

        UniRecogLogger logger = new UniRecogLogger();
        try
        {
            // Initialize platform first
            UniMRCPClient.StaticInitialize(logger, UniMRCPLogPriority.INFO);
        }
        catch (ApplicationException ex)
        {
            Console.WriteLine("Unable to initialize platform: " + ex.Message);
            Environment.Exit(1);
        }

        UniMRCPClient client = null;
        UniRecogSession sess = null;
        UniRecogChannel chan = null;
        try
        {
            // Create and start client in a root dir
            client = new UniMRCPClient(ROOT_DIR, true);
            // Create a session using MRCP profile MRCP_PROFILE
            sess = new UniRecogSession(client, MRCP_PROFILE);
            // Create audio termination with capabilities
            UniRecogTermination term = new UniRecogTermination(sess, inputFile);
            term.AddCapability("LPCM", UniMRCPSampleRate.SAMPLE_RATE_8000);
            // Semaphore to wait for completion
            Semaphore sem = new Semaphore(0, 1);
            // Add signaling channel (and start processing in OnAdd method
            chan = new UniRecogChannel(sess, term, sem, grammarFile, inputFile);

            // Now wait until the processing finishes
            sem.WaitOne();
        }
        catch (ApplicationException ex)
        {
            err = 1;
            Console.WriteLine("An error occured: " + ex.Message);
        }

        // Ensure correct destruction order (do not let GC decide)
        if (chan != null) chan.Dispose();
        if (sess != null) sess.Dispose();
        if (client != null) client.Dispose();

        UniMRCPClient.StaticDeinitialize();
        Console.WriteLine("Program finished, memory released. Press enter to exit.");
        Console.ReadKey();
        Environment.Exit(err);
    }
}
