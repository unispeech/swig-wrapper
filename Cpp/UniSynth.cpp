#include "UniMRCP-wrapper.h"
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
using namespace std;

// UniMRCP client root directory
static char const ROOT_DIR1[] = "../../../../trunk";
static char const ROOT_DIR2[] = "../../../../UniMRCP";
static char const ROOT_DIR3[] = "../../../../unimrcp";
static char const*ROOT_DIR = ROOT_DIR1;
// UniMRCP profile to use for communication with server
static char const MRCP_PROFILE[] = "uni1";
// File to write synthesized text to (ROOT_DIR/data/PCM_OUT_FILE)
static char const PCM_OUT_FILE[] = "UniSynth.pcm";

static int err = 0;


// User-defined logger
class UniSynthLogger : public UniMRCPLogger
{
public:
	virtual bool Log(char const* file, unsigned line, UniMRCPLogPriority prio, char const* msg)
	{
		(void) file;
		(void) line;
		(void) prio;
		cout << "  " << msg << endl;
		return true;
	}
};


class UniSynthSession : public UniMRCPClientSession {
public:
	UniSynthSession(UniMRCPClient* client, char const* profile) :
		UniMRCPClientSession(client, profile)
	{
	}
	
	virtual bool OnTerminate(UniMRCPSigStatusCode status)
	{
		cout << "Session terminated with code " << status << endl;
		return true;
	}

	virtual bool OnTerminateEvent()
	{
		cerr << "Session terminated unexpectedly" << endl;
		return true;
	}
};


class UniSynthStream : public UniMRCPStreamTx {
	// Output file descriptor
	ofstream f;
	// Buffer to copy audio data to
	char* buf;
public:
	// Set to true upon SPEAK's IN-PROGRESS
	bool started;

	UniSynthStream(char const* pcmFile) :
		f(pcmFile, ios::binary | ios::out),
		buf(NULL),
		started(false)
	{
	}

	virtual ~UniSynthStream()
	{
		delete[] buf;
	}

	// Called when an audio frame arrives
	virtual bool WriteFrame()
	{
		if (!started)
			return false;
		size_t frameSize = GetDataSize();
		if (!frameSize)
			return true;
		if (!buf)
			buf = new char[frameSize];
		GetData(buf, frameSize);
		f.write(buf, frameSize);
		return true;
	}
};


class UniSynthTermination : public UniMRCPAudioTermination {
public:
	UniSynthStream stream;

	UniSynthTermination(UniMRCPClientSession* sess, char const* outfile) :
		UniMRCPAudioTermination(sess),
		stream(outfile)
	{
	}

	virtual UniMRCPStreamTx* OnStreamOpenTx(bool enabled, unsigned char payload_type, char const* name,
	                                        char const* format, unsigned char channels, unsigned freq)
	{
		(void) enabled;
		(void) payload_type;
		(void) name;
		(void) format;
		(void) channels;
		(void) freq;
		// Configure outgoing stream here
		return &stream;
	}
};


class UniSynthChannel : public UniMRCPSynthesizerChannel {
	UniSynthTermination* term;
	char const* text;

public:
	UniSynthChannel(UniMRCPClientSession* sess, UniSynthTermination* term, char const* text) :
		UniMRCPSynthesizerChannel(sess, term),
		term(term),
		text(text)
	{
	}

private:
	// Shorthand for graceful fail: Write message, release semaphore and return false
	bool Fail(ostream& stm)
	{
		stm << endl;
		err = 1;
		return false;
	}

public:
	// MRCP connection established, start communication
	virtual bool OnAdd(UniMRCPSigStatusCode status)
	{
		if (status != MRCP_SIG_STATUS_CODE_SUCCESS)
			return Fail(cout << "Failed to add channel " << status);
		// Start processing here
		UniMRCPSynthesizerMessage* msg = CreateMessage(SYNTHESIZER_SPEAK);
		msg->content_type_set("text/plain");
		msg->SetBody(text);
		return msg->Send();
	}

	// Response from the server arrived
	virtual bool OnMessageReceive(UniMRCPSynthesizerMessage const* message)
	{
		// Analyze, update your application state and reply messages here
		if (message->GetMsgType() == MRCP_MESSAGE_TYPE_RESPONSE) {
			if (message->GetStatusCode() != MRCP_STATUS_CODE_SUCCESS)
				return Fail(cout << "SPEAK request failed: " << message->GetStatusCode());
			if (message->GetRequestState() != MRCP_REQUEST_STATE_INPROGRESS)
				return Fail(cout << "Failed to start SPEAK processing");
			// Start writing audio to the file
			term->stream.started = true;
			return true;  // Does not actually matter
		}
		if (message->GetMsgType() != MRCP_MESSAGE_TYPE_EVENT)
			return Fail(cout << "Unexpected message from the server");
		if (message->GetEventID() == SYNTHESIZER_SPEAK_COMPLETE) {
			char const* reason = message->completion_reason_get();
			cout << "Speak complete: " << message->completion_cause_get() << ' ' <<
				(reason ? reason : "") << endl;
			term->stream.started = false;
			return true;
		}
		return Fail(cout << "Unknown message arrived");
	}
};


int main(int argc, char const* const argv[])
{
	if (argc < 2) {
		cout << "Usage:" << endl <<
			"\t" << argv[0] << " \"This is a synthetic voice.\"" << endl;
		return 1;
	}
	{
		// Just detect various directory layout constellations
		struct stat info;
		if (stat(ROOT_DIR, &info))
			ROOT_DIR = ROOT_DIR2;
		if (stat(ROOT_DIR, &info))
			ROOT_DIR = ROOT_DIR3;
	}
	string text;
	for (int i = 1; i < argc - 1; i++)
		text.append(argv[i]).append(1, ' ');
	text.append(argv[argc - 1]);
	string outfile;
	outfile.append(ROOT_DIR).append("/data/").append(PCM_OUT_FILE);

	unsigned short major, minor, patch;
	UniMRCPClient::WrapperVersion(major, minor, patch);
	cout << "This is a sample C++ UniMRCP client synthesizer scenario." << endl <<
		"Wrapper version: " << major << '.' << minor << '.' << patch << endl <<
		"Use client configuration from " << ROOT_DIR << "/conf/unimrcpclient.xml" << endl <<
		"Use profile " << MRCP_PROFILE << endl <<
		"Synthesize text: `" << text.c_str() << '\'' << endl <<
		"Write output to file: " << outfile.c_str() << endl << endl <<
		"Press enter to start the session..." << endl <<
		"Press enter the second time to finish the program." << endl;
	cin.get();

	UniSynthLogger logger;
	try {
		// Initialize platform first
		UniMRCPClient::StaticInitialize(&logger, APT_PRIO_INFO);
	} catch (UniMRCPException const& ex) {
		cout << "Unable to initialize platform: " << ex.msg << endl;
		return 1;
	}

	try {
		// Create and start client in a root dir
		UniMRCPClient client(ROOT_DIR, true);
		// Create a session using MRCP profile uni1
		UniSynthSession sess(&client, MRCP_PROFILE);
		// Create audio termination with capabilities
		UniSynthTermination term(&sess, outfile.c_str());
		term.AddCapability("LPCM", SAMPLE_RATE_8000);
		// Add signaling channel (and start processing in OnAdd method
		UniSynthChannel chan(&sess, &term, text.c_str());

		// Now wait until the processing finishes
		cin.get();
	} catch (UniMRCPException const& ex) {
		cout << endl << "A UniMRCP error occured: " << ex.msg << endl;
	} catch (std::exception const& ex) {
		cout << endl << "An exception occured: " << ex.what() << endl;
	} catch (...) {
		cout << endl << "Unknown error occured" << endl;
	}
	try {
		UniMRCPClient::StaticDeinitialize();
		cout << endl << "Program finished, memory released. Press any key to exit." << endl;
	} catch (UniMRCPException const& ex) {
		cout << endl << "Failed to deinitialize platform: " << ex.msg << endl;
	}
	cin.get();
	return err;
}
