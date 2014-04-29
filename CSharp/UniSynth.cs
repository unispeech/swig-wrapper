using System;
using System.IO;
using System.Threading;

class UniSynth
{
    // UniMRCP client root directory
    static string ROOT_DIR = "../../../../trunk";
    // UniMRCP profile to use for communication with server
    const string MRCP_PROFILE = "uni1";
    // File to write synthesized text to (ROOT_DIR/data/PCM_OUT_FILE)
    const string PCM_OUT_FILE = "UniSynth.pcm";

    static int err = 0;

    // User-defined logger
    class UniSynthLogger : UniMRCPLogger
    {
        // Log messages go here
        public override bool Log(string file, uint line, UniMRCPLogPriority priority, string message)
        {
            Console.WriteLine("  {0}", message);
            return true;
        }
    }


    class UniSynthSession : UniMRCPClientSession
    {
        public UniSynthSession(UniMRCPClient client, string profile) :
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


    class UniSynthStream : UniMRCPStreamTx
    {
        // Set to true upon SPEAK's IN-PROGRESS
        public bool started = false;
        // Output file descriptor
        private FileStream f = null;
        // Buffer to copy audio data to
        private byte[] buf = null;

        public UniSynthStream(string pcmFile)
        {
            f = new FileStream(pcmFile, FileMode.Create, FileAccess.Write);
        }

        // Called when an audio frame arrives
        public override bool WriteFrame()
        {
            if (!started)
                return false;
            uint frameSize = GetDataSize();
            if (frameSize <= 0)
                return true;
            if (buf == null)
                buf = new byte[frameSize];
            GetData(buf, frameSize);
            f.Write(buf, 0, (int) frameSize);
            return true;
        }
    }


    class UniSynthTermination : UniMRCPAudioTermination
    {
        public UniSynthStream stream;

        public UniSynthTermination(UniMRCPClientSession sess, string outFile) :
            base(sess)
        {
            stream = new UniSynthStream(outFile);
        }

        public override UniMRCPStreamTx OnStreamOpenTx(bool enabled, byte payload_type, string name, string format, byte channels, uint freq)
        {
            // Configure outgoing stream here
            return stream;
        }
    }


    class UniSynthChannel : UniMRCPSynthesizerChannel
    {
        private UniSynthTermination term;
        private Semaphore sem;
        private string text;

        public UniSynthChannel(UniSynthSession sess, UniSynthTermination term, Semaphore sem, string text) :
            base(sess, term)
        {
            this.term = term;
            this.sem = sem;
            this.text = text;
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
            // Start processing here
            UniMRCPSynthesizerMessage msg = CreateMessage(UniMRCPSynthesizerMethod.SYNTHESIZER_SPEAK);
            msg.content_type = "text/plain";
            msg.SetBody(text);
            return msg.Send();
        }

        // Response or event from the server arrived
        public override bool OnMessageReceive(UniMRCPSynthesizerMessage message)
        {
            // Analyze message, update your application state and reply messages here
            if (message.GetMsgType() == UniMRCPMessageType.RESPONSE)
            {
                if (message.GetStatusCode() != UniMRCPStatusCode.CODE_SUCCESS)
                    return Fail(String.Format("SPEAK request failed: {0}", message.GetStatusCode()));
                if (message.GetRequestState() != UniMRCPRequestState.STATE_INPROGRESS)
                    return Fail(String.Format("Failed to start SPEAK processing"));
                // Start writing audio to the file
                term.stream.started = true;
                return true;  // Does not actually matter
            }
            if (message.GetMsgType() != UniMRCPMessageType.EVENT)
                return Fail("Unexpected message from the server");
            if (message.GetEventID() == UniMRCPSynthesizerEvent.SYNTHESIZER_SPEAK_COMPLETE)
            {
                Console.WriteLine(String.Format("Speak complete: {0} {1}", message.completion_cause, message.completion_reason));
                term.stream.started = false;
                sem.Release();
                return true;  // Does not actually matter
            }
            return Fail("Unknown message received");
        }
    }


    static void Main(string[] argv)
    {
        if (argv.Length < 1)
        {
            Console.WriteLine("Usage:");
            Console.WriteLine("\tUniSynth \"This is a synthetic voice.\"");
            Environment.Exit(1);
        }
        // Just detect various directory layout constellations
        if (!Directory.Exists(ROOT_DIR))
            ROOT_DIR = "../../../../UniMRCP";
        if (!Directory.Exists(ROOT_DIR))
            ROOT_DIR = "../../../../unimrcp";
        string text = String.Join(" ", argv).Trim();
        string outfile = String.Format("{0}/data/{1}", ROOT_DIR, PCM_OUT_FILE);

        ushort major, minor, patch;
        UniMRCPClient.WrapperVersion(out major, out minor, out patch);
        Console.WriteLine("This is a sample C# UniMRCP client synthesizer scenario.");
        Console.WriteLine("Wrapper version: {0}.{1}.{2}", major, minor, patch);
        Console.WriteLine("Use client configuration from {0}/conf/unimrcpclient.xml", ROOT_DIR);
        Console.WriteLine("Use profile {0}", MRCP_PROFILE);
        Console.WriteLine("Synthesize text: `{0}'", text);
        Console.WriteLine("Write output to file: {0}", outfile);
        Console.WriteLine();
        Console.WriteLine("Press enter to start the session...");
        Console.ReadKey();

        UniSynthLogger logger = new UniSynthLogger();
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
        UniSynthSession sess = null;
        UniSynthChannel chan = null;
        try
        {
            // Create and start client in a root dir
            client = new UniMRCPClient(ROOT_DIR, true);
            // Create a session using MRCP profile MRCP_PROFILE
            sess = new UniSynthSession(client, MRCP_PROFILE);
            // Create audio termination with capabilities
            UniSynthTermination term = new UniSynthTermination(sess, outfile);
            term.AddCapability("LPCM", UniMRCPSampleRate.SAMPLE_RATE_8000);
            // Semaphore to wait for completion
            Semaphore sem = new Semaphore(0, 1);
            // Add signaling channel (and start processing in OnAdd method
            chan = new UniSynthChannel(sess, term, sem, text);

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
