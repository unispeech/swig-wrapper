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

// Types of streaming
enum StreamType {
    ST_FRAME_BY_FRAME,
    ST_BUFFERED,
    ST_MEMORY,
    ST_FILE
};

// Choose type of streaming to demonstrate
static StreamType const streamType = ST_FILE;

static int err = 0;


// User-defined logger
class UniRecogLogger : public UniMRCPLogger
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


class UniRecogSession : public UniMRCPClientSession {
public:
	UniRecogSession(UniMRCPClient* client, char const* profile) :
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


class UniRecogStreamFrames : public UniMRCPStreamRx {
	// Input file descriptor
	ifstream f;
	// Buffer to copy audio data to
	char* buf;
public:
	// Set to true upon RECOGNIZE's IN-PROGRESS
	bool started;

	UniRecogStreamFrames(char const* pcmFile) :
		f(pcmFile, ios::binary | ios::in),
		buf(NULL),
		started(false)
	{
	}

	virtual ~UniRecogStreamFrames()
	{
		delete[] buf;
	}

	// Called when an audio frame is needed
	virtual bool ReadFrame()
	{
		if (!started)
			return false;
		size_t frameSize = GetDataSize();
		if (!frameSize)
			return true;
		if (!buf)
			buf = new char[frameSize];
		f.read(buf, frameSize);
		if (f.gcount() > 0) {
			SetData(buf, static_cast<size_t>(f.gcount()));
			return true;
		}
		return false;
	}
};


class UniRecogTermination : public UniMRCPAudioTermination {
public:
	UniMRCPStreamRx* stream;

	UniRecogTermination(UniMRCPClientSession* sess, char const* pcmfile) :
		UniMRCPAudioTermination(sess),
		stream(NULL)
	{
		switch (streamType) {
		case ST_FRAME_BY_FRAME:
			stream = new UniRecogStreamFrames(pcmfile);
			break;
		case ST_BUFFERED:
			stream = new UniMRCPStreamRxBuffered();
			break;
		case ST_MEMORY: 
			{
				ifstream stm(pcmfile);
				string data((std::istreambuf_iterator<char>(stm)), (std::istreambuf_iterator<char>()));
				stream = new UniMRCPStreamRxMemory(data.data(), data.length(), true, UniMRCPStreamRxMemory::SRM_NOTHING, true);
				break;
			}
		case ST_FILE:
			stream = new UniMRCPStreamRxFile(pcmfile, 0, UniMRCPStreamRxMemory::SRM_NOTHING, true);
			break;
		}
	}

	virtual ~UniRecogTermination()
	{
		delete stream;
	}

	virtual UniMRCPStreamRx* OnStreamOpenRx(bool enabled, unsigned char payload_type, char const* name,
	                                        char const* format, unsigned char channels, unsigned freq)
	{
		(void) enabled;
		(void) payload_type;
		(void) name;
		(void) format;
		(void) channels;
		(void) freq;
		// Configure incoming stream here
		return stream;
	}
};


class UniRecogChannel : public UniMRCPRecognizerChannel {
	UniRecogTermination* term;
	char const* grammarfile;
	char const* inputfile;

public:
	UniRecogChannel(UniMRCPClientSession* sess, UniRecogTermination* term, char const* grammarfile, char const* inputfile) :
		UniMRCPRecognizerChannel(sess, term),
		term(term),
		grammarfile(grammarfile),
		inputfile(inputfile)
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

	void StartStreaming()
	{
		switch (streamType) {
		case ST_FRAME_BY_FRAME:
			static_cast<UniRecogStreamFrames*>(term->stream)->started = true;
			break;
		case ST_BUFFERED:
			{
				ifstream stm(inputfile);
				string data((std::istreambuf_iterator<char>(stm)), (std::istreambuf_iterator<char>()));
				static_cast<UniMRCPStreamRxBuffered*>(term->stream)->AddData(data.data(), data.length());
				break;
			}
		case ST_MEMORY: 
		case ST_FILE:
			static_cast<UniMRCPStreamRxMemory*>(term->stream)->SetPaused(false);
			break;
		}
	}

public:
	// MRCP connection established, start communication
	virtual bool OnAdd(UniMRCPSigStatusCode status)
	{
		if (status != MRCP_SIG_STATUS_CODE_SUCCESS)
			return Fail(cout << "Failed to add channel " << status);
		// Load grammar content from file
		ifstream stm(grammarfile);
		string content((std::istreambuf_iterator<char>(stm)), (std::istreambuf_iterator<char>()));
		// Start processing here
		UniMRCPRecognizerMessage* msg = CreateMessage(RECOGNIZER_RECOGNIZE);
		msg->content_type_set("application/grammar+xml");
		msg->SetBody(content.data(), content.length());
		return msg->Send();
	}

	// Response from the server arrived
	virtual bool OnMessageReceive(UniMRCPRecognizerMessage const* message)
	{
		// Analyze, update your application state and reply messages here
		if (message->GetMsgType() == MRCP_MESSAGE_TYPE_RESPONSE) {
			if (message->GetStatusCode() != MRCP_STATUS_CODE_SUCCESS)
				return Fail(cout << "RECOGNIZE request failed: " << message->GetStatusCode());
			if (message->GetRequestState() != MRCP_REQUEST_STATE_INPROGRESS)
				return Fail(cout << "Failed to start RECOGNIZE processing");
			// Start streaming audio from the file
			StartStreaming();
			return true;  // Does not actually matter
		}
		if (message->GetMsgType() != MRCP_MESSAGE_TYPE_EVENT)
			return Fail(cout << "Unexpected message from the server");
		if (message->GetEventID() == RECOGNIZER_START_OF_INPUT_EVENT) {
			cout << "Start of input detected" << endl;
			return true;  // Does not acutally matter
		}
		if (message->GetEventID() == RECOGNIZER_RECOGNITION_COMPLETE) {
			char const* reason = message->completion_reason_get();
			cout << "Recognition complete: " << message->completion_cause_get() << ' ' <<
				(reason ? reason : "") << endl;
			return true;
		}
		return Fail(cout << "Unknown message arrived");
	}
};


int main(int argc, char const* const argv[])
{
	if (argc < 3) {
		cout << "Usage:" << endl <<
			"\t" << argv[0] << " \"/path/to/grammar/file\" \"/path/to/input/file\"" << endl;
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
	char const* grammarfile = argv[1];
	char const* inputfile = argv[2];

	unsigned short major, minor, patch;
	UniMRCPClient::WrapperVersion(major, minor, patch);
	cout << "This is a sample C++ UniMRCP client Recognizer scenario." << endl <<
		"Wrapper version: " << major << '.' << minor << '.' << patch << endl <<
		"Use client configuration from " << ROOT_DIR << "/conf/unimrcpclient.xml" << endl <<
		"Use profile " << MRCP_PROFILE << endl <<
		"Use recognition grammar: `" << grammarfile << '\'' << endl <<
		"Use recognition input file: " << inputfile << endl << endl <<
		"Press enter to start the session..." << endl <<
		"Press enter the second time to finish the program." << endl;
	cin.get();

	UniRecogLogger logger;
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
		UniRecogSession sess(&client, MRCP_PROFILE);
		// Create audio termination with capabilities
		UniRecogTermination term(&sess, inputfile);
		term.AddCapability("LPCM", SAMPLE_RATE_8000);
		// Add signaling channel (and start processing in OnAdd method
		UniRecogChannel chan(&sess, &term, grammarfile, inputfile);

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
