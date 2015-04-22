#! /usr/bin/env python
# -*- coding: utf-8 -*-
import os
import sys
import threading

# UniMRCP client root directory
ROOT_DIR = "../../../../trunk"
# Just detect various directory layout constellations
if not os.path.isdir(ROOT_DIR):
    ROOT_DIR = "../../../../UniMRCP"
if not os.path.isdir(ROOT_DIR):
    ROOT_DIR = "../../../../unimrcp"
# UniMRCP profile to use for communication with server
MRCP_PROFILE = "uni1"

# Types of streaming
ST_FRAME_BY_FRAME = 0
ST_BUFFERED = 1
ST_MEMORY = 2
ST_FILE = 3

# Choose type of streaming to demonstrate
streamType = ST_FILE


# Import UniMRCP symbols
from UniMRCP import *


# User-defined logger
class UniRecogLogger(UniMRCPLogger):
    def __init__(self):
        super(UniRecogLogger, self).__init__()

    # Log messages go here
    def Log(self, file, line, prio, message):
        print "  %s\n" % message,  # Ensure atomic logging, do not intermix
        return True


class UniRecogSession(UniMRCPClientSession):
    def __init__(self, client, profile):
        super(UniRecogSession, self).__init__(client, profile)

    def OnTerminate(self, status):
        print "Session terminated with code", status
        return True

    def OnTerminateEvent(self):
        print "Session terminated unexpectedly"
        return True


class UniRecogStreamFrames(UniMRCPStreamRx):
    def __init__(self, pcmfile):
        super(UniRecogStreamFrames, self).__init__()
        # Set to True upon RECOGNIZE's IN-PROGRESS
        self.started = False
        # Input file descriptor
        self.f = open(pcmfile, 'rb')

    # Called when an audio frame is needed
    def ReadFrame(self):
        if not self.started:
            return False
        frameSize = self.GetDataSize()
        if frameSize:
            buf = self.f.read(frameSize)
            if len(buf):
                self.SetData(buf)
                return True
        return False


class UniRecogTermination(UniMRCPAudioTermination):
    def __init__(self, sess, pcmfile):
        super(UniRecogTermination, self).__init__(sess)
        if streamType == ST_FRAME_BY_FRAME:
            self.stream = UniRecogStreamFrames(pcmfile)
        elif streamType == ST_BUFFERED:
            self.stream = UniMRCPStreamRxBuffered()
        elif streamType == ST_MEMORY:
            data = open(pcmfile, "rb").read()
            self.stream = UniMRCPStreamRxMemory(data, True, UniMRCPStreamRxMemory.SRM_NOTHING, True)
        elif streamType == ST_FILE:
            self.stream = UniMRCPStreamRxFile(pcmfile, 0, SRM_NOTHING, True)

    def OnStreamOpenRx(self, enabled, payload_type, name, format, channels, freq):
        # Configure outgoing stream here
        return self.stream


class UniRecogChannel(UniMRCPRecognizerChannel):
    def __init__(self, sess, term, sem, grammarfile, inputfile):
        super(UniRecogChannel, self).__init__(sess, term)
        self.sess = sess
        self.term = term
        self.sem = sem
        self.grammarfile = grammarfile
        self.inputfile = inputfile

    # Shorthand for graceful fail: Write message, release semaphore and return False
    def Fail(self, msg):
        global err
        print msg
        err = 1
        self.sem.release()
        return False

    # MRCP connection established, start communication
    def OnAdd(self, status):
        if status != MRCP_SIG_STATUS_CODE_SUCCESS:
            return self.Fail("Failed to add channel: %d" % status)
        # Load grammar content from file
        content = open(grammarfile, "rb").read()
        # Start processing here
        msg = self.CreateMessage(RECOGNIZER_RECOGNIZE)
        msg.content_type = "application/grammar+xml"
        msg.SetBody(content)
        return msg.Send()

    # Response or event from the server arrived
    def OnMessageReceive(self, message):
        # Analyze message, update your application state and reply messages here
        if message.GetMsgType() == MRCP_MESSAGE_TYPE_RESPONSE:
            if message.GetStatusCode() != MRCP_STATUS_CODE_SUCCESS:
                return self.Fail("RECOGNIZE request failed: %d" % message.GetStatusCode())
            if message.GetRequestState() != MRCP_REQUEST_STATE_INPROGRESS:
                return self.Fail("Failed to start RECOGNIZE processing")
            # Start streaming audio from the file
            self.StartStreaming()
            return True  # Does not actually matter
        if message.GetMsgType() != MRCP_MESSAGE_TYPE_EVENT:
            return self.Fail("Unexpected message from the server")
        if message.GetEventID() == RECOGNIZER_START_OF_INPUT_EVENT:
            print "Start of input detected"
            return True
        if message.GetEventID() == RECOGNIZER_RECOGNITION_COMPLETE:
            print "Recognition complete:", message.completion_cause, message.completion_reason
            self.sem.release()
            return True  # Does not actually matter
        return self.Fail("Unknown message received")

    def StartStreaming(self):
        if streamType == ST_FRAME_BY_FRAME:
            self.term.stream.started = True
        elif streamType == ST_BUFFERED:
            data = open(self.inputfile, "rb").read()
            self.term.stream.AddData(data)
        elif streamType == ST_MEMORY:
            self.term.stream.SetPaused(False)
        elif streamType == ST_FILE:
            self.term.stream.SetPaused(False)


if len(sys.argv) < 3:
    print "Usage:"
    print '\t%s "/path/to/grammar/file" "/path/to/input/file"' % sys.argv[0]
    sys.exit(1)
grammarfile = sys.argv[1]
inputfile = sys.argv[2]

print "This is a sample Python UniMRCP client recognizer scenario."
print "Wrapper version: {0}.{1}.{2}".format(*UniMRCPClient.WrapperVersion())
print "Use client configuration from %s/conf/unimrcpclient.xml" % ROOT_DIR
print "Use profile %s" % MRCP_PROFILE
print "Use recognition grammar: `%s'" % grammarfile
print "Use recognition input file: %s" % inputfile
print
print "Press enter to start the session..."
sys.stdin.read(1)

logger = UniRecogLogger()
try:
    # Initialize platform first
    UniMRCPClient.StaticInitialize(logger, APT_PRIO_DEBUG)
except RuntimeError as ex:
    print "Unable to initialize platform:", ex
    sys.exit(1)

err = 0
client = None
sess = None
term = None
chan = None
try:
    # Create and start client in a root dir
    client = UniMRCPClient(ROOT_DIR, True)
    # Create a session using MRCP profile MRCP_PROFILE
    sess = UniRecogSession(client, MRCP_PROFILE)
    # Create audio termination with capabilities
    term = UniRecogTermination(sess, inputfile)
    term.AddCapability("LPCM", SAMPLE_RATE_8000)
    # Semaphore to wait for completion
    sem = threading.Semaphore(0)
    # Add signaling channel (and start processing in OnAdd method)
    chan = UniRecogChannel(sess, term, sem, grammarfile, inputfile)

    # Now wait until the processing finishes
    sem.acquire()
except Exception as ex:
    err = 1
    print "An error occured:", ex

# Ensure correct destruction order (do not let GC decide)
chan = None
sess = None
client = None

UniMRCPClient.StaticDeinitialize()
print "Program finished, memory released. Press enter to exit."
sys.stdin.read(1)
sys.exit(err)
