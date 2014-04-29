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
MRCP_PROFILE = "uni1";
# File to write synthesized text to (ROOT_DIR/data/PCM_OUT_FILE)
PCM_OUT_FILE = "UniSynth.pcm";

# Import UniMRCP symbols
from UniMRCP import *


# User-defined logger
class UniSynthLogger(UniMRCPLogger):
    def __init__(self):
        super(UniSynthLogger, self).__init__()

    # Log messages go here
    def Log(self, file, line, prio, message):
        print "  %s\n" % message,  # Ensure atomic logging, do not intermix
        return True


class UniSynthSession(UniMRCPClientSession):
    def __init__(self, client, profile):
        super(UniSynthSession, self).__init__(client, profile)

    def OnTerminate(self, status):
        print "Session terminated with code", status
        return True

    def OnTerminateEvent(self):
        print "Session terminated unexpectedly"
        return True


class UniSynthStream(UniMRCPStreamTx):
    def __init__(self, pcmFile):
        super(UniSynthStream, self).__init__()
        # Set to True upon SPEAK's IN-PROGRESS
        self.started = False
        # Output file descriptor
        self.f = open(pcmFile, 'wb')
        # Buffer to copy audio data to
        self.buf = None

    # Called when an audio frame arrives
    def WriteFrame(self):
        if not self.started:
            return False
        frameSize = self.GetDataSize()
        if frameSize:
            if not self.buf:
                self.buf = bytearray(frameSize)
            self.GetData(self.buf)
            self.f.write(self.buf)
        return True


class UniSynthTermination(UniMRCPAudioTermination):
    def __init__(self, sess, outfile):
        super(UniSynthTermination, self).__init__(sess)
        self.stream = UniSynthStream(outfile)

    def OnStreamOpenTx(self, enabled, payload_type, name, format, channels, freq):
        # Configure outgoing stream here
        return self.stream


class UniSynthChannel(UniMRCPSynthesizerChannel):
    def __init__(self, sess, term, sem, text):
        super(UniSynthChannel, self).__init__(sess, term)
        self.sess = sess
        self.term = term
        self.sem = sem
        self.text = text

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
        # Start processing here
        msg = self.CreateMessage(SYNTHESIZER_SPEAK)
        msg.content_type = "text/plain"
        msg.SetBody(text)
        return msg.Send()

    # Response or event from the server arrived
    def OnMessageReceive(self, message):
        # Analyze message, update your application state and reply messages here
        if message.GetMsgType() == MRCP_MESSAGE_TYPE_RESPONSE:
            if message.GetStatusCode() != MRCP_STATUS_CODE_SUCCESS:
                return self.Fail("SPEAK request failed: %d" % message.GetStatusCode())
            if message.GetRequestState() != MRCP_REQUEST_STATE_INPROGRESS:
                return self.Fail("Failed to start SPEAK processing")
            # Start writing audio to the file
            self.term.stream.started = True
            return True  # Does not actually matter
        if message.GetMsgType() != MRCP_MESSAGE_TYPE_EVENT:
            return self.Fail("Unexpected message from the server")
        if message.GetEventID() == SYNTHESIZER_SPEAK_COMPLETE:
            print "Speak complete:", message.completion_cause, message.completion_reason
            self.term.stream.started = False
            self.sem.release()
            return True  # Does not actually matter
        return self.Fail("Unknown message received")


if len(sys.argv) < 2:
    print "Usage:"
    print '\t%s "This is a synthetic voice."' % sys.argv[0]
    sys.exit(1)
text = ' '.join(sys.argv[1:]).strip()
outfile = "%s/data/%s" % (ROOT_DIR, PCM_OUT_FILE)

print "This is a sample Python UniMRCP client synthesizer scenario."
print "Wrapper version: {0}.{1}.{2}".format(*UniMRCPClient.WrapperVersion())
print "Use client configuration from %s/conf/unimrcpclient.xml" % ROOT_DIR
print "Use profile %s" % MRCP_PROFILE
print "Synthesize text: `%s'" % text
print "Write output to file: %s" % outfile
print
print "Press enter to start the session..."
sys.stdin.read(1)

logger = UniSynthLogger()
try:
    # Initialize platform first
    UniMRCPClient.StaticInitialize(logger, APT_PRIO_INFO)
except RuntimeError as ex:
    print "Unable to initialize platform:", ex
    sys.exit(1)

err = 0
client = None
sess = None
chan = None
try:
    # Create and start client in a root dir
    client = UniMRCPClient(ROOT_DIR, True)
    # Create a session using MRCP profile MRCP_PROFILE
    sess = UniSynthSession(client, MRCP_PROFILE)
    # Create audio termination with capabilities
    term = UniSynthTermination(sess, outfile)
    term.AddCapability("LPCM", SAMPLE_RATE_8000)
    # Semaphore to wait for completion
    sem = threading.Semaphore(0)
    # Add signaling channel (and start processing in OnAdd method)
    chan = UniSynthChannel(sess, term, sem, text)

    # Now wait until the processing finishes
    sem.acquire()
except RuntimeError as ex:
    err = 1
    print "An error occured:", ex
except:
    err = 1
    print "Unknown error occured"

# Ensure correct destruction order (do not let GC decide)
if chan: del chan
if sess: del sess
if client: del client

UniMRCPClient.StaticDeinitialize()
print "Program finished, memory released. Press enter to exit."
sys.stdin.read(1)
sys.exit(err)
