/*
 * Copyright 2014 SpeechTech, s.r.o. http://www.speechtech.cz/en
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Id$
 */

#ifndef UNIMRCP_WRAPPER_H
#define UNIMRCP_WRAPPER_H

/**
 * @file UniMRCP-wrapper.h
 * @brief UniMRCP Client high-level interface.
 */

#include <cstddef>  // For NULL
#include <cstdarg>  // For va_list

#ifdef DOXYGEN
/**
 * @brief Define if building a (wrapper) library and want to export the C++ methods.
 */
#define UNIMRCP_WRAPPER_EXPORT

/**
 * @brief Define if you want to use the C++ methods from a library built usign UNIMRCP_WRAPPER_EXPORT.
 *
 * This may not be necessary.
 */
#define UNIMRCP_WRAPPER_IMPORT

/**
 * @brief Defined when building wrapper library (i.e. included from UniMRCP-wrapper.cpp)
 */
#define UNIMRCP_WRAPPER_CPP

/**
 * @brief Defined when building the library wrapped by SWIG
 */
#define WRAPPER

/**
 * @brief When building the wrapper, define to platform name, e.g. "Python" (for logging purposes).
 */
#define TARGET_PLATFORM "cpp"

/**
 * @brief Define to trace/log media frames flow
 */
#define UW_TRACE_BUFFERS
#endif  // ifdef DOXYGEN

/**
 * @brief Declarator to export C++ methods if wanted.
 */
#if defined(WRAPPER_DECL)
	// User-defined declarator
#elif defined(SWIG) || defined(DOXYGEN)
#	define WRAPPER_DECL
#elif defined(UNIMRCP_WRAPPER_EXPORT)
#	if defined(_MSC_VER)
#		define WRAPPER_DECL __declspec(dllexport)
#	elif defined(__GNUC__) && (__GNUC__ >= 4)
#		define WRAPPER_DECL __attribute__((visibility("default")))
#	endif
#elif defined(UNIMRCP_WRAPPER_IMPORT)
#	if defined(_MSC_VER)
#		define WRAPPER_DECL __declspec(dllimport)
#	endif
#endif
#ifndef WRAPPER_DECL
#	define WRAPPER_DECL
#endif

/**
 * @mainpage Documentation
 * This is UniMRCP (http://unimrcp.org/) client C++ interface
 * which can also be wrapped by SWIG (http://swig.org/)
 * currently for .NET, Python and Java languages.
 * Binaries are built using CMake (http://cmake.org/).
 *
 * UniMRCP client application must first call UniMRCPClient::StaticInitialize
 * to initialize the platform. Then UniMRCPClient object is created.
 * UniMRCP client sessions are esteblished creating UniMRCPClientSession objects.
 * For each session UniMRCPAudioTermination objects are created to handle media streams.
 * To each session (and for media termination) UniMRCPClientChannel objects are added
 * to handle signaling. But do not use UniMRCPClientChannel directly, use specialized
 * resource channels, e.g. #UniMRCPRecognizerChannel (UniMRCPClientResourceChannel\<#MRCP_RECOGNIZER\>),
 * #UniMRCPSynthesizerChannel (UniMRCPClientResourceChannel\<#MRCP_SYNTHESIZER\>)
 * and #UniMRCPRecorderChannel (UniMRCPClientResourceChannel\<#MRCP_RECORDER\>)
 * to handle signaling for above specified resources.
 *
 * To implement/customize methods of terminations, streams, sessions and channels, just
 * override their virtual methods. See this documentation for further details.
 *
 *
 * @section sec-MRCP MRCP in a Nutshell
 * For specifications, see
 * <a href="http://tools.ietf.org/html/rfc4463">RFC4463</a> (MRCPv1) and
 * <a href="http://tools.ietf.org/html/rfc6787">RFC6787</a> (MRCPv2).
 *
 * MRCPv1 uses RTSP (TCP-based) protocol to setup media session (via SDP) and
 * for signaling as well.
 *
 * MRCPv2 uses SIP (UDP or TCP-based) to setup media session and MRCP signaling.
 * Both media formats, connection endpoints and MRCP endpoint are described in
 * SDP answer. The client then connects to the MRCP TCP IP/port contained in
 * the answer.
 *
 * See below for some typical message flow examples.
 *
 *
 * @section sec-UniMRCP UniMRCP in a Nutshell
 * UniMRCP is an excellent MRCP stack written in C that transparently works with
 * both versions of the protocol. Protocol version, connection details and other
 * settings are specified in configuration proviles. We will focus on client use case only.
 * Its binary distribution assumes following subdirectories in its root directory:
 * - @c bin/ -- contains executables,
 * - @c conf/ -- contains configurations (unimrcpclient.xml),
 * - @c data/ -- contains data files (sounds, grammars etc.) used by examples,
 * - @c log/ -- default location for logging.
 *
 * After client initialization, sessions are created. The session is associated with
 * a media termination and channels are added to it. Media terminations manage
 * streams and channels send and receive messages.
 *
 *
 * @section sec-using-wrapper Using the Wrapper
 * See examples (UniSynth).
 *
 * The application using UniMRCPWrapper is event-driven. Main processing starts in
 * UniMRCPClientChannel::OnAdd.
 * -# Initialize the platform first by UniMRCPClient::StaticInitialize.
 *    - You can initialize the platform to use the directory structure described
 *      above or you can use variant independent of filesystem.
 * -# Create UniMRCPClient object.
 * -# Create UniMRCPClientSession object using specified profile.
 *    - Several methods (events) may be overriden, e.g.
 *      UniMRCPClientSession::OnTerminate, UniMRCPClientSession::OnTerminateEvent.
 * -# Create UniMRCPAudioTermination, set its options and add desired capabilities
 *    using UniMRCPAudioTermination::AddCapability.
 *    - Override at least UniMRCPAudioTermination::OnStreamOpenRx
 *      or UniMRCPAudioTermination::OnStreamOpenTx to create outgoing or incoming
 *      streams respectively (the direction is from the server's point of view).
 *    - In the methods check negotiated stream parameters and return instances
 *      of UniMRCPStreamRx (UniMRCPStreamRxBuffered) or UniMRCPStreamTx, override
 *      their UniMRCPStreamRx::ReadFrame or UniMRCPStreamTx::WriteFrame to use
 *      UniMRCPStreamRx::SetData or UniMRCPStreamTx::GetData to transmit
 *      or receive data respectively.
 *      - Remember, sending or receiving data should be synchronized (started)
 *        with MRCP synchronization, i.e. after receiving @c IN-PROGRESS response
 *        to @c SPEAK or @c RECOGNIZE etc.
 * -# Create specialized UniMRCPClientResourceChannel (#UniMRCPSynthesizerChannel,
 *    #UniMRCPRecognizerChannel or #UniMRCPRecorderChannel).
 *    It is automatically added to the session.
 *    - Override UniMRCPClientChannel::OnAdd and if the #UniMRCPSigStatusCode
 *      parameter is #MRCP_SIG_STATUS_CODE_SUCCESS, start sending messages
 *      and return @c true (@c false causes disconnection).
 *    - Override UniMRCPClientResourceChannel::OnMessageReceive to get responses and events
 *      from the server.
 * -# When done, call UniMRCPClientSession::Terminate.
 *    In UniMRCPClientSession::OnTerminate you can UniMRCPClientSession::Destroy
 *    the session and thus free all the memory used during the session.
 *
 *
 * @section sec-mrcp-flow MRCP Message Flow Examples
 * Few typical MRCP message flow examples.
 *
 *
 * @subsection sec-synth Simple Synthesis
 * Bundled UniSynth example.
 * <pre>
 * C>S: Send SPEAK message with Content-Type header and body.
 *      UniMRCPClientChannel::OnAdd
 * C<S: Receive IN-PROGRESS response.
 *      UniMRCPSynthesizerChannel::OnMessageReceive
 * (Client now consumes incoming media.) UniMRCPStreamTx::WriteFrame
 * C<S: Receive SPEAK-COMPLETE event.
 *      UniMRCPSynthesizerChannel::OnMessageReceive
 * </pre>
 *
 *
 * @subsection sec-recog1 Simple Recognition
 * <pre>
 * C>S: Send RECOGNIZE message with Content-Type header and body with grammar.
 *      UniMRCPClientChannel::OnAdd
 * C<S: Receive IN-PROGRESS response.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * (Client now sends audio and/or DTMF to recognize.) UniMRCPStreamRx::ReadFrame
 * C<S: May receive START-OF-INPUT event.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * C<S: Receive RECOGNITION-COMPLETE event.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * </pre>
 *
 *
 * @subsection sec-recog2 Recognition with Extra Grammar(s)
 * <pre>
 * C>S: Send DEFINE-GRAMMAR message with Content-Type, body, and
 *          Content-Id set to grammar ID.
 *      UniMRCPClientChannel::OnAdd
 * C<S: Receive DEFINE-GRAMMAR response.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * C>S: Send RECOGNIZE message with
 *          Content-Type=text/uri-list and
 *          body with one grammar ID per line.
 * C<S: Receive IN-PROGRESS response.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * (Client now sends audio and/or DTMF to recognize.) UniMRCPStreamRx::ReadFrame
 * C<S: May receive START-OF-INPUT event.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * C<S: Receive RECOGNITION-COMPLETE event.
 *      UniMRCPRecognizerChannel::OnMessageReceive
 * </pre>
 */

/** @brief Helper macro for declaring exceptions so that it can be disabled */
#ifndef THROWS
#	define THROWS throw
#endif
#ifdef _MSC_VER
#	pragma warning (disable: 4290)
#endif


/*
 * Environment setup:
 * - If ENUM_MEM is defined as NAME_PREFIXED, all enum members have full prefix and
 *     are unique througout entire wrapper. Necessary for e.g. C++.
 * - If ENUM_MEM is defined as NAME_NOPREFIX, enum members are as short as possible. Useful for e.g. C#.
 * - TARGET platform defined to target platform name.
 */
/**
 * @def ENUM_MEM
 * @brief Depending on binding platform expands to NAME_PREFIXED or NAME_NOPREFIX or NAME_UWPREFIXED
 */
#if defined(DOXYGEN)
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#elif !defined(SWIG)
#	ifdef UNIMRCP_WRAPPER_CPP
#		define TARGET_PLATFORM "cpp"
#		define ENUM_MEM(pre, name) NAME_UWPREFIXED(pre, name)
#	else
#		define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#	endif
#elif defined(SWIGCSHARP)
#	define TARGET_PLATFORM "CSharp"
#	define ENUM_MEM(pre, name) NAME_NOPREFIX(pre, name)
#elif defined(SWIGPYTHON)
#	define TARGET_PLATFORM "Python"
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#elif defined(SWIGJAVA)
#	define TARGET_PLATFORM "Java"
#	define ENUM_MEM(pre, name) NAME_NOPREFIX(pre, name)
#elif defined(SWIGLUA)
#	define TARGET_PLATFORM "Lua"
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#elif defined(SWIGOCTAVE)
#	define TARGET_PLATFORM "Octave"
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#elif defined(SWIGPERL)
#	define TARGET_PLATFORM "PERL"
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#elif defined(SWIGPHP)
#	define TARGET_PLATFORM "PHP"
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#else
#	warning "Unknown SWIG target platform. Some functions may not work!"
#	define TARGET_PLATFORM "unknown"
#	define ENUM_MEM(pre, name) NAME_PREFIXED(pre, name)
#endif

/** @brief NAME_UWPREFIXED(AAA, BBB) expands to UW_AAABBB to distinguish constants defined in UniMRCP headers */
#define NAME_UWPREFIXED(pre, name) UW_ ## pre ## name
/** @brief NAME_PREFIXED(AAA, BBB) expands to AAABBB */
#define NAME_PREFIXED(pre, name) pre ## name
/** @brief NAME_NOPREFIX(AAA, BBB) expands to BBB */
#define NAME_NOPREFIX(pre, name) name


#if defined(TARGET_PLATFORM) && (defined(SWIG) || (defined(UNIMRCP_WRAPPER_CPP) && !defined(WRAPPER)))
#ifdef SWIG
%inline {
#endif
	/** @brief Target platform name, e.g. CSharp, Python, ... */
	extern "C" char const* const swig_target_platform = TARGET_PLATFORM;
	/** @brief MRCP client application name */
	extern "C" char const* const unimrcp_client_app_name = "UniMRCPClient-" TARGET_PLATFORM;
	/** @brief MRCP client log file name (if used) */
	extern "C" char const* const unimrcp_client_log_name = "unimrcpclient-" TARGET_PLATFORM;
#ifdef SWIG
}
#endif
#else  // TARGET_PLATFORM
	/** @brief Target platform name, e.g. CSharp, Python, ... */
	extern "C" WRAPPER_DECL char const* const swig_target_platform;
	/** @brief MRCP client application name */
	extern "C" WRAPPER_DECL char const* const unimrcp_client_app_name;
	/** @brief MRCP client log file name (if used) */
	extern "C" WRAPPER_DECL char const* const unimrcp_client_log_name;
#endif

/* Opaque structures */
struct apr_pool_t;                //< APR memory pool opaque C structure
struct apr_thread_mutex_t;        //< APR mutex opaque C structure
struct mrcp_client_t;             //< MRCP client opaque C structure
struct mrcp_application_t;        //< MRCP application opaque C structure
struct mrcp_app_message_t;        //< MRCP application message opaque C structure
struct mrcp_session_t;            //< MRCP session opaque C structure
struct mrcp_channel_t;            //< MRCP client channel opaque C structure
struct mrcp_session_descriptor_t; //< MRCP session descriptor opaque C structure
struct mrcp_message_t;            //< MRCP message opaque C structure
struct mrcp_generic_header_t;     //< MRCP message generic header opaque C structure
struct mrcp_synth_header_t;       //< MRCP message synthesizer header opaque C structure
struct mrcp_recog_header_t;       //< MRCP message recognizer header opaque C structure
struct mrcp_recorder_header_t;    //< MRCP message recorder header opaque C structure
struct mpf_termination_t;         //< Media termination opaque C structure
struct mpf_stream_capabilities_t; //< Media stream capabilities opaque C structure
struct mpf_audio_stream_t;        //< Audio stream opaque C structure
struct mpf_codec_t;               //< Media codec opaque C structure
struct mpf_frame_t;               //< Media frame opaque C structure
struct mpf_dtmf_generator_t;      //< DTMF generator opaque C structure
struct mpf_dtmf_detector_t;       //< DTMF detector opaque C structure

/** @brief MRCP request ID */
typedef unsigned long long UniMRCPRequestId;

/* Forward declarations */
class UniMRCPException;
class UniMRCPClient;
class UniMRCPClientSession;
class UniMRCPStreamRx;
class UniMRCPStreamTx;
class UniMRCPAudioTermination;
class UniMRCPClientChannel;
class UniMRCPMessage;


/*
 * Following types copied from various headers for SWIG interface generation.
 * Keep them up-to-date!
 * When compiling, actual headers are included.
 */

/** @brief DTMF generator/detector band */
// From /libs/mpf/include/mpf_dtmf_generator.h
// or /libs/mpf/include/mpf_dtmf_detector.h
enum UniMRCPDTMFBand {
	ENUM_MEM(DTMF_, AUTO)      = 0x0,
	ENUM_MEM(DTMF_, INBAND)    = 0x1,
	ENUM_MEM(DTMF_, OUTBAND)   = 0x2,
	ENUM_MEM(DTMF_, BAND_BOTH) = 0x1 | 0x2
};

// From /libs/mrcp/include/mrcp_types.h
/** @brief Enumeration of MRCP resource types */
enum UniMRCPResource {
	MRCP_SYNTHESIZER, /**< Synthesizer resource */
	MRCP_RECOGNIZER,  /**< Recognizer resource */
	MRCP_RECORDER     /**< Recorder resource */
};

// From /libs/mrcp/resources/include/mrcp_synth_resource
/** @brief MRCP synthesizer methods */
enum UniMRCPSynthesizerMethod {
	SYNTHESIZER_SET_PARAMS,
	SYNTHESIZER_GET_PARAMS,
	SYNTHESIZER_SPEAK,
	SYNTHESIZER_STOP,
	SYNTHESIZER_PAUSE,
	SYNTHESIZER_RESUME,
	SYNTHESIZER_BARGE_IN_OCCURRED,
	SYNTHESIZER_CONTROL,
	SYNTHESIZER_DEFINE_LEXICON
};

// From /libs/mrcp/resources/include/mrcp_recog_resource
/** @brief MRCP recognizer methods */
enum UniMRCPRecognizerMethod {
	RECOGNIZER_SET_PARAMS,
	RECOGNIZER_GET_PARAMS,
	RECOGNIZER_DEFINE_GRAMMAR,
	RECOGNIZER_RECOGNIZE,
	RECOGNIZER_GET_RESULT,
	RECOGNIZER_START_INPUT_TIMERS,
	RECOGNIZER_STOP
};

// From /libs/mrcp/resources/include/mrcp_recorder_resource
/** @brief MRCP recorder methods */
enum UniMRCPRecorderMethod {
	RECORDER_SET_PARAMS,
	RECORDER_GET_PARAMS,
	RECORDER_RECORD,
	RECORDER_STOP,
	RECORDER_START_INPUT_TIMERS
};

// From /libs/mrcp/resources/include/mrcp_synth_resource
/** @brief MRCP synthesizer events */
enum UniMRCPSynthesizerEvent {
	SYNTHESIZER_SPEECH_MARKER_EVENT,
	SYNTHESIZER_SPEAK_COMPLETE
};

// From /libs/mrcp/resources/include/mrcp_recog_resource
/** @brief MRCP recognizer events */
enum UniMRCPRecognizerEvent {
	RECOGNIZER_START_OF_INPUT_EVENT,
	RECOGNIZER_RECOGNITION_COMPLETE
};

// From /libs/mrcp/resources/include/mrcp_recorder_resource
/** @brief MRCP recorder events */
enum UniMRCPRecorderEvent {
	RECORDER_START_OF_INPUT_EVENT,
	RECORDER_RECORD_COMPLETE
};

// From /libs/apr-toolkit/include/apt_log.h
/** @brief Priority of log messages ordered from highest priority to lowest (rfc3164) */
enum UniMRCPLogPriority{
	ENUM_MEM(APT_PRIO_, EMERGENCY), /**< system is unusable */
	ENUM_MEM(APT_PRIO_, ALERT),     /**< action must be taken immediately */
	ENUM_MEM(APT_PRIO_, CRITICAL),  /**< critical condition */
	ENUM_MEM(APT_PRIO_, ERROR),     /**< error condition */
	ENUM_MEM(APT_PRIO_, WARNING),   /**< warning condition */
	ENUM_MEM(APT_PRIO_, NOTICE),    /**< normal, but significant condition */
	ENUM_MEM(APT_PRIO_, INFO),      /**< informational message */
	ENUM_MEM(APT_PRIO_, DEBUG)      /**< debug-level message */
};

// From /libs/apr-toolkit/include/apt_log.h
/** @brief Mode of log output */
enum UniMRCPLogOutput{
	ENUM_MEM(APT_LOG_, OUTPUT_NONE)     = 0x00, /**< disable logging */
	ENUM_MEM(APT_LOG_, OUTPUT_CONSOLE)  = 0x01, /**< enable console output */
	ENUM_MEM(APT_LOG_, OUTPUT_FILE)     = 0x02, /**< enable log file output */
	ENUM_MEM(APT_LOG_, OUTPUT_BOTH)     = 0x03, /**< enable log file output */
};

// From /libs/mrcp-client/include/mrcp_application.h
/** @brief Enumeration of MRCP signaling status codes */
enum UniMRCPSigStatusCode {
	ENUM_MEM(MRCP_SIG_STATUS_CODE_, SUCCESS),   /**< indicates success */
	ENUM_MEM(MRCP_SIG_STATUS_CODE_, FAILURE),   /**< request failed */
	ENUM_MEM(MRCP_SIG_STATUS_CODE_, TERMINATE), /**< request failed, session/channel/connection unexpectedly terminated */
	ENUM_MEM(MRCP_SIG_STATUS_CODE_, CANCEL)     /**< request cancelled */
};

// From /libs/mpf/include/mpf_stream_descriptor.h
/** @brief Stream directions (none, send, receive, duplex) */
enum UniMRCPStreamDirection {
	ENUM_MEM(STREAM_DIRECTION_, NONE)    = 0x0, /**< none */
	ENUM_MEM(STREAM_DIRECTION_, SEND)    = 0x1, /**< send (sink) */
	ENUM_MEM(STREAM_DIRECTION_, RECEIVE) = 0x2, /**< receive (source) */

	ENUM_MEM(STREAM_DIRECTION_, DUPLEX)  =
		0x1 | 0x2 /**< duplex */
};

// From /libs/mpf/include/mpf_codec_descriptor.h
/** @brief Codec frame time base in msec */
#define UNIMRCP_FRAME_TIME_BASE 10
/** @brief Bytes per sample for linear pcm */
#define UNIMRCP_BYTES_PER_SAMPLE 2
/** @brief Bits per sample for linear pcm */
#define UNIMRCP_BITS_PER_SAMPLE 16
/** @brief Supported sampling rates */
enum UniMRCPSampleRate {
	SAMPLE_RATE_NONE  = 0x00,
	SAMPLE_RATE_8000  = 0x01,
	SAMPLE_RATE_16000 = 0x02,
	SAMPLE_RATE_32000 = 0x04,
	SAMPLE_RATE_48000 = 0x08,
	SAMPLE_RATE_11025 = 0x10,
	SAMPLE_RATE_22050 = 0x20,
	SAMPLE_RATE_44100 = 0x40,

	SAMPLE_RATE_SUPPORTED =	0x01 | 0x02 |
	                        0x04 | 0x08 |
	                        0x10 | 0x20 |
	                        0x40
};

// From /libs/mrcp/message/include/mrcp_start_line.h
/** @brief Request-states used in MRCP response message */
enum UniMRCPRequestState {
	/** The request was processed to completion and there will be no
	    more events from that resource to the client with that request-id */
	ENUM_MEM(MRCP_REQUEST_, STATE_COMPLETE),
	/** Indicate that further event messages will be delivered with that request-id */
	ENUM_MEM(MRCP_REQUEST_, STATE_INPROGRESS),
	/** The job has been placed on a queue and will be processed in first-in-first-out order */
	ENUM_MEM(MRCP_REQUEST_, STATE_PENDING),

	/** Unknown request state */
	ENUM_MEM(MRCP_REQUEST_, STATE_UNKNOWN)
};

// From /libs/mrcp/message/include/mrcp_start_line.h
/** @brief Status codes */
enum UniMRCPStatusCode {
	ENUM_MEM(MRCP_STATUS_, CODE_UNKNOWN)                   = 0,
	/* success codes (2xx) */
	ENUM_MEM(MRCP_STATUS_, CODE_SUCCESS)                   = 200,
	ENUM_MEM(MRCP_STATUS_, CODE_SUCCESS_WITH_IGNORE)       = 201,
	/* failure codes (4xx) */
	ENUM_MEM(MRCP_STATUS_, CODE_METHOD_NOT_ALLOWED)        = 401,
	ENUM_MEM(MRCP_STATUS_, CODE_METHOD_NOT_VALID)          = 402,
	ENUM_MEM(MRCP_STATUS_, CODE_UNSUPPORTED_PARAM)         = 403,
	ENUM_MEM(MRCP_STATUS_, CODE_ILLEGAL_PARAM_VALUE)       = 404,
	ENUM_MEM(MRCP_STATUS_, CODE_NOT_FOUND)                 = 405,
	ENUM_MEM(MRCP_STATUS_, CODE_MISSING_PARAM)             = 406,
	ENUM_MEM(MRCP_STATUS_, CODE_METHOD_FAILED)             = 407,
	ENUM_MEM(MRCP_STATUS_, CODE_UNRECOGNIZED_MESSAGE)      = 408,
	ENUM_MEM(MRCP_STATUS_, CODE_UNSUPPORTED_PARAM_VALUE)   = 409,
	ENUM_MEM(MRCP_STATUS_, CODE_OUT_OF_ORDER)              = 410,
	ENUM_MEM(MRCP_STATUS_, CODE_RESOURCE_SPECIFIC_FAILURE) = 421
};

// From /libs/mrcp/message/include/mrcp_start_line.h
/** @brief MRCP message types */
enum UniMRCPMessageType {
	ENUM_MEM(MRCP_MESSAGE_TYPE_, UNKNOWN_TYPE),
	ENUM_MEM(MRCP_MESSAGE_TYPE_, REQUEST),
	ENUM_MEM(MRCP_MESSAGE_TYPE_, RESPONSE),
	ENUM_MEM(MRCP_MESSAGE_TYPE_, EVENT)
};

// From /libs/mrcp/message/include/mrcp_generic_header.h
/** @brief Enumeration of MRCP generic header fields */
enum UniMRCPGenericHeaderId {
	ENUM_MEM(HEADER_, GENERIC_ACTIVE_REQUEST_ID_LIST),
	ENUM_MEM(HEADER_, GENERIC_PROXY_SYNC_ID),
	ENUM_MEM(HEADER_, GENERIC_ACCEPT_CHARSET),
	ENUM_MEM(HEADER_, GENERIC_CONTENT_TYPE),
	ENUM_MEM(HEADER_, GENERIC_CONTENT_ID),
	ENUM_MEM(HEADER_, GENERIC_CONTENT_BASE),
	ENUM_MEM(HEADER_, GENERIC_CONTENT_ENCODING),
	ENUM_MEM(HEADER_, GENERIC_CONTENT_LOCATION),
	ENUM_MEM(HEADER_, GENERIC_CONTENT_LENGTH),
	ENUM_MEM(HEADER_, GENERIC_CACHE_CONTROL),
	ENUM_MEM(HEADER_, GENERIC_LOGGING_TAG),
	ENUM_MEM(HEADER_, GENERIC_VENDOR_SPECIFIC_PARAMS),

	/* Additional header fields for MRCP v2 */
	ENUM_MEM(HEADER_, GENERIC_ACCEPT),
	ENUM_MEM(HEADER_, GENERIC_FETCH_TIMEOUT),
	ENUM_MEM(HEADER_, GENERIC_SET_COOKIE),
	ENUM_MEM(HEADER_, GENERIC_SET_COOKIE2)
};

// From /libs/mrcp/resources/include/mrcp_recog_header.h
/** @brief MRCP recognizer header fields */
enum UniMRCPRecognizerHeaderId {
	ENUM_MEM(HEADER_, RECOGNIZER_CONFIDENCE_THRESHOLD),
	ENUM_MEM(HEADER_, RECOGNIZER_SENSITIVITY_LEVEL),
	ENUM_MEM(HEADER_, RECOGNIZER_SPEED_VS_ACCURACY),
	ENUM_MEM(HEADER_, RECOGNIZER_N_BEST_LIST_LENGTH),
	ENUM_MEM(HEADER_, RECOGNIZER_NO_INPUT_TIMEOUT),
	ENUM_MEM(HEADER_, RECOGNIZER_RECOGNITION_TIMEOUT),
	ENUM_MEM(HEADER_, RECOGNIZER_WAVEFORM_URI),
	ENUM_MEM(HEADER_, RECOGNIZER_COMPLETION_CAUSE),
	ENUM_MEM(HEADER_, RECOGNIZER_RECOGNIZER_CONTEXT_BLOCK),
	ENUM_MEM(HEADER_, RECOGNIZER_START_INPUT_TIMERS_HDR),  /* To disambiguate from method */
	ENUM_MEM(HEADER_, RECOGNIZER_SPEECH_COMPLETE_TIMEOUT),
	ENUM_MEM(HEADER_, RECOGNIZER_SPEECH_INCOMPLETE_TIMEOUT),
	ENUM_MEM(HEADER_, RECOGNIZER_DTMF_INTERDIGIT_TIMEOUT),
	ENUM_MEM(HEADER_, RECOGNIZER_DTMF_TERM_TIMEOUT),
	ENUM_MEM(HEADER_, RECOGNIZER_DTMF_TERM_CHAR),
	ENUM_MEM(HEADER_, RECOGNIZER_FAILED_URI),
	ENUM_MEM(HEADER_, RECOGNIZER_FAILED_URI_CAUSE),
	ENUM_MEM(HEADER_, RECOGNIZER_SAVE_WAVEFORM),
	ENUM_MEM(HEADER_, RECOGNIZER_NEW_AUDIO_CHANNEL),
	ENUM_MEM(HEADER_, RECOGNIZER_SPEECH_LANGUAGE),

	/* Additional header fields for MRCP v2 */
	ENUM_MEM(HEADER_, RECOGNIZER_INPUT_TYPE),
	ENUM_MEM(HEADER_, RECOGNIZER_INPUT_WAVEFORM_URI),
	ENUM_MEM(HEADER_, RECOGNIZER_COMPLETION_REASON),
	ENUM_MEM(HEADER_, RECOGNIZER_MEDIA_TYPE),
	ENUM_MEM(HEADER_, RECOGNIZER_VER_BUFFER_UTTERANCE),
	ENUM_MEM(HEADER_, RECOGNIZER_RECOGNITION_MODE),
	ENUM_MEM(HEADER_, RECOGNIZER_CANCEL_IF_QUEUE),
	ENUM_MEM(HEADER_, RECOGNIZER_HOTWORD_MAX_DURATION),
	ENUM_MEM(HEADER_, RECOGNIZER_HOTWORD_MIN_DURATION),
	ENUM_MEM(HEADER_, RECOGNIZER_INTERPRET_TEXT),
	ENUM_MEM(HEADER_, RECOGNIZER_DTMF_BUFFER_TIME),
	ENUM_MEM(HEADER_, RECOGNIZER_CLEAR_DTMF_BUFFER),
	ENUM_MEM(HEADER_, RECOGNIZER_EARLY_NO_MATCH)
};

// From /libs/mrcp/resources/include/mrcp_recog_header.h
/** @brief MRCP recognizer completion-cause  */
enum UniMRCPRecogCompletionCause {
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_SUCCESS)                 = 0,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_NO_MATCH)                = 1,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_NO_INPUT_TIMEOUT)        = 2,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_RECOGNITION_TIMEOUT)     = 3,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_GRAM_LOAD_FAILURE)       = 4,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_GRAM_COMP_FAILURE)       = 5,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_ERROR)                   = 6,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_SPEECH_TOO_EARLY)        = 7,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_TOO_MUCH_SPEECH_TIMEOUT) = 8,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_URI_FAILURE)             = 9,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_LANGUAGE_UNSUPPORTED)    = 10,

	/* Additional completion-cause for MRCP v2 */
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_CANCELLED)               = 11,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_SEMANTICS_FAILURE)       = 12,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_PARTIAL_MATCH)           = 13,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_PARTIAL_MATCH_MAXTIME)   = 14,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_NO_MATCH_MAXTIME)        = 15,
	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_GRAM_DEFINITION_FAILURE) = 16,

	ENUM_MEM(COMPLETION_, RECOGNIZER_CAUSE_UNKNOWN)                 = 17
};

// From /libs/mrcp/resources/include/mrcp_recorder_header.h
/** @brief MRCP recorder header fields */
enum UniMRCPRecorderHeaderId {
	ENUM_MEM(HEADER_, RECORDER_SENSITIVITY_LEVEL),
	ENUM_MEM(HEADER_, RECORDER_NO_INPUT_TIMEOUT),
	ENUM_MEM(HEADER_, RECORDER_COMPLETION_CAUSE),
	ENUM_MEM(HEADER_, RECORDER_COMPLETION_REASON),
	ENUM_MEM(HEADER_, RECORDER_FAILED_URI),
	ENUM_MEM(HEADER_, RECORDER_FAILED_URI_CAUSE),
	ENUM_MEM(HEADER_, RECORDER_RECORD_URI),
	ENUM_MEM(HEADER_, RECORDER_MEDIA_TYPE),
	ENUM_MEM(HEADER_, RECORDER_MAX_TIME),
	ENUM_MEM(HEADER_, RECORDER_TRIM_LENGTH),
	ENUM_MEM(HEADER_, RECORDER_FINAL_SILENCE),
	ENUM_MEM(HEADER_, RECORDER_CAPTURE_ON_SPEECH),
	ENUM_MEM(HEADER_, RECORDER_VER_BUFFER_UTTERANCE),
	ENUM_MEM(HEADER_, RECORDER_START_INPUT_TIMERS_HDR),  /* To disambiguate from method */
	ENUM_MEM(HEADER_, RECORDER_NEW_AUDIO_CHANNEL)
};

// From /libs/mrcp/resources/include/mrcp_recorder_header.h
/** @brief MRCP recorder completion-cause  */
enum UniMRCPRecorderCompletionCause {
	ENUM_MEM(COMPLETION_, RECORDER_CAUSE_SUCCESS_SILENCE)  = 0,
	ENUM_MEM(COMPLETION_, RECORDER_CAUSE_SUCCESS_MAXTIME)  = 1,
	ENUM_MEM(COMPLETION_, RECORDER_CAUSE_NO_INPUT_TIMEOUT) = 2,
	ENUM_MEM(COMPLETION_, RECORDER_CAUSE_URI_FAILURE)      = 3,
	ENUM_MEM(COMPLETION_, RECORDER_CAUSE_ERROR)            = 4,

	ENUM_MEM(COMPLETION_, RECORDER_CAUSE_UNKNOWN)          = 5
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief MRCP synthesizer header fields */
enum UniMRCPSynthesizerHeaderId {
	ENUM_MEM(HEADER_, SYNTHESIZER_JUMP_SIZE),
	ENUM_MEM(HEADER_, SYNTHESIZER_KILL_ON_BARGE_IN),
	ENUM_MEM(HEADER_, SYNTHESIZER_SPEAKER_PROFILE),
	ENUM_MEM(HEADER_, SYNTHESIZER_COMPLETION_CAUSE),
	ENUM_MEM(HEADER_, SYNTHESIZER_COMPLETION_REASON),
	ENUM_MEM(HEADER_, SYNTHESIZER_VOICE_GENDER),
	ENUM_MEM(HEADER_, SYNTHESIZER_VOICE_AGE),
	ENUM_MEM(HEADER_, SYNTHESIZER_VOICE_VARIANT),
	ENUM_MEM(HEADER_, SYNTHESIZER_VOICE_NAME),
	ENUM_MEM(HEADER_, SYNTHESIZER_PROSODY_VOLUME),
	ENUM_MEM(HEADER_, SYNTHESIZER_PROSODY_RATE),
	ENUM_MEM(HEADER_, SYNTHESIZER_SPEECH_MARKER),
	ENUM_MEM(HEADER_, SYNTHESIZER_SPEECH_LANGUAGE),
	ENUM_MEM(HEADER_, SYNTHESIZER_FETCH_HINT),
	ENUM_MEM(HEADER_, SYNTHESIZER_AUDIO_FETCH_HINT),
	ENUM_MEM(HEADER_, SYNTHESIZER_FAILED_URI),
	ENUM_MEM(HEADER_, SYNTHESIZER_FAILED_URI_CAUSE),
	ENUM_MEM(HEADER_, SYNTHESIZER_SPEAK_RESTART),
	ENUM_MEM(HEADER_, SYNTHESIZER_SPEAK_LENGTH),
	ENUM_MEM(HEADER_, SYNTHESIZER_LOAD_LEXICON),
	ENUM_MEM(HEADER_, SYNTHESIZER_LEXICON_SEARCH_ORDER)
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Synthesizer completion-cause specified in SPEAK-COMPLETE event */
enum UniMRCPSynthesizerCompletionCause {
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_NORMAL)               = 0,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_BARGE_IN)             = 1,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_PARSE_FAILURE)        = 2,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_URI_FAILURE)          = 3,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_ERROR)                = 4,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_LANGUAGE_UNSUPPORTED) = 5,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_LEXICON_LOAD_FAILURE) = 6,
	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_CANCELLED)            = 7,

	ENUM_MEM(COMPLETION_, SYNTHESIZER_CAUSE_UNKNOWN)              = 8
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief MRCP voice-gender */
enum UniMRCPVoiceGender {
	ENUM_MEM(VOICE_, GENDER_MALE),
	ENUM_MEM(VOICE_, GENDER_FEMALE),
	ENUM_MEM(VOICE_, GENDER_NEUTRAL),

	ENUM_MEM(VOICE_, GENDER_UNKNOWN)
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Speech-length types */
enum UniMRCPSpeechLengthType {
	ENUM_MEM(SPEECH_, LENGTH_TYPE_TEXT)    = 0,
	ENUM_MEM(SPEECH_, LENGTH_TYPE_NUMERIC) = 1,

	ENUM_MEM(SPEECH_, LENGTH_TYPE_UNKNOWN) = 3
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Speech-units */
enum UniMRCPSpeechUnit {
	ENUM_MEM(SPEECH_UNIT_, SECOND),
	ENUM_MEM(SPEECH_UNIT_, WORD),
	ENUM_MEM(SPEECH_UNIT_, SENTENCE),
	ENUM_MEM(SPEECH_UNIT_, PARAGRAPH),
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Prosody-rate type */
enum UniMRCPProsodyRateType {
	ENUM_MEM(PROSODY_, RATE_TYPE_LABEL),
	ENUM_MEM(PROSODY_, RATE_TYPE_RELATIVE),

	ENUM_MEM(PROSODY_, RATE_TYPE_UNKNOWN)
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Prosody-rate */
enum UniMRCPProsodyRateLabel {
	ENUM_MEM(PROSODY_, RATE_XSLOW),
	ENUM_MEM(PROSODY_, RATE_SLOW),
	ENUM_MEM(PROSODY_, RATE_MEDIUM),
	ENUM_MEM(PROSODY_, RATE_FAST),
	ENUM_MEM(PROSODY_, RATE_XFAST),
	ENUM_MEM(PROSODY_, RATE_DEFAULT),

	ENUM_MEM(PROSODY_, RATE_UNKNOWN)
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Prosody-volume type */
enum UniMRCPProsodyVolumeType {
	ENUM_MEM(PROSODY_, VOLUME_TYPE_LABEL),
	ENUM_MEM(PROSODY_, VOLUME_TYPE_NUMERIC),
	ENUM_MEM(PROSODY_, VOLUME_TYPE_RELATIVE),

	ENUM_MEM(PROSODY_, VOLUME_TYPE_UNKNOWN)
};

// From /libs/mrcp/resources/include/mrcp_synth_header.h
/** @brief Prosody-volume */
enum UniMRCPProsodyVolumeLabel {
	ENUM_MEM(PROSODY_, VOLUME_SILENT),
	ENUM_MEM(PROSODY_, VOLUME_XSOFT),
	ENUM_MEM(PROSODY_, VOLUME_SOFT),
	ENUM_MEM(PROSODY_, VOLUME_MEDIUM),
	ENUM_MEM(PROSODY_, VOLUME_LOUD),
	ENUM_MEM(PROSODY_, VOLUME_XLOUD),
	ENUM_MEM(PROSODY_, VOLUME_DEFAULT),

	ENUM_MEM(PROSODY_, VOLUME_UNKNOWN)
};


/**
 * @brief The only exception thrown directly by the wrapper.
 */
#ifdef _MSC_VER
#	pragma warning (push)
#	pragma warning (disable: 4512)
#endif
class UniMRCPException {
public:
	UniMRCPException(char const* _file, unsigned _line, char const* _msg) :
		file(_file), line(_line), msg(_msg)
	{};

	char const* const file;
	unsigned const    line;
	char const* const msg;
};
#ifdef _MSC_VER
#	pragma warning (pop)
#endif


/**
 * @brief Logger object for user-implemented logging.
 */
class UniMRCPLogger {
public:
	/** @brief Receives the messages. Must be overriden. */
	virtual bool Log(char const* file, unsigned line, UniMRCPLogPriority priority, char const* message) = 0;
	inline UniMRCPLogger() {};
	WRAPPER_DECL virtual ~UniMRCPLogger();

private:
	/** @brief Called by APT logger instance, calls Log method */
	static int LogExtHandler(char const* file, int line, char const* id, UniMRCPLogPriority priority, char const* format, va_list arg_ptr);
	/** @brief Only one logger used at a time */
	static UniMRCPLogger* logger;

	friend class UniMRCPClient;
};


/**
 * @brief UniMRCP client object and static methods to initialize the platform
 */
class UniMRCPClient {
/** @name Version getters */
/** @{ */
public:
	WRAPPER_DECL static void UniMRCPVersion(unsigned short& major, unsigned short& minor, unsigned short& patch);
	WRAPPER_DECL static void WrapperVersion(unsigned short& major, unsigned short& minor, unsigned short& patch);
	WRAPPER_DECL static void APRVersion(unsigned short& major, unsigned short& minor, unsigned short& patch);
	WRAPPER_DECL static void APRUtilVersion(unsigned short& major, unsigned short& minor, unsigned short& patch);
	WRAPPER_DECL static char const* SofiaSIPVersion();
/** @} */

public:
	/**
	 * @brief Initialize UniMRCP platform with default UniMRCP (APT) logger.
	 *
	 * Must be called first before any clients are created.
	 *
	 * @param root_dir       UniMRCP directory structure root
	 * @param log_prio       Priority of log messages to write to log
	 * @param log_out        Where to write log
	 * @param log_fname      Log file name; default unimrcpclient-PLATFORM
	 * @param max_log_fsize  Max log file size
	 * @param max_log_fcount Max log files for rotation
	 */
	WRAPPER_DECL static void StaticInitialize(char const* root_dir,
	                                          UniMRCPLogPriority log_prio = ENUM_MEM(APT_PRIO_, NOTICE),
	                                          UniMRCPLogOutput log_out = ENUM_MEM(APT_LOG_OUTPUT_, FILE),
	                                          char const* log_fname = NULL,
	                                          unsigned max_log_fsize = 8 * 1024 * 1024,
	                                          unsigned max_log_fcount = 10) THROWS(UniMRCPException);

	/**
	 * @brief Initialize UniMRCP platform with user-defined logger.
	 *
	 * Must be called first before any clients are created.
	 *
	 * @param logger   User logger object.
	 * @param log_prio Priority of messages to send to the logger.
	 */
	WRAPPER_DECL static void StaticInitialize(UniMRCPLogger* logger,
	                                          UniMRCPLogPriority log_prio = ENUM_MEM(APT_PRIO_, NOTICE)) THROWS(UniMRCPException);

	/**
	 * @brief Deinitialize UniMRCP platform. Can be called after all client instances are destroyed.
	 */
	WRAPPER_DECL static void StaticDeinitialize() THROWS(UniMRCPException);

public:
	/**
	 * @brief Create UniMRCP client instance.
	 *
	 * @param config Client framework root dir or inline XML configuration (see below)
	 * @param dir    If true, above parameter is the root dir, otherwise it is XML string
	 */
	WRAPPER_DECL UniMRCPClient(char const* config, bool dir = false) THROWS(UniMRCPException);
	/** @brief Calls Destroy if not already destroyed */
	WRAPPER_DECL ~UniMRCPClient();
	/** @brief Destroy the client immediately (blocking call) */
	WRAPPER_DECL void Destroy();

private:
	mrcp_client_t* client;   ///< The client opaque object
	mrcp_application_t* app; ///< Application opaque object
	bool terminated;         ///< Destroying
	unsigned sess_id;        ///< Sessions counter

	static unsigned instances;         ///< How many clients there are
	static unsigned staticInitialized; ///< How many times static initialized
	static apr_pool_t* staticPool;     ///< Memory pool for common things (very small)

	/** @brief Common static pre-initialization steps with possible file descriptor hack */
	static void StaticPreinitialize(int& fd_stdin, int& fd_stdout, int& fd_stderr) THROWS(UniMRCPException);
	/** @brief Common static post-initialization steps with possible file descriptor hack */
	static void StaticPostinitialize(int fd_stdin, int fd_stdout, int fd_stderr);

private:
	/** @brief Called by UniMRCP client task */
	static int AppMessageHandler(const mrcp_app_message_t* msg);

	friend class UniMRCPClientSession;
};


/**
 * @brief MRCP session. Contains media streams and signaling channels.
 */
class UniMRCPClientSession {
public:
	/** @brief Create an MRCP session using specified profile */
	WRAPPER_DECL UniMRCPClientSession(UniMRCPClient* client, char const* profile) THROWS(UniMRCPException);
	/** @brief Calls Destroy */
	WRAPPER_DECL virtual ~UniMRCPClientSession();

	WRAPPER_DECL void ResourceDiscover();
	/** @brief Terminate the session. It must still be destroyed after termination. */
	WRAPPER_DECL void Terminate();
	/** @brief Destroy and terminate the session if not terminated yet */
	WRAPPER_DECL void Destroy();
	/** @brief Get MRCP session ID */
	WRAPPER_DECL char const* GetID() const;

	/** @brief Session updated (SDP renegotiated?) */
	WRAPPER_DECL virtual bool OnUpdate(UniMRCPSigStatusCode status);
	/** @brief Session terminated gracefully */
	WRAPPER_DECL virtual bool OnTerminate(UniMRCPSigStatusCode status);
	/** @brief Session terminated unexpectedly */
	WRAPPER_DECL virtual bool OnTerminateEvent();
//	apt_bool_t (*on_resource_discover)(mrcp_application_t* application, mrcp_session_t* session, mrcp_session_descriptor_t* descriptor, mrcp_sig_status_code_e status);

private:
	static int AppOnSessionUpdate(mrcp_application_t* application, mrcp_session_t* session, UniMRCPSigStatusCode status);
	static int AppOnSessionTerminate(mrcp_application_t* application, mrcp_session_t* session, UniMRCPSigStatusCode status);
	static int AppOnTerminateEvent(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel);
	static int AppOnResourceDiscover(mrcp_application_t* application, mrcp_session_t* session, mrcp_session_descriptor_t* descriptor, UniMRCPSigStatusCode status);

private:
	mrcp_session_t* sess;    ///< Opaque C object
	UniMRCPClient* client;   ///< Owner of the session
	bool terminated;         ///< Has it been terminated
	bool destroyOnTerminate; ///< Destroy as soon as terminated

	friend class UniMRCPClient;
	friend class UniMRCPAudioTermination;
	friend class UniMRCPClientChannel;
};


/**
 * @brief Outgoing media stream (RX from the point of view of server)
 */
class UniMRCPStreamRx {
public:
	/** @brief Create in UniMRCPAudioTermination::OnStreamOpenRx() */
	WRAPPER_DECL UniMRCPStreamRx();
	WRAPPER_DECL virtual ~UniMRCPStreamRx();

	/** @brief Enqueue DTMF digit for transmission */
	WRAPPER_DECL bool SendDTMF(char digit);
	/** @brief Frame size in bytes */
	WRAPPER_DECL size_t GetDataSize() const;
	/**
	 * @brief Send data to the server (should be used in ReadFrame())
	 * @param buf Pointer to the buffer to send
	 * @param len Length of the buffer, but at most GetDataSize() bytes will be sent
	 */
	WRAPPER_DECL void SetData(void const* buf, size_t len);

	/** @brief Called when stream is being closed */
	WRAPPER_DECL virtual void OnClose();
	/** @brief Called whenever a frame is needed */
	WRAPPER_DECL virtual bool ReadFrame();

protected:
	/** @brief Initialize internal data after user-defined creation procedure */
	WRAPPER_DECL virtual bool OnOpenInternal(UniMRCPAudioTermination const* term, mpf_audio_stream_t const* stm);
	/** @brief Clean-up after OnClose() event handled */
	WRAPPER_DECL virtual void OnCloseInternal();

private:
	mpf_frame_t* frm;               ///< Single media frame
	mpf_dtmf_generator_t* dtmf_gen; ///< DTMF generator C opaque object
	UniMRCPAudioTermination* term;  ///< Owning audio termination

	friend class UniMRCPAudioTermination;
	friend class UniMRCPStreamRxBuffered;
};


/**
 * @brief Thread-safe buffered outgoing stream (RX from the point of view of server).
 *
 * User can add data any time and they are continually transmitted.
 * @see UniMRCPStreamRx
 */
class UniMRCPStreamRxBuffered : public UniMRCPStreamRx {
public:
	/** @brief Create in UniMRCPAudioTermination::OnStreamOpenRx() */
	WRAPPER_DECL UniMRCPStreamRxBuffered();
	WRAPPER_DECL virtual ~UniMRCPStreamRxBuffered();

	/** @brief Enqueue DTMF digit for transmission */
	WRAPPER_DECL bool SendDTMF(char digit);
	/** @brief Enqueue data form transmission */
	WRAPPER_DECL bool AddData(void const* buf, size_t len);
	/** @brief Send remaining data even if there is less than whole frame */
	WRAPPER_DECL void Flush();

public:
	/** @brief Automatic data transmitter. Still can be overriden! */
	virtual bool ReadFrame();

protected:
	virtual bool OnOpenInternal(UniMRCPAudioTermination const* term, mpf_audio_stream_t const* stm);
	virtual void OnCloseInternal();

private:
	/** @brief Audio or DTMF chunk in the buffer */
	struct chunk_t {
		chunk_t* next;    ///< Linked list
		char     digit;   ///< DTMF digit
		size_t   len;     ///< Chunk length
		char     data[1]; ///< Container
	};

	chunk_t* first;  ///< A chunk to be sent
	chunk_t* last;   ///< Tail of the list
	size_t   len;    ///< Total audio data length
	size_t   pos;    ///< Position in current buffer
	bool     flush;  ///< @see Flush()
	apr_thread_mutex_t* mutex;
#ifdef UW_TRACE_BUFFERS
	unsigned long rcv;
	unsigned long snt;
#endif
};


/**
 * @brief Incoming media stream (TX from the point of view of server)
 */
class UniMRCPStreamTx {
public:
	/** @brief Create in UniMRCPAudioTermination::OnStreamOpenTx() */
	WRAPPER_DECL UniMRCPStreamTx();
	WRAPPER_DECL virtual ~UniMRCPStreamTx();

	/** @brief Get DTMF received, if there is none, '\0' is returned, call in overriden WriteFrame() */
	WRAPPER_DECL char GetDTMF();
	/** @brief Check whether the frame contains audio, call in overriden WriteFrame() */
	WRAPPER_DECL bool HasAudio() const;
	/** @brief Get size of the frame in bytes, call in overriden WriteFrame() */
	WRAPPER_DECL size_t GetDataSize() const;
	/**
	 * @brief Get data contained in just received frame (should be called in WriteFrame())
	 * @param buf Buffer to put data to
	 * @param len Size of the buffer, at most MAX(len, GetDataSize()) will be copied
	 */
	WRAPPER_DECL void GetData(void* buf, size_t len);

	/** @brief The stream is being closed */
	WRAPPER_DECL virtual void OnClose();
	/** @brief Called whenever a frame arrives */
	WRAPPER_DECL virtual bool WriteFrame();

private:
	WRAPPER_DECL virtual bool OnOpenInternal(UniMRCPAudioTermination const* term, mpf_audio_stream_t const* stm);
	WRAPPER_DECL virtual void OnCloseInternal();

private:
	mpf_frame_t const* frm;        ///< Media frame last arrived
	mpf_dtmf_detector_t* dtmf_det; ///< DTMF detector C opaque structure
	UniMRCPAudioTermination* term; ///< Owner termination

	friend class UniMRCPAudioTermination;
};


/**
 * @brief Media termination. Configures codecs, streams and DTMF.
 */
class UniMRCPAudioTermination {
public:
	/** @brief Add media termination to the session */
	WRAPPER_DECL UniMRCPAudioTermination(UniMRCPClientSession* session) THROWS(UniMRCPException);
	/** @brief Destroys the termination but does not actually deallocate the streams */
	WRAPPER_DECL virtual ~UniMRCPAudioTermination();
	/** @brief Sets media stream direction(s) */
	WRAPPER_DECL void SetDirection(UniMRCPStreamDirection dir);
	/** @brief Add codec and sample rate capability to the termination */
	WRAPPER_DECL bool AddCapability(char const* codec, UniMRCPSampleRate rates);
	/** @brief Switch on DTMF generator with parameters */
	WRAPPER_DECL void EnableDTMFGenerator(UniMRCPDTMFBand band = ENUM_MEM(DTMF_, AUTO), unsigned tone_ms = 70, unsigned silence_ms = 50);
	/** @brief Switch on DTMF detector with parameters */
	WRAPPER_DECL void EnableDTMFDetector(UniMRCPDTMFBand band = ENUM_MEM(DTMF_, AUTO));

	/**
	 * @brief Stream is being opened, user should create and return stream object here
	 * @see UniMRCPStreamRx @see UniMRCPStreamRxBuffered
	 */
	WRAPPER_DECL virtual UniMRCPStreamRx* OnStreamOpenRx(bool enabled, unsigned char payload_type, char const* name,
	                                                     char const* format, unsigned char channels, unsigned freq);
	/**
	 * @brief Stream is being opened, user should create and return stream object here
	 * @see UniMRCPStreamTx
	 */
	WRAPPER_DECL virtual UniMRCPStreamTx* OnStreamOpenTx(bool enabled, unsigned char payload_type, char const* name,
	                                                     char const* format, unsigned char channels, unsigned freq);

private:
	static int StmDestroy(mpf_audio_stream_t* stream);
	static int StmOpenRx(mpf_audio_stream_t* stream, mpf_codec_t* codec);
	static int StmCloseRx(mpf_audio_stream_t* stream);
	static int StmReadFrame(mpf_audio_stream_t* stream, mpf_frame_t* frame);
	static int StmOpenTx(mpf_audio_stream_t* stream, mpf_codec_t* codec);
	static int StmCloseTx(mpf_audio_stream_t* stream);
	static int StmWriteFrame(mpf_audio_stream_t* stream, mpf_frame_t const* frame);

private:
	mpf_stream_capabilities_t* caps; ///< Capabilities structure
	mrcp_session_t* sess;            ///< Owner session
	UniMRCPStreamRx* streamRx;       ///< User outgoing stream object
	mpf_audio_stream_t *stmRx;       ///< C outgoing stream object
	UniMRCPStreamTx* streamTx;       ///< User incoming stream object
	mpf_audio_stream_t *stmTx;       ///< C incoming stream object. Caution! Might be the same as stmRx!!!
	int dg_band;                     ///< DTMF generator band
	unsigned dg_tone;                ///< DTMF generator tone length
	unsigned dg_silence;             ///< DTMF generator silence length
	int dd_band;                     ///< DTMF detector band

	friend class UniMRCPClientChannel;
	friend class UniMRCPStreamTx;
	friend class UniMRCPStreamRx;
	friend class UniMRCPStreamRxBuffered;
};


/**
 * @brief MRCP general signaling channel. Should not be used, use specialized (resource) channels instead.
 * @see UniMRCPSynthesizerChannel
 * @see UniMRCPRecognizerChannel
 * @see UniMRCPRecorderChannel
 */
class UniMRCPClientChannel {
public:
	WRAPPER_DECL virtual ~UniMRCPClientChannel();

	/** @brief Remove channel from Session */
	WRAPPER_DECL void Remove();

	/**
	 * @brief Channel added to a session and can transmit messages
	 * @param status Start sending messages here if status is MRCP_SIG_STATUS_CODE_SUCCESS
	 * @return false if you do not wish to continue the session
	 */
	WRAPPER_DECL virtual bool OnAdd(UniMRCPSigStatusCode status);
	/** @brief Channel removed from a session */
	WRAPPER_DECL virtual bool OnRemove(UniMRCPSigStatusCode status);
	/** @brief Session terminated unexpectedly */
	WRAPPER_DECL virtual bool OnTerminateEvent();

protected:
	/**
	 * @brief Create general channel and add it to a session
	 *
	 * Called (created) by specialized resource channels.
	 * @see UniMRCPSynthesizerChannel
	 * @see UniMRCPRecognizerChannel
	 * @see UniMRCPRecorderChannel
	 */
	WRAPPER_DECL UniMRCPClientChannel(UniMRCPClientSession* session, UniMRCPResource resource, UniMRCPAudioTermination* termination) THROWS(UniMRCPException);

protected:
	mrcp_session_t* sess;  ///< Owner session
	mrcp_channel_t* chan;  ///< Opaque C structure

private:
	static int AppOnChannelAdd(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, UniMRCPSigStatusCode status);
	static int AppOnChannelRemove(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, UniMRCPSigStatusCode status);
	static int AppOnMessageReceive(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, mrcp_message_t* message);

	/** Create signaling message, it will get its wrapped type later */
	WRAPPER_DECL mrcp_message_t* CreateMsg(unsigned method) THROWS(UniMRCPException);
	/** Message received, passed to resource channel to give it proper type */
	virtual bool OnMsgReceive(mrcp_message_t* message);

	friend class UniMRCPClient;
	template<UniMRCPResource resource>
	friend class UniMRCPClientResourceChannel;
};


/*
 * Many macros allowing us to specify headers that can appear at many times once.
 *
 * There are several groups of the macros:
 * - List macros which take command and argumet which is called for every header
 * - - PROCESS_rrr_HEADERS_STR    - List of string headers
 * - - PROCESS_rrr_HEADERS_BOOL   - List of boolean headers
 * - - PROCESS_rrr_HEADERS_SIMPLE - List of simple-type (numeric) headers
 * - - PROCESS_rrr_HEADERS_OTHER  - List of complex-type headers. These macros are not used, just for completeness
 * - - rrr is GENERIC, SYNTH, RECOG or RECORDER
 * - Declarators for header type ttt (ttt is STR, BOOL or SIMPLE)
 * - - ttt_HEADER_CLASS_DECL    - Declare class functions. Defined as nothing, ttt_HEADER_FUNC_CLASS_DECL or ttt_HEADER_SWIG_CLASS_DECL
 * - - ttt_HEADER_GLOBAL_DECL   - Declare helper functions. Defined as nothing or ttt_HEADER_SWIG_GLOBAL_DECL
 * - - ttt_HEADER_GLOBAL_DECL_T - Declare helper functions for template class UniMRCPResourceMessage<resource>
 * - Definers ttt_HEADER_DEF - located in .cpp file
 */

#ifdef SWIG
// Declarator versions for wrapper
#	define STR_HEADER_CLASS_DECL       STR_HEADER_SWIG_CLASS_DECL
#	define BOOL_HEADER_CLASS_DECL      BOOL_HEADER_SWIG_CLASS_DECL
#	define SIMPLE_HEADER_CLASS_DECL    SIMPLE_HEADER_SWIG_CLASS_DECL
#	define STR_HEADER_GLOBAL_DECL      STR_HEADER_SWIG_GLOBAL_DECL
#	define STR_HEADER_GLOBAL_DECL_T    STR_HEADER_SWIG_GLOBAL_DECL_T
#	define BOOL_HEADER_GLOBAL_DECL     BOOL_HEADER_SWIG_GLOBAL_DECL
#	define BOOL_HEADER_GLOBAL_DECL_T   BOOL_HEADER_SWIG_GLOBAL_DECL_T
#	define SIMPLE_HEADER_GLOBAL_DECL   SIMPLE_HEADER_SWIG_GLOBAL_DECL
#	define SIMPLE_HEADER_GLOBAL_DECL_T SIMPLE_HEADER_SWIG_GLOBAL_DECL_T
#else
// Declarator versions for compilation
/** @brief Declare string header getter and setter methods */
#	define STR_HEADER_CLASS_DECL       STR_HEADER_FUNC_CLASS_DECL
/** @brief Declare boolean header getter and setter methods */
#	define BOOL_HEADER_CLASS_DECL      BOOL_HEADER_FUNC_CLASS_DECL
/** @brief Declare simple-type (e.g. integer) header getter and setter methods */
#	define SIMPLE_HEADER_CLASS_DECL    SIMPLE_HEADER_FUNC_CLASS_DECL
/** @brief Declare and define string header getter and setter functions for SWIG wrappers */
#	define STR_HEADER_GLOBAL_DECL(name, type, prop, arg)
/** @brief Declare and define string header getter and setter functions for SWIG wrappers (templated version) */
#	define STR_HEADER_GLOBAL_DECL_T(name, type, prop, arg)
/** @brief Declare and define boolean header getter and setter functions for SWIG wrappers */
#	define BOOL_HEADER_GLOBAL_DECL(name, type, prop, arg)
/** @brief Declare and define boolean header getter and setter functions for SWIG wrappers (templated version) */
#	define BOOL_HEADER_GLOBAL_DECL_T(name, type, prop, arg)
/** @brief Declare and define simple-type (e.g. integer) header getter and setter functions for SWIG wrappers */
#	define SIMPLE_HEADER_GLOBAL_DECL(name, type, prop, arg)
/** @brief Declare and define boolean header getter and setter functions for SWIG wrappers (templated version) */
#	define SIMPLE_HEADER_GLOBAL_DECL_T(name, type, prop, arg)
#endif

/**
 * @brief Declare string header getter and setter methods
 * @see
 */
#define STR_HEADER_FUNC_CLASS_DECL(name, type, prop, arg) \
	WRAPPER_DECL char const* name ## _get() const;        \
	WRAPPER_DECL void name ## _set(char const* val);

/**
 * @brief Declare simple-type (e.g. integer) header getter and setter methods
 * @see BOOL_HEADER_DEF
 */
#define BOOL_HEADER_FUNC_CLASS_DECL(name, type, prop, arg) \
	WRAPPER_DECL bool name ## _get() const;                \
	WRAPPER_DECL void name ## _set(bool val);

/**
 * @brief Declare simple-type (e.g. integer) header getter and setter methods
 * @see SIMPLE_HEADER_DEF
 */
#define SIMPLE_HEADER_FUNC_CLASS_DECL(name, type, prop, arg) \
	WRAPPER_DECL type name ## _get() const;                  \
	WRAPPER_DECL void name ## _set(type val);

#ifdef SWIG

#define STR_HEADER_SWIG_CLASS_DECL(name, type, prop, arg) \
	public:                                               \
	%extend {                                             \
		char const* name;                                 \
	}                                                     \
	private:                                              \
		STR_HEADER_FUNC_CLASS_DECL(name, type, prop, arg)

#define BOOL_HEADER_SWIG_CLASS_DECL(name, type, prop, arg) \
	public:                                                \
	%extend {                                              \
		bool name;                                         \
	}                                                      \
	private:                                               \
		BOOL_HEADER_FUNC_CLASS_DECL(name, type, prop, arg)

#define SIMPLE_HEADER_SWIG_CLASS_DECL(name, type, prop, arg) \
	public:                                                  \
	%extend {                                                \
		type name;                                           \
	}                                                        \
	private:                                                 \
		SIMPLE_HEADER_FUNC_CLASS_DECL(name, type, prop, arg)

#define STR_HEADER_SWIG_GLOBAL_DECL(name, type, prop, class)           \
	%{                                                                 \
		static inline char const* class ## _ ## name ## _get(class const* obj) { \
			return obj->name ## _get();                                \
		}                                                              \
		static inline void class ## _ ## name ## _set(class* obj, char const* val) { \
			obj->name ## _set(val);                                    \
		}                                                              \
	%}

#define STR_HEADER_SWIG_GLOBAL_DECL_T(name, type, prop, class)         \
	%{                                                                 \
		static inline char const* UniMRCPResourceMessage_Sl_ ## class ## _Sg__ ## name ## _get(UniMRCPResourceMessage<class> const* obj) { \
			return obj->name ## _get();                                \
		}                                                              \
		static inline void UniMRCPResourceMessage_Sl_ ## class ## _Sg__ ## name ## _set(UniMRCPResourceMessage<class>* obj, char const* val) { \
			obj->name ## _set(val);                                    \
		}                                                              \
	%}

#define BOOL_HEADER_SWIG_GLOBAL_DECL(name, type, prop, class)   \
	%{                                                          \
		static inline bool class ## _ ## name ## _get(class const* obj) { \
			return obj->name ## _get();                         \
		}                                                       \
		static inline void class ## _ ## name ## _set(class* obj, bool val) { \
			obj->name ## _set(val);                             \
		}                                                       \
	%}

#define BOOL_HEADER_SWIG_GLOBAL_DECL_T(name, type, prop, class) \
	%{                                                          \
		static inline bool UniMRCPResourceMessage_Sl_ ## class ## _Sg__ ## name ## _get(UniMRCPResourceMessage<class> const* obj) { \
			return obj->name ## _get();                         \
		}                                                       \
		static inline void UniMRCPResourceMessage_Sl_ ## class ## _Sg__ ## name ## _set(UniMRCPResourceMessage<class>* obj, bool val) { \
			obj->name ## _set(val);                             \
		}                                                       \
	%}

#define SIMPLE_HEADER_SWIG_GLOBAL_DECL(name, type, prop, class) \
	%{                                                          \
		static inline type class ## _ ## name ## _get(class const* obj) { \
			return obj->name ## _get();                         \
		}                                                       \
		static inline void class ## _ ## name ## _set(class* obj, type val) { \
			obj->name ## _set(val);                             \
		}                                                       \
	%}

#define SIMPLE_HEADER_SWIG_GLOBAL_DECL_T(name, type, prop, class) \
	%{                                                            \
		static inline type UniMRCPResourceMessage_Sl_ ## class ## _Sg__ ## name ## _get(UniMRCPResourceMessage<class> const* obj) { \
			return obj->name ## _get();                           \
		}                                                         \
		static inline void UniMRCPResourceMessage_Sl_ ## class ## _Sg__ ## name ## _set(UniMRCPResourceMessage<class>* obj, type val) { \
			obj->name ## _set(val);                               \
		}                                                         \
	%}

#endif  // ifdef SWIG

/// @brief Process generic string headers with command cmd along with argument arg
/// @param cmd can be #STR_HEADER_CLASS_DECL, #STR_HEADER_GLOBAL_DECL, #STR_HEADER_DEF
#define PROCESS_GENERIC_HEADERS_STR(cmd, arg)                                  \
	cmd(proxy_sync_id,    apt_string_t, HEADER_GENERIC_PROXY_SYNC_ID, arg)     \
	cmd(accept_charset,   apt_string_t, HEADER_GENERIC_ACCEPT_CHARSET, arg)    \
	cmd(content_type,     apt_string_t, HEADER_GENERIC_CONTENT_TYPE, arg)      \
	cmd(content_id,       apt_string_t, HEADER_GENERIC_CONTENT_ID, arg)        \
	cmd(content_base,     apt_string_t, HEADER_GENERIC_CONTENT_BASE, arg)      \
	cmd(content_encoding, apt_string_t, HEADER_GENERIC_CONTENT_ENCODING, arg)  \
	cmd(content_location, apt_string_t, HEADER_GENERIC_CONTENT_LOCATION, arg)  \
	cmd(cache_control,    apt_string_t, HEADER_GENERIC_CACHE_CONTROL, arg)     \
	cmd(logging_tag,      apt_string_t, HEADER_GENERIC_LOGGING_TAG, arg)       \
	cmd(accept,           apt_string_t, HEADER_GENERIC_ACCEPT, arg)            \
	cmd(set_cookie,       apt_string_t, HEADER_GENERIC_SET_COOKIE, arg)        \
	cmd(set_cookie2,      apt_string_t, HEADER_GENERIC_SET_COOKIE2, arg)
/// @brief Process generic simple-type headers with command cmd along with argument arg
/// @param cmd can be #SIMPLE_HEADER_CLASS_DECL, #SIMPLE_HEADER_GLOBAL_DECL, #SIMPLE_HEADER_DEF
#define PROCESS_GENERIC_HEADERS_SIMPLE(cmd, arg)                    \
	cmd(content_length, size_t, HEADER_GENERIC_CONTENT_LENGTH, arg) \
	cmd(fetch_timeout,  size_t, HEADER_GENERIC_FETCH_TIMEOUT, arg)
/// @brief Generic headers with complex types. Never processed automatically, just for the sake of completeness
#define PROCESS_GENERIC_HEADERS_OTHER(cmd, arg)                                                     \
	cmd(active_request_id_list, mrcp_request_id_list_t, HEADER_GENERIC_ACTIVE_REQUEST_ID_LIST, arg) \
	cmd(vendor_specific_params, apt_pair_arr_t,         HEADER_GENERIC_VENDOR_SPECIFIC_PARAMS, arg)


/**
 * @brief General MRCP message with generic headers.
 * Should not be used, use specialized (resource) messages instead.
 *
 * Create messages with UniMRCPClientResourceChannel::CreateMessage().
 * They are created automatically upon calling UniMRCPClientResourceChannel::OnMessageReceive().
 * @see UniMRCPSynthesizerMessage
 * @see UniMRCPRecognizerMessage
 * @see UniMRCPRecorderMessage
 */
class UniMRCPMessage {
public:
	/// @brief Message type (request, response or event)
	WRAPPER_DECL UniMRCPMessageType GetMsgType() const;
	/// @brief MRCP method name
	WRAPPER_DECL char const* GetMethodName() const;
	/// @brief MRCP status code
	WRAPPER_DECL UniMRCPStatusCode GetStatusCode() const;
	/// @brief MRCP request state
	WRAPPER_DECL UniMRCPRequestState GetRequestState() const;

	/// @brief Get body as unprocessed string. Unsuitable for binary data.
	WRAPPER_DECL char const* GetBody() const;
	/// @brief Get binary body
	/// @param buf Buffer with size at least len
	/// @param len Maximum bytes to copy
	/// @see content_length_get
	WRAPPER_DECL void GetBody(void* buf, size_t len) const;
	/// @brief Set simple string body
	WRAPPER_DECL void SetBody(char const* body);
	/// @brief Set simple string body with specified length
	WRAPPER_DECL void SetBody(char const* body, size_t len);
	/// @brief Set binary body
	WRAPPER_DECL void SetBody(void const* buf, size_t len);

	/// @brief Send the message through owner channel
	WRAPPER_DECL bool Send();

public:
	/// @brief Add (render to string) header to the message immediately.
	/// If autoAddProperty not specified in construction, you must call this after filling header's value
	/// (before sending)
	WRAPPER_DECL void AddProperty(UniMRCPGenericHeaderId prop);
	/// @brief Add header to the message, but render it upon sending
	WRAPPER_DECL void LazyAddProperty(UniMRCPGenericHeaderId prop);
	/// @brief Add empty header to the message (useful for e.g. GET-PARAMS)
	WRAPPER_DECL void AddPropertyName(UniMRCPGenericHeaderId prop);
	/// @brief Does the message contain a property?
	WRAPPER_DECL bool CheckProperty(UniMRCPGenericHeaderId prop) const;
	/// @brief Remove header from the message
	WRAPPER_DECL void RemoveProperty(UniMRCPGenericHeaderId prop);

/** @name MRCP generic headers getters and setters */
/** @{ */
public:
	/* Generic string headers declaration */
	PROCESS_GENERIC_HEADERS_STR(STR_HEADER_CLASS_DECL, 0)
public:
	/* Generic simple-type headers declaration */
	PROCESS_GENERIC_HEADERS_SIMPLE(SIMPLE_HEADER_CLASS_DECL, 0)
/** @name Complex-type generic header manipulators */
/** @{ */
public:
	/// @brief Vendor-Specific-Parameters count
	WRAPPER_DECL unsigned VendorParamCount() const;
	/// @brief Append a Vendor-Specific-Parameter
	WRAPPER_DECL void VendorParamAppend(char const* name, char const* value);
	/// @brief Get i-th Vendor-Specific-Parameter name
	WRAPPER_DECL char const* VendorParamGetName(unsigned i) const;
	/// @brief Get i-th Vendor-Specific-Parameter value
	WRAPPER_DECL char const* VendorParamGetValue(unsigned i) const;
	/// @brief Get value of Vendor-Specific-Parameter with specified name
	WRAPPER_DECL char const* VendorParamFind(char const* name) const;
	/// @brief Get Active-Request-Id-List capacity
	WRAPPER_DECL unsigned ActiveRequestIdListMaxSize() const;
	/// @brief Get Active-Request-Id-List length
	WRAPPER_DECL unsigned ActiveRequestIdListSizeGet() const;
	/// @brief Set Active-Request-Id-List length
	WRAPPER_DECL void ActiveRequestIdListSizeSet(unsigned size);
	/// @brief Get Active-Request-Id at specified index
	WRAPPER_DECL UniMRCPRequestId ActiveRequestIdGet(unsigned index) const;
	/// @brief Set Active-Request-Id at specified index
	WRAPPER_DECL void ActiveRequestIdSet(unsigned index, UniMRCPRequestId request) THROWS(UniMRCPException);
/** @} */
/** @} */

protected:
	/* Resource headers management. Will be retyped in specialized messges. */
	WRAPPER_DECL unsigned GetMethodID() const THROWS(UniMRCPException);
	WRAPPER_DECL unsigned GetEventID() const THROWS(UniMRCPException);
	WRAPPER_DECL void AddProperty(unsigned prop);
	WRAPPER_DECL void LazyAddProperty(unsigned prop);
	WRAPPER_DECL void AddPropertyName(unsigned prop);
	WRAPPER_DECL bool CheckProperty(unsigned prop) const;
	WRAPPER_DECL void RemoveProperty(unsigned prop);

protected:
	bool AutoAddProperty;  ///< Automatically add headers when changed and render upon sending
	mrcp_message_t* msg;   ///< Message C structure

private:
	/// Called from UniMRCPChannel::CreateMessage or UniMRCPChannel::OnMessageReceive alternatives
	UniMRCPMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException);

private:
	mrcp_session_t* sess;       ///< Owner session (for memory pool)
	mrcp_channel_t* chan;       ///< Owner channel (for sending)
	mrcp_generic_header_t* hdr; ///< Generic header C structure
	char generic_props[8];      ///< Generic headers to render upon sending if AutoAddProperty
	char resource_props[8];     ///< Resource headers to render upon sending if AutoAddProperty

	friend class UniMRCPClientChannel;
	template<typename prop_t, typename method_t, typename event_t>
	friend class UniMRCPResourceMessageBase;
};

#ifndef DOXYGEN
/* Generic headers wrapper functions */
PROCESS_GENERIC_HEADERS_STR(STR_HEADER_GLOBAL_DECL, UniMRCPMessage)
PROCESS_GENERIC_HEADERS_SIMPLE(SIMPLE_HEADER_GLOBAL_DECL, UniMRCPMessage)
#endif


/**
 * @brief Properties common to all resource messages (but some types can differ, therefore the template).
 * Should not be used, use specialized (resource) messages instead.
 *
 * Create messages with UniMRCPClientResourceChannel::CreateMessage().
 * They are created automatically upon calling UniMRCPClientResourceChannel::OnMessageReceive().
 * @param prop_t   Property (header) ID enum
 *                 (#UniMRCPSynthesizerHeaderId, #UniMRCPRecognizerHeaderId, #UniMRCPRecorderHeaderId)
 * @param method_t Method ID enum
 *                 (#UniMRCPSynthesizerMethod, #UniMRCPRecognizerMethod, #UniMRCPRecorderMethod)
 * @param event_t  Event ID enum
 *                 (#UniMRCPSynthesizerEvent, #UniMRCPRecognizerEvent, #UniMRCPRecorderEvent)
 * @see UniMRCPSynthesizerMessage
 * @see UniMRCPRecognizerMessage
 * @see UniMRCPRecorderMessage
 */
template<typename prop_t, typename method_t, typename event_t>
class UniMRCPResourceMessageBase : public UniMRCPMessage {
public:
	typedef method_t Method;  ///< Accessible method enum
	typedef event_t Event;    ///< Accessible event enum

public:
	/// Get properly typed MRCP resource method ID
	inline method_t GetMethodID() const THROWS(UniMRCPException)
	{
		return static_cast<method_t>(UniMRCPMessage::GetMethodID());
	}

	/// Get properly typed MRCP resource event ID
	inline event_t GetEventID() const THROWS(UniMRCPException)
	{
		return static_cast<event_t>(UniMRCPMessage::GetEventID());
	}

	/// Add properly typed resource property (header)
	inline void AddProperty(prop_t prop)
	{
		UniMRCPMessage::AddProperty(prop);
	}

	/// Add properly typed resource property (header) for lazy rendering
	inline void LazyAddProperty(prop_t prop)
	{
		UniMRCPMessage::LazyAddProperty(prop);
	}

	/// Add properly typed resource property (header) name
	inline void AddPropertyName(prop_t prop)
	{
		UniMRCPMessage::AddPropertyName(prop);
	}

	/// Check presence of properly typed resource property (header)
	inline bool CheckProperty(prop_t prop) const
	{
		return UniMRCPMessage::CheckProperty(prop);
	}

	/// Add properly typed resource property (header)
	inline void RemoveProperty(prop_t prop)
	{
		UniMRCPMessage::RemoveProperty(prop);
	}

private:
	UniMRCPResourceMessageBase(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException) :
		UniMRCPMessage(sess, chan, msg, autoAddProperty)
	{
	}

	template<UniMRCPResource resource>
	friend class UniMRCPResourceMessage;
};


/* Forward declaration */
template<UniMRCPResource resource>
class UniMRCPResourceMessage;


/**
 * @brief Encapsulation of speech length variants -- number and unit (SYNTHESIZER)
 */
class UniMRCPNumericSpeechLength {
public:
	long              length;  ///< Length number
	UniMRCPSpeechUnit unit;    ///< Length unit
private:
	UniMRCPNumericSpeechLength(UniMRCPSpeechUnit _unit, size_t _len, bool negative);
	friend class UniMRCPResourceMessage<MRCP_SYNTHESIZER>;
};

/**
 * @brief Helper macro to easily repeat speech length definition/declaration many times
 * @see PROCESS_SYNTH_HEADERS_SPLEN
 * @see SPLEN_HEADER_DEF
 */
#define SPLEN_HEADER_FUNC_CLASS_DECL(name, NaMe, prop, class)                    \
	WRAPPER_DECL UniMRCPSpeechLengthType NaMe ## TypeGet() const;                \
	WRAPPER_DECL UniMRCPNumericSpeechLength const* NaMe ## NumericGet() const;   \
	WRAPPER_DECL char const* NaMe ## TextGet() const;                            \
	WRAPPER_DECL void NaMe ## NumericSet(long length, UniMRCPSpeechUnit unit);   \
	WRAPPER_DECL void NaMe ## NumericSet(UniMRCPNumericSpeechLength const& nsl); \
	WRAPPER_DECL void NaMe ## TextSet(char const* tag);


/// @brief Process synthesizer string headers with command cmd along with argument arg
/// @param cmd can be #STR_HEADER_CLASS_DECL, #STR_HEADER_GLOBAL_DECL_T, #STR_HEADER_DEF
#define PROCESS_SYNTH_HEADERS_STR(cmd, arg)                                            \
	cmd(speaker_profile,      apt_str_t, HEADER_SYNTHESIZER_SPEAKER_PROFILE, arg)      \
	cmd(completion_reason,    apt_str_t, HEADER_SYNTHESIZER_COMPLETION_REASON, arg)    \
	cmd(speech_marker,        apt_str_t, HEADER_SYNTHESIZER_SPEECH_MARKER, arg)        \
	cmd(speech_language,      apt_str_t, HEADER_SYNTHESIZER_SPEECH_LANGUAGE, arg)      \
	cmd(fetch_hint,           apt_str_t, HEADER_SYNTHESIZER_FETCH_HINT, arg)           \
	cmd(audio_fetch_hint,     apt_str_t, HEADER_SYNTHESIZER_AUDIO_FETCH_HINT, arg)     \
	cmd(failed_uri,           apt_str_t, HEADER_SYNTHESIZER_FAILED_URI, arg)           \
	cmd(failed_uri_cause,     apt_str_t, HEADER_SYNTHESIZER_FAILED_URI_CAUSE, arg)     \
	cmd(lexicon_search_order, apt_str_t, HEADER_SYNTHESIZER_LEXICON_SEARCH_ORDER, arg) \
	cmd(voice_name,           apt_str_t, HEADER_SYNTHESIZER_VOICE_NAME, arg)  // Defined differently
/// @brief Process synthesizer boolean headers with command cmd along with argument arg
/// @param cmd can be #BOOL_HEADER_CLASS_DECL, #BOOL_HEADER_GLOBAL_DECL_T, #BOOL_HEADER_DEF
#define PROCESS_SYNTH_HEADERS_BOOL(cmd, arg)                                   \
	cmd(speak_restart,    apt_bool_t, HEADER_SYNTHESIZER_SPEAK_RESTART, arg)   \
	cmd(load_lexicon,     apt_bool_t, HEADER_SYNTHESIZER_LOAD_LEXICON, arg)    \
	cmd(kill_on_barge_in, apt_bool_t, HEADER_SYNTHESIZER_KILL_ON_BARGE_IN, arg)
/// @brief Process synthesizer simple-type headers with command cmd along with argument arg
/// @param cmd can be #SIMPLE_HEADER_CLASS_DECL, #SIMPLE_HEADER_GLOBAL_DECL_T, #SIMPLE_HEADER_DEF
#define PROCESS_SYNTH_HEADERS_SIMPLE(cmd, arg)                                \
	cmd(completion_cause, 	UniMRCPSynthesizerCompletionCause, HEADER_SYNTHESIZER_COMPLETION_CAUSE, arg) \
	/* Defined diferently */                                                  \
	cmd(voice_age,     size_t,         HEADER_SYNTHESIZER_VOICE_AGE, arg)     \
	cmd(voice_variant, size_t,         HEADER_SYNTHESIZER_VOICE_VARIANT, arg) \
	cmd(voice_gender,  UniMRCPVoiceGender, HEADER_SYNTHESIZER_VOICE_GENDER, arg)
/// @brief Process synthesizer speech-length headers with command cmd along with argument arg
/// @param cmd can be #SPLEN_HEADER_FUNC_CLASS_DECL, #SPLEN_HEADER_DEF
#define PROCESS_SYNTH_HEADERS_SPLEN(cmd, arg)                            \
	cmd(jump_size,     JumpSize,    HEADER_SYNTHESIZER_JUMP_SIZE, arg)   \
	cmd(speak_length,  SpeakLength, HEADER_SYNTHESIZER_SPEAK_LENGTH, arg)
/// @brief Synthesizer headers with complex types. Never processed automatically, just for the sake of completeness
#define PROCESS_SYNTH_HEADERS_OTHER(cmd, arg)                                        \
	cmd(prosody_param, mrcp_prosody_param_t, HEADER_SYNTHESIZER_PROSODY_VOLUME, arg) \
	cmd(prosody_param, mrcp_prosody_param_t, HEADER_SYNTHESIZER_PROSODY_RATE, arg)

/**
 * @brief Synthesizer resource message with headers
 *
 * Create messages with UniMRCPClientResourceChannel<MRCP_SYNTHESIZER>::CreateMessage().
 * They are created automatically upon calling UniMRCPClientResourceChannel<MRCP_SYNTHESIZER>::OnMessageReceive().
 * @see UniMRCPSynthesizerMessage
 * @see UniMRCPSynthesizerChannel
 */
template<>
class UniMRCPResourceMessage<MRCP_SYNTHESIZER> :
	public UniMRCPResourceMessageBase<UniMRCPSynthesizerHeaderId, UniMRCPSynthesizerMethod, UniMRCPSynthesizerEvent>
{
/** @name MRCP synthesizer headers getters and setters */
/** @{ */
public:
	PROCESS_SYNTH_HEADERS_STR(STR_HEADER_CLASS_DECL, UniMRCPSynthHeader)
public:
	PROCESS_SYNTH_HEADERS_BOOL(BOOL_HEADER_CLASS_DECL, UniMRCPSynthHeader)
public:
	PROCESS_SYNTH_HEADERS_SIMPLE(SIMPLE_HEADER_CLASS_DECL, UniMRCPSynthHeader)
public:
	PROCESS_SYNTH_HEADERS_SPLEN(SPLEN_HEADER_FUNC_CLASS_DECL, UniMRCPSynthHeader)
public:
	WRAPPER_DECL UniMRCPProsodyRateType ProsodyRateTypeGet() const;
	WRAPPER_DECL UniMRCPProsodyRateLabel ProsodyRateLabelGet() const;
	WRAPPER_DECL float ProsodyRateRelativeGet() const;
	WRAPPER_DECL void ProsodyRateLabelSet(UniMRCPProsodyRateLabel label);
	WRAPPER_DECL void ProsodyRateRelativeSet(float rate);
public:
	WRAPPER_DECL UniMRCPProsodyVolumeType ProsodyVolumeTypeGet() const;
	WRAPPER_DECL UniMRCPProsodyVolumeLabel ProsodyVolumeLabelGet() const;
	WRAPPER_DECL float ProsodyVolumeNumericGet() const;
	WRAPPER_DECL float ProsodyVolumeRelativeGet() const;
	WRAPPER_DECL void ProsodyVolumeLabelSet(UniMRCPProsodyVolumeLabel vol);
	WRAPPER_DECL void ProsodyVolumeNumericSet(float absvol);
	WRAPPER_DECL void ProsodyVolumeRelativeSet(float relvol);
/** @} */
private:
	mrcp_synth_header_t *hdr;  ///< Opaque synthesizer header C structure

	WRAPPER_DECL UniMRCPResourceMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException);

	template<UniMRCPResource resource>
	friend class UniMRCPClientResourceChannel;
};

#ifndef DOXYGEN
PROCESS_SYNTH_HEADERS_STR(STR_HEADER_GLOBAL_DECL_T, MRCP_SYNTHESIZER)
PROCESS_SYNTH_HEADERS_BOOL(BOOL_HEADER_GLOBAL_DECL_T, MRCP_SYNTHESIZER)
PROCESS_SYNTH_HEADERS_SIMPLE(SIMPLE_HEADER_GLOBAL_DECL_T, MRCP_SYNTHESIZER)
#endif

/** @brief Shorthand for synthesizer resource message */
typedef UniMRCPResourceMessage<MRCP_SYNTHESIZER> UniMRCPSynthesizerMessage;



/// @brief Process recognizer string headers with command cmd along with argument arg
/// @param cmd can be #STR_HEADER_CLASS_DECL, #STR_HEADER_GLOBAL_DECL_T, #STR_HEADER_DEF
#define PROCESS_RECOG_HEADERS_STR(cmd, arg)                                                   \
	cmd(recognizer_context_block, apt_str_t, HEADER_RECOGNIZER_RECOGNIZER_CONTEXT_BLOCK, arg) \
	cmd(failed_uri,               apt_str_t, HEADER_RECOGNIZER_FAILED_URI, arg)               \
	cmd(failed_uri_cause,         apt_str_t, HEADER_RECOGNIZER_FAILED_URI_CAUSE, arg)         \
	cmd(speech_language,          apt_str_t, HEADER_RECOGNIZER_SPEECH_LANGUAGE, arg)          \
	cmd(input_type,               apt_str_t, HEADER_RECOGNIZER_INPUT_TYPE, arg)               \
	cmd(waveform_uri,             apt_str_t, HEADER_RECOGNIZER_WAVEFORM_URI, arg)             \
	cmd(input_waveform_uri,       apt_str_t, HEADER_RECOGNIZER_INPUT_WAVEFORM_URI, arg)       \
	cmd(completion_reason,        apt_str_t, HEADER_RECOGNIZER_COMPLETION_REASON, arg)        \
	cmd(media_type,               apt_str_t, HEADER_RECOGNIZER_MEDIA_TYPE, arg)               \
	cmd(recognition_mode,         apt_str_t, HEADER_RECOGNIZER_RECOGNITION_MODE, arg)         \
	cmd(interpret_text,           apt_str_t, HEADER_RECOGNIZER_INTERPRET_TEXT, arg)
/// @brief Process recognizer boolean headers with command cmd along with argument arg
/// @param cmd can be #BOOL_HEADER_CLASS_DECL, #BOOL_HEADER_GLOBAL_DECL_T, #BOOL_HEADER_DEF
#define PROCESS_RECOG_HEADERS_BOOL(cmd, arg)                                           \
	cmd(start_input_timers,   apt_bool_t, HEADER_RECOGNIZER_START_INPUT_TIMERS_HDR, arg)   \
	cmd(save_waveform,        apt_bool_t, HEADER_RECOGNIZER_SAVE_WAVEFORM, arg)        \
	cmd(new_audio_channel,    apt_bool_t, HEADER_RECOGNIZER_NEW_AUDIO_CHANNEL, arg)    \
	cmd(ver_buffer_utterance, apt_bool_t, HEADER_RECOGNIZER_VER_BUFFER_UTTERANCE, arg) \
	cmd(cancel_if_queue,      apt_bool_t, HEADER_RECOGNIZER_CANCEL_IF_QUEUE, arg)      \
	cmd(clear_dtmf_buffer,    apt_bool_t, HEADER_RECOGNIZER_CLEAR_DTMF_BUFFER, arg)    \
	cmd(early_no_match,       apt_bool_t, HEADER_RECOGNIZER_EARLY_NO_MATCH, arg)
/// @brief Process recognizer simple-type headers with command cmd along with argument arg
/// @param cmd can be #SIMPLE_HEADER_CLASS_DECL, #SIMPLE_HEADER_GLOBAL_DECL_T, #SIMPLE_HEADER_DEF
#define PROCESS_RECOG_HEADERS_SIMPLE(cmd, arg)                                               \
	cmd(confidence_threshold,      float,  HEADER_RECOGNIZER_CONFIDENCE_THRESHOLD, arg)      \
	cmd(sensitivity_level,         float,  HEADER_RECOGNIZER_SENSITIVITY_LEVEL, arg)         \
	cmd(speed_vs_accuracy,         float,  HEADER_RECOGNIZER_SPEED_VS_ACCURACY, arg)         \
	cmd(n_best_list_length,        size_t, HEADER_RECOGNIZER_N_BEST_LIST_LENGTH, arg)        \
	cmd(no_input_timeout,          size_t, HEADER_RECOGNIZER_NO_INPUT_TIMEOUT, arg)          \
	cmd(recognition_timeout,       size_t, HEADER_RECOGNIZER_RECOGNITION_TIMEOUT, arg)       \
	cmd(completion_cause,          UniMRCPRecogCompletionCause, HEADER_RECOGNIZER_COMPLETION_CAUSE, arg) \
	cmd(speech_complete_timeout,   size_t, HEADER_RECOGNIZER_SPEECH_COMPLETE_TIMEOUT, arg)   \
	cmd(speech_incomplete_timeout, size_t, HEADER_RECOGNIZER_SPEECH_INCOMPLETE_TIMEOUT, arg) \
	cmd(dtmf_interdigit_timeout,   size_t, HEADER_RECOGNIZER_DTMF_INTERDIGIT_TIMEOUT, arg)   \
	cmd(dtmf_term_timeout,         size_t, HEADER_RECOGNIZER_DTMF_TERM_TIMEOUT, arg)         \
	cmd(dtmf_term_char,            char,   HEADER_RECOGNIZER_DTMF_TERM_CHAR, arg)            \
	cmd(hotword_max_duration,      size_t, HEADER_RECOGNIZER_HOTWORD_MAX_DURATION, arg)      \
	cmd(hotword_min_duration,      size_t, HEADER_RECOGNIZER_HOTWORD_MIN_DURATION, arg)      \
	cmd(dtmf_buffer_time,          size_t, HEADER_RECOGNIZER_DTMF_BUFFER_TIME, arg)          \

/**
 * @brief Recognizer resource message with headers
 *
 * Create messages with UniMRCPClientResourceChannel<MRCP_RECOGNIZER>::CreateMessage().
 * They are created automatically upon calling UniMRCPClientResourceChannel<MRCP_RECOGNIZER>::OnMessageReceive().
 * @see UniMRCPRecognizerMessage
 * @see UniMRCPRecognizerChannel
 */
template<>
class UniMRCPResourceMessage<MRCP_RECOGNIZER> :
	public UniMRCPResourceMessageBase<UniMRCPRecognizerHeaderId, UniMRCPRecognizerMethod, UniMRCPRecognizerEvent>
{
/** @name MRCP recognizer headers getters and setters */
/** @{ */
public:
	PROCESS_RECOG_HEADERS_STR(STR_HEADER_CLASS_DECL, UniMRCPRecogHeader)
public:
	PROCESS_RECOG_HEADERS_BOOL(BOOL_HEADER_CLASS_DECL, UniMRCPRecogHeader)
public:
	PROCESS_RECOG_HEADERS_SIMPLE(SIMPLE_HEADER_CLASS_DECL, UniMRCPRecogHeader)
/** @} */
private:
	mrcp_recog_header_t* hdr;  ///< Recognizer header opaque C structure

	UniMRCPResourceMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException);

	template<UniMRCPResource resource>
	friend class UniMRCPClientResourceChannel;
};

#ifndef DOXYGEN
PROCESS_RECOG_HEADERS_STR(STR_HEADER_GLOBAL_DECL_T, MRCP_RECOGNIZER)
PROCESS_RECOG_HEADERS_BOOL(BOOL_HEADER_GLOBAL_DECL_T, MRCP_RECOGNIZER)
PROCESS_RECOG_HEADERS_SIMPLE(SIMPLE_HEADER_GLOBAL_DECL_T, MRCP_RECOGNIZER)
#endif

/** @brief Shorthand for recognizer resource message */
typedef UniMRCPResourceMessage<MRCP_RECOGNIZER> UniMRCPRecognizerMessage;



/// @brief Process recorder string headers with command cmd along with argument arg
/// @param cmd can be #STR_HEADER_CLASS_DECL, #STR_HEADER_GLOBAL_DECL_T, #STR_HEADER_DEF
#define PROCESS_RECORDER_HEADERS_STR(cmd, arg)                                \
	cmd(completion_reason, apt_str_t, HEADER_RECORDER_COMPLETION_REASON, arg) \
	cmd(failed_uri,        apt_str_t, HEADER_RECORDER_FAILED_URI, arg)        \
	cmd(failed_uri_cause,  apt_str_t, HEADER_RECORDER_FAILED_URI_CAUSE, arg)  \
	cmd(record_uri,        apt_str_t, HEADER_RECORDER_RECORD_URI, arg)        \
	cmd(media_type,        apt_str_t, HEADER_RECORDER_MEDIA_TYPE, arg)
/// @brief Process recorder boolean headers with command cmd along with argument arg
/// @param cmd can be #BOOL_HEADER_CLASS_DECL, #BOOL_HEADER_GLOBAL_DECL_T, #BOOL_HEADER_DEF
#define PROCESS_RECORDER_HEADERS_BOOL(cmd, arg)                                      \
	cmd(capture_on_speech,    apt_bool_t, HEADER_RECORDER_CAPTURE_ON_SPEECH, arg)    \
	cmd(ver_buffer_utterance, apt_bool_t, HEADER_RECORDER_VER_BUFFER_UTTERANCE, arg) \
	cmd(start_input_timers,   apt_bool_t, HEADER_RECORDER_START_INPUT_TIMERS_HDR, arg)   \
	cmd(new_audio_channel,    apt_bool_t, HEADER_RECORDER_NEW_AUDIO_CHANNEL, arg)
/// @brief Process recorder simple-type headers with command cmd along with argument arg
/// @param cmd can be #SIMPLE_HEADER_CLASS_DECL, #SIMPLE_HEADER_GLOBAL_DECL_T, #SIMPLE_HEADER_DEF
#define PROCESS_RECORDER_HEADERS_SIMPLE(cmd, arg)                          \
	cmd(sensitivity_level, float,  HEADER_RECORDER_SENSITIVITY_LEVEL, arg) \
	cmd(no_input_timeout,  size_t, HEADER_RECORDER_NO_INPUT_TIMEOUT, arg)  \
	cmd(completion_cause,  UniMRCPRecorderCompletionCause, HEADER_RECORDER_COMPLETION_CAUSE, arg) \
	cmd(max_time,          size_t, HEADER_RECORDER_MAX_TIME, arg)          \
	cmd(trim_length,       size_t, HEADER_RECORDER_TRIM_LENGTH, arg)       \
	cmd(final_silence,     size_t, HEADER_RECORDER_FINAL_SILENCE, arg)

/**
 * @brief Recorder resource message with headers
 *
 * Create messages with UniMRCPClientResourceChannel<MRCP_RECORDER>::CreateMessage().
 * They are created automatically upon calling UniMRCPClientResourceChannel<MRCP_RECORDER>::OnMessageReceive().
 * @see UniMRCPRecorderMessage
 * @see UniMRCPRecorderChannel
 */
template<>
class UniMRCPResourceMessage<MRCP_RECORDER> :
	public UniMRCPResourceMessageBase<UniMRCPRecorderHeaderId, UniMRCPRecorderMethod, UniMRCPRecorderEvent>
{
/** @name MRCP recognizer headers getters and setters */
/** @{ */
public:
	PROCESS_RECORDER_HEADERS_STR(STR_HEADER_CLASS_DECL, UniMRCPRecorderMessage)
public:
	PROCESS_RECORDER_HEADERS_BOOL(BOOL_HEADER_CLASS_DECL, UniMRCPRecorderMessage)
public:
	PROCESS_RECORDER_HEADERS_SIMPLE(SIMPLE_HEADER_CLASS_DECL, UniMRCPRecorderMessage)
/** @} */
private:
	mrcp_recorder_header_t* hdr;  ///< Recorder header opque C structure

	UniMRCPResourceMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException);

	template<UniMRCPResource resource>
	friend class UniMRCPClientResourceChannel;
};

/** @brief Shorthand for recorder resource message */
typedef UniMRCPResourceMessage<MRCP_RECORDER> UniMRCPRecorderMessage;

#ifndef DOXYGEN
PROCESS_RECORDER_HEADERS_STR(STR_HEADER_GLOBAL_DECL_T, MRCP_RECORDER)
PROCESS_RECORDER_HEADERS_BOOL(BOOL_HEADER_GLOBAL_DECL_T, MRCP_RECORDER)
PROCESS_RECORDER_HEADERS_SIMPLE(SIMPLE_HEADER_GLOBAL_DECL_T, MRCP_RECORDER)
#endif


/** @brief Allocate object from MRCP session APR memory pool */
WRAPPER_DECL void* operator new(size_t objSize, mrcp_session_t* sess);
/** @brief Called if construction after allocation from session pool failed. @see operator new(size_t objSize, mrcp_session_t* sess) */
inline void operator delete(void* obj, mrcp_session_t* sess) {
	(void) obj;
	(void) sess;
}


/**
 * @brief MRCP resource channel. Uses properly typed messages and methods.
 */
template<UniMRCPResource resource>
class UniMRCPClientResourceChannel: public UniMRCPClientChannel {
public:
	/** @brief Create a resource channel */
	inline UniMRCPClientResourceChannel(UniMRCPClientSession* session, UniMRCPAudioTermination* termination) THROWS(UniMRCPException) :
		UniMRCPClientChannel(session, resource, termination)
	{
	}

	/** @brief Explicitly define public destructor, otherwise SWIG exception occurs */
	virtual inline ~UniMRCPClientResourceChannel()
	{
	}

	/** @brief Resource message received */
	virtual bool OnMessageReceive(UniMRCPResourceMessage<resource> const* message)
	{
		(void) message;
		return false;
	}

	/** @brief Create resource message */
	inline UniMRCPResourceMessage<resource>* CreateMessage(typename UniMRCPResourceMessage<resource>::Method method, bool autoAddProperty = true) THROWS(UniMRCPException)
	{
		return new(sess) UniMRCPResourceMessage<resource>(
			sess, chan, CreateMsg(method), autoAddProperty);
	}

private:
	/** Create message of proper type and pass to OnMessageReceive */
	virtual bool OnMsgReceive(mrcp_message_t* message)
	{
		return OnMessageReceive(new(sess) UniMRCPResourceMessage<resource>(
			sess, chan, message, false));
	}
};

/** @brief Shorthand for synthesizer resource channel. */
typedef UniMRCPClientResourceChannel<MRCP_SYNTHESIZER> UniMRCPSynthesizerChannel;
/** @brief Shorthand for recognizer resource channel. */
typedef UniMRCPClientResourceChannel<MRCP_RECOGNIZER> UniMRCPRecognizerChannel;
/** @brief Shorthand for recorder resource channel. */
typedef UniMRCPClientResourceChannel<MRCP_RECORDER> UniMRCPRecorderChannel;


#endif  // ifndef UNIMRCP_WRAPPER_H

/*
TO-DO:
------
- Warnings
- Move everything possible from wrapper.h to unimrcp.i
- Resource discovery
- Verifier resource
- Keep methods and headers up-to-date with UniMRCP sources
- Refactor header accessors to use HeaderAccessor naming instead of header_accessor?
- UTF/Unicode for header (or string data generally)?
*/
