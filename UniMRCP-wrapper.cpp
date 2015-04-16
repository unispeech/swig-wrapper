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

/**
 * @file UniMRCP-wrapper.cpp
 * @brief UniMRCP Client wrapper implementation.
 */

#ifdef DOXYGEN
/** @brief Define to log data sent to and received from the application */
#define LOG_STREAM_DATA

/** @brief Define to log media frames flowing */
#define LOG_STREAM_FRAMES
#endif  // DOXYGEN

/** @brief Tell the header we are including from this source */
#define UNIMRCP_WRAPPER_CPP
#include "UniMRCP-wrapper.h"
#include "UniMRCP-wrapper-version.h"
#include <stdlib.h>  // For malloc, realloc and free
#include "apr_general.h"
#include "apr_atomic.h"
#include "apt_pool.h"
#include "apt_dir_layout.h"
#include "apt_log.h"
#include "apt_pair.h"
#include "unimrcp_client.h"
#include "mrcp_application.h"
#include "mrcp_types.h"
#include "mrcp_message.h"
#include "mrcp_recog_header.h"
#include "mrcp_recorder_header.h"
#include "mrcp_synth_header.h"
#include "mpf_dtmf_generator.h"
#include "mpf_dtmf_detector.h"
#include "apr_version.h"
#include "apu_version.h"
#include "uni_version.h"
/** @brief Sofia include version hack */
#define SU_CONFIGURE_H
#include "sofia-sip/su_config.h"
#include "sofia-sip/sofia_features.h"
#if defined(WIN32) && defined(PTW32_STATIC_LIB)
#	include "pthread.h"
#endif

#ifdef _MSC_VER
#	define snprintf _snprintf
#endif

/** @brief Throw UniMRCP exception from here with message */
#define UNIMRCP_THROW(msg) throw UniMRCPException(__FILE__, __LINE__, msg)

/** @brief Log line formatting buffer */
#ifndef MAX_LOG_ENTRY_SIZE
#	define MAX_LOG_ENTRY_SIZE 4096
#endif


/**
 * @brief Allocate object's memory from APR memory pool
 */
void* operator new(size_t objSize, apr_pool_t* pool)
{
	return apr_palloc(pool, objSize);
}


/**
 * @brief Called if construction after allocation from pool failed.
 * @see operator new(size_t objSize, apr_pool_t* pool)
 */
void operator delete(void* obj, apr_pool_t* pool)
{
	(void) obj;
	(void) pool;
}


void* operator new(size_t objSize, mrcp_session_t* sess)
{
	return apr_palloc(mrcp_application_session_pool_get(sess), objSize);
}


UniMRCPLogger* UniMRCPLogger::logger = NULL;
unsigned       UniMRCPClient::instances = 0;
unsigned       UniMRCPClient::staticInitialized = 0;
apr_pool_t*    UniMRCPClient::staticPool = NULL;


UniMRCPLogger::~UniMRCPLogger()
{
#ifdef _DEBUG
	printf("~UniMRCPLogger logger(%p)\n", logger);
#endif
	logger = NULL;
}


int UniMRCPLogger::LogExtHandler(char const* file, int line, char const* id, UniMRCPLogPriority priority, char const* format, va_list arg_ptr)
{
	(void) id;
	if (!logger) return false;
	char buf[MAX_LOG_ENTRY_SIZE];
	buf[0] = 0;
	apr_vsnprintf(buf, MAX_LOG_ENTRY_SIZE, format, arg_ptr);
	return logger->Log(file, line, priority, buf);
}


void UniMRCPClient::UniMRCPVersion(unsigned short& major, unsigned short& minor, unsigned short& patch)
{
	major = UNI_MAJOR_VERSION;
	minor = UNI_MINOR_VERSION;
	patch = UNI_PATCH_VERSION;
}


void UniMRCPClient::WrapperVersion(unsigned short& major, unsigned short& minor, unsigned short& patch)
{
	major = UW_MAJOR_VERSION;
	minor = UW_MINOR_VERSION;
	patch = UW_PATCH_VERSION;
}


void UniMRCPClient::APRVersion(unsigned short& major, unsigned short& minor, unsigned short& patch)
{
	major = APR_MAJOR_VERSION;
	minor = APR_MINOR_VERSION;
	patch = APR_PATCH_VERSION;
}


void UniMRCPClient::APRUtilVersion(unsigned short& major, unsigned short& minor, unsigned short& patch)
{
	major = APU_MAJOR_VERSION;
	minor = APU_MINOR_VERSION;
	patch = APU_PATCH_VERSION;
}


char const* UniMRCPClient::SofiaSIPVersion()
{
	return SOFIA_SIP_VERSION;
}


void UniMRCPClient::StaticPreinitialize(int& fd_stdin, int& fd_stdout, int& fd_stderr) THROWS(UniMRCPException)
{
	if (staticInitialized) {
		staticInitialized++;
#ifdef _DEBUG
		printf("staticInitialized(%u)\n", staticInitialized);
#endif
		return;
	}

// Must be called once per process when linked statically for SofiaSIP to work
#if defined(WIN32) && defined(PTW32_STATIC_LIB)
	if (!pthread_win32_process_attach_np())
		UNIMRCP_THROW("Cannot initialize pthreads win32");
#endif

// Switch file descriptors in order to be able to write on console
#if defined(WIN32) && defined(DOTNET_CONSOLE_HACK)
	fd_stdin = _fileno(stdin);
	if (fd_stdin < 0) freopen("CONIN$", "rt", stdin);
	fd_stdout = _fileno(stdout);
	if (fd_stdout < 0) freopen("CONOUT$", "wt", stdout);
	fd_stderr = _fileno(stderr);
	if (fd_stderr < 0) freopen("CONERR$", "wt", stderr);
#else
	(void) fd_stdin;
	(void) fd_stdout;
	(void) fd_stderr;
#endif

	/* APR global initialization */
	if(apr_initialize() != APR_SUCCESS)
	{
		apr_terminate();
		UNIMRCP_THROW("Error initializing APR");
	}

	/* Create APR pool */
	staticPool = apt_pool_create();
	if (!staticPool)
		UNIMRCP_THROW("Insufficient memory");

	staticInitialized++;
#ifdef _DEBUG
	printf("staticInitialized(%u)\n", staticInitialized);
#endif
}


void UniMRCPClient::StaticInitialize(char const* root_dir,
                                     UniMRCPLogPriority log_prio,
                                     UniMRCPLogOutput log_out,
                                     char const* log_fname,
                                     unsigned max_log_fsize,
                                     unsigned max_log_fcount) THROWS(UniMRCPException)
{
	int fd_stdin, fd_stdout, fd_stderr;
	apt_dir_layout_t* dirLayout;
	StaticPreinitialize(fd_stdin, fd_stdout, fd_stderr);

	/* create the structure of default directories layout */
	dirLayout = apt_default_dir_layout_create(root_dir, staticPool);

	/* get path to logger configuration file */
	const char* logConfPath = apt_confdir_filepath_get(dirLayout, "logger.xml", staticPool);
	/* create and load singleton logger */
	apt_log_instance_load(logConfPath, staticPool);
	/* override the log priority, if specified in command line */
	apt_log_priority_set(static_cast<apt_log_priority_e>(log_prio));
	apt_log_output_mode_set(static_cast<apt_log_output_e>(log_out));
	if (!log_fname) log_fname = unimrcp_client_log_name;
	if (apt_log_output_mode_check(APT_LOG_OUTPUT_FILE) == TRUE)
	{
		/* open the log file */
#if defined(UNI_VERSION_AT_LEAST) && UNI_VERSION_AT_LEAST(1, 3, 0)
		apt_log_file_open(apt_dir_layout_path_get(dirLayout, APT_LAYOUT_LOG_DIR),
			log_fname, max_log_fsize, max_log_fcount, FALSE, staticPool);
#else
		apt_log_file_open(dirLayout->log_dir_path, log_fname,
			max_log_fsize, max_log_fcount, FALSE, staticPool);
#endif
	}
	apt_log(APT_LOG_MARK, APT_PRIO_INFO, "Initialized UniMRCP for %s: log_root_dir(%s) "
		"log_prio(%d) log_out(%d) log_fname(%s) max_log_fsize(%u) max_log_fcount(%u)",
		swig_target_platform, root_dir, static_cast<int>(log_prio),
		static_cast<int>(log_out), log_fname, max_log_fsize, max_log_fcount);
	StaticPostinitialize(fd_stdin, fd_stdout, fd_stderr);
}


void UniMRCPClient::StaticInitialize(UniMRCPLogger* logger,
                                     UniMRCPLogPriority log_prio) THROWS(UniMRCPException)
{
	int fd_stdin, fd_stdout, fd_stderr;
	StaticPreinitialize(fd_stdin, fd_stdout, fd_stderr);

	apt_log_instance_create(APT_LOG_OUTPUT_NONE, static_cast<apt_log_priority_e>(log_prio), staticPool);
	UniMRCPLogger::logger = logger;
	apt_log_ext_handler_set(reinterpret_cast<apt_log_ext_handler_f>(UniMRCPLogger::LogExtHandler));
	apt_log(APT_LOG_MARK, APT_PRIO_INFO, "Initialized UniMRCP for %s: "
		"logger(%pp) log_prio(%d)", swig_target_platform, logger, static_cast<int>(log_prio));
	StaticPostinitialize(fd_stdin, fd_stdout, fd_stderr);
}


void UniMRCPClient::StaticPostinitialize(int fd_stdin, int fd_stdout, int fd_stderr)
{
#if defined(WIN32) && defined(DOTNET_CONSOLE_HACK)
	if (fd_stdin < 0) apt_log(APT_LOG_MARK, APT_PRIO_DEBUG,
		".NET console hack for stdin: old fd: %d, new fd %d", fd_stdin, _fileno(stdin));
	if (fd_stdout < 0) apt_log(APT_LOG_MARK, APT_PRIO_DEBUG,
		".NET console hack for stdout: old fd: %d, new fd %d", fd_stdout, _fileno(stdout));
	if (fd_stderr < 0) apt_log(APT_LOG_MARK, APT_PRIO_DEBUG,
		".NET console hack for stderr: old fd: %d, new fd %d", fd_stderr, _fileno(stderr));
#else
	(void) fd_stdin;
	(void) fd_stdout;
	(void) fd_stderr;
#endif
}


void UniMRCPClient::StaticDeinitialize() THROWS(UniMRCPException)
{
	if (!staticInitialized) {
#ifdef _DEBUG
		printf("staticInitialized(%u) deinitialize\n", staticInitialized);
#endif
		return;
	}
	if ((staticInitialized == 1) && instances)
		UNIMRCP_THROW("Destroy all clients before static deinitialization");
	staticInitialized--;
	if (staticInitialized) {
#ifdef _DEBUG
		printf("staticInitialized(%u) deinitialize\n", staticInitialized);
#endif
		return;
	}
	/* destroy singleton logger */
	apt_log_instance_destroy();
	/* destroy APR pool */
	apr_pool_destroy(staticPool);
	staticPool = NULL;
	/* APR global termination */
	apr_terminate();

#	ifdef PTW32_STATIC_LIB
	pthread_win32_process_detach_np();
#	endif
#ifdef _DEBUG
	printf("staticDeinitialized\n");
#endif
}


UniMRCPClient::UniMRCPClient(char const* config, bool dir /* = false */)  THROWS(UniMRCPException) :
	client(NULL),
	app(NULL),
	terminated(false),
	sess_id(1)
{
	if (!staticInitialized)
		UNIMRCP_THROW("UniMRCP platform not statically initialized");

	if (dir) {
		/* create the structure of default directories layout */
		apt_dir_layout_t* dirLayout = apt_default_dir_layout_create(config, staticPool);
		client = unimrcp_client_create(dirLayout);
	} else
		client = unimrcp_client_create2(config);

	apt_log(APT_LOG_MARK, APT_PRIO_NOTICE, "Creating %s UniMRCPClient (%pp) instance %u",
		swig_target_platform, this, instances);

	if (!client)
		UNIMRCP_THROW("Cannot create UniMRCP client");
	app = mrcp_application_create(AppMessageHandler, this, mrcp_client_memory_pool_get(client));
	if (!app) {
		mrcp_client_destroy(client);
		client = NULL;
		UNIMRCP_THROW("Cannot create UniMRCP client application");
	}

	/* register MRCP application to MRCP client */
	if (mrcp_client_application_register(client, app, unimrcp_client_app_name) == FALSE) {
		mrcp_application_destroy(app);
		app = NULL;
		mrcp_client_destroy(client);
		client = NULL;
		UNIMRCP_THROW("Cannot register UniMRCP client application");
	}
	/* start MRCP client stack processing */
	if (mrcp_client_start(client) == FALSE) {
		mrcp_application_destroy(app);
		app = NULL;
		mrcp_client_destroy(client);
		client = NULL;
		UNIMRCP_THROW("Cannot start UniMRCP client");
	}
	instances++;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s UniMRCPClient created: client(%pp) app(%pp) instances(%u) this(%pp)",
		swig_target_platform, client, app, instances, this);
}



UniMRCPClient::~UniMRCPClient()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s ~UniMRCPClient: client(%pp) app(%pp) this(%pp)",
		swig_target_platform, client, app, this);
	Destroy();
}


void UniMRCPClient::Destroy()
{
	if (client && !terminated) {
		terminated = true;
		mrcp_client_shutdown(client);
		mrcp_client_destroy(client);
		app = NULL;
		client = NULL;
		instances--;
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s UniMRCPClient (%pp) destroyed, instances(%u)",
			swig_target_platform, this, instances);
	}
}


apt_bool_t UniMRCPClient::AppMessageHandler(mrcp_app_message_t const* msg)
{
	static const mrcp_app_message_dispatcher_t appDisp =
	{
		reinterpret_cast<int (*)(mrcp_application_t*, mrcp_session_t*, mrcp_sig_status_code_e)>(
			UniMRCPClientSession::AppOnSessionUpdate),
		reinterpret_cast<int (*)(mrcp_application_t*, mrcp_session_t*, mrcp_sig_status_code_e)>(
			UniMRCPClientSession::AppOnSessionTerminate),
		reinterpret_cast<int (*)(mrcp_application_t*, mrcp_session_t*, mrcp_channel_t*, mrcp_sig_status_code_e)>(
			UniMRCPClientChannel::AppOnChannelAdd),
		reinterpret_cast<int (*)(mrcp_application_t*, mrcp_session_t*, mrcp_channel_t*, mrcp_sig_status_code_e)>(
			UniMRCPClientChannel::AppOnChannelRemove),
		UniMRCPClientChannel::AppOnMessageReceive,
		UniMRCPClientSession::AppOnTerminateEvent,
		reinterpret_cast<int (*)(mrcp_application_t*, mrcp_session_t*, mrcp_session_descriptor_t*, mrcp_sig_status_code_e)>(
			UniMRCPClientSession::AppOnResourceDiscover)
	};
	return mrcp_application_message_dispatch(&appDisp, msg);
}


UniMRCPClientSession::UniMRCPClientSession(UniMRCPClient* _client, char const* profile) THROWS(UniMRCPException) :
	sess(NULL),
	client(_client),
	terminated(false),
	destroyOnTerminate(false)
{
	sess = mrcp_application_session_create(client->app, profile, this);
	if (!sess)
		UNIMRCP_THROW("Cannot create UniMRCP client session");
	char name[64];
	unsigned int id = apr_atomic_inc32(&client->sess_id);
	snprintf(name, sizeof(name) - 1, "%s-%02u", swig_target_platform, static_cast<unsigned>(id));
	mrcp_application_session_name_set(sess, name);
}


UniMRCPClientSession::~UniMRCPClientSession()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s ~UniMRCPClientSession sess(%pp) terminated(%s) this(%pp)",
		swig_target_platform, sess, terminated ? "TRUE" : "FALSE", this);
	if (sess)
		// This object will not exist anymore
		mrcp_application_session_object_set(sess, NULL);
	Destroy();
}


void UniMRCPClientSession::ResourceDiscover()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s Session ResourceDiscover sess(%pp) this(%pp)",
		swig_target_platform, sess, this);
	if (sess)
		mrcp_application_resource_discover(sess);
}


void UniMRCPClientSession::Terminate()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s Session Terminate sess(%pp) terminated(%s) this(%pp)",
		swig_target_platform, sess, terminated ? "TRUE" : "FALSE", this);
	if (sess && !terminated) {
		mrcp_application_session_terminate(sess);
	}
}


void UniMRCPClientSession::Destroy()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s Session Destroy sess(%pp) terminated(%s) this(%pp)",
		swig_target_platform, sess, terminated ? "TRUE" : "FALSE", this);
	if (client->terminated) return;
	if (destroyOnTerminate) return;
	if (sess) {
		if (!terminated) {
			destroyOnTerminate = true;
			mrcp_application_session_terminate(sess);
		} else {
			mrcp_application_session_destroy(sess);
			sess = NULL;
		}
	}
}


char const* UniMRCPClientSession::GetID() const
{
	if (!sess) return NULL;
	return mrcp_application_session_id_get(sess)->buf;
}


bool UniMRCPClientSession::OnUpdate(UniMRCPSigStatusCode status)
{
	(void) status;
	return true;
}


bool UniMRCPClientSession::OnTerminate(UniMRCPSigStatusCode status)
{
	(void) status;
	return true;
}


bool UniMRCPClientSession::OnTerminateEvent()
{
	return true;
}


apt_bool_t UniMRCPClientSession::AppOnSessionUpdate(mrcp_application_t* application, mrcp_session_t* session, UniMRCPSigStatusCode status)
{
	(void) application;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnSessionUpdate: sess(%pp) status(%d) sess_obj(%pp)",
		swig_target_platform, session, static_cast<int>(status), s);
	if (!s) return FALSE;
	bool ret = s->OnUpdate(status);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnSessionUpdate: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	return ret;
}


apt_bool_t UniMRCPClientSession::AppOnSessionTerminate(mrcp_application_t* application, mrcp_session_t* session, UniMRCPSigStatusCode status)
{
	(void) application;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	if (!s) {
		// Session object already destroyed so free the C memory as well
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnSessionTerminate: sess(%pp) status(%d) sess_obj(NULL)",
			swig_target_platform, session);
		mrcp_application_session_destroy(session);
		return false;
	}
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnSessionTerminate: sess(%pp) status(%d) destroyOnTerminate(%s)",
		swig_target_platform, session, static_cast<int>(status), s->destroyOnTerminate ? "TRUE" : "FALSE");
	s->terminated = true;
	bool ret;
	if (!s->destroyOnTerminate)
		ret = s->OnTerminate(status);
	else
		ret = false;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnSessionTerminate: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	if (s->destroyOnTerminate) {
		mrcp_application_session_destroy(session);
		s->sess = NULL;
	}
	return ret;
}


apt_bool_t UniMRCPClientSession::AppOnTerminateEvent(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel)
{
	(void) application;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	UniMRCPClientChannel* c = reinterpret_cast<UniMRCPClientChannel*>(mrcp_application_channel_object_get(channel));
	if (!s) {
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnTerminateEvent: sess(%pp) status(%d) obj(NULL)",
			swig_target_platform, session);
		return false;
	}
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnTerminateEvent: sess(%pp) chan(%pp) sess_obj(%pp) chan_obj(%pp)",
		swig_target_platform, session, channel, s, c);
	bool ret = false;
	if (!s->destroyOnTerminate) {
		if (channel) {
			if (c) ret |= c->OnTerminateEvent();
		}
		if (session) {
			ret |= s->OnTerminateEvent();
		}
	}
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnTerminateEvent: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	return ret;
}


apt_bool_t UniMRCPClientSession::AppOnResourceDiscover(mrcp_application_t* application, mrcp_session_t* session, mrcp_session_descriptor_t* descriptor, UniMRCPSigStatusCode status)
{
	(void) application;
	(void) descriptor;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnResourceDiscover: sess(%pp) status(%d) sess_obj(%pp)",
		swig_target_platform, session, static_cast<int>(status), s);
	bool ret = false;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnResourceDiscover: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	return ret;
}


UniMRCPStreamRx::UniMRCPStreamRx() :
	frm(NULL),
	dtmf_gen(NULL),
	term(NULL)
{}


UniMRCPStreamRx::~UniMRCPStreamRx()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s ~UniMRCPStreamRx term(%pp) dtmf_gen(%pp) this(%pp)",
		swig_target_platform, term, dtmf_gen, this);
	if (term) {
		term->streamRx = NULL;
		term = NULL;
	}
}


bool UniMRCPStreamRx::SendDTMF(char digit)
{
	if (!dtmf_gen) return false;
	char digits[2] = {digit, 0};
	return mpf_dtmf_generator_enqueue(dtmf_gen, digits) ? true : false;
}


size_t UniMRCPStreamRx::GetDataSize() const
{
	if (!frm) return 0;
	return frm->codec_frame.size;
}


void UniMRCPStreamRx::SetData(void const* buf, size_t len)
{
	if (!frm) return;
	if (len > frm->codec_frame.size)
		len = frm->codec_frame.size;
		memcpy(frm->codec_frame.buffer, buf, len);
	frm->type |= MEDIA_FRAME_TYPE_AUDIO;
#ifdef LOG_STREAM_DATA
	printf("%s UniMRCPStreamRx::SetData %lu bytes:\n", swig_target_platform,
		static_cast<unsigned long>(len));
	for (size_t i = 0; i < len; i++)
		printf("%02x", static_cast<unsigned char const*>(buf)[i]);
	putchar('\n');
#endif
}


void UniMRCPStreamRx::OnClose()
{
}


bool UniMRCPStreamRx::ReadFrame()
{
	return false;
}


bool UniMRCPStreamRx::OnOpenInternal(UniMRCPAudioTermination const* term, mpf_audio_stream_t const* stm)
{
	if (term->dg_band >= 0) {
		dtmf_gen = mpf_dtmf_generator_create_ex(stm,
			term->dg_band ? static_cast<mpf_dtmf_generator_band_e>(term->dg_band) :
				(stm->rx_event_descriptor ? MPF_DTMF_GENERATOR_OUTBAND : MPF_DTMF_GENERATOR_INBAND),
			term->dg_tone, term->dg_silence, mrcp_application_session_pool_get(term->sess));
		if (!dtmf_gen) {
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "%s StreamOpenRx: Failed to create DTMF generator",
				swig_target_platform);
			return false;
		}
	}
	return true;
}


void UniMRCPStreamRx::OnCloseInternal()
{
	if (dtmf_gen) {
		mpf_dtmf_generator_destroy(dtmf_gen);
		dtmf_gen = NULL;
	}
}


UniMRCPStreamRxBuffered::UniMRCPStreamRxBuffered() :
	UniMRCPStreamRx(),
	first(NULL),
	last(NULL),
	len(0),
	pos(0),
	flush(false),
	mutex(NULL)
#ifdef UW_TRACE_BUFFERS
	,rcv(0)
	,snt(0)
#endif
{
}


UniMRCPStreamRxBuffered::~UniMRCPStreamRxBuffered()
{
}


/**
 * @brief Calculate total buffer chunk size (i.e. metadata/headers plus audio data)
 * @param len Length of audio data in bytes
 */
#define CHUNK_SIZE(len) (sizeof(chunk_t) + len)

bool UniMRCPStreamRxBuffered::AddData(void const* buf, size_t len)
{
	if (!mutex) return false;
	chunk_t* ch = static_cast<chunk_t*>(malloc(CHUNK_SIZE(len)));
	if (!ch) return false;
	ch->next = NULL;
#ifdef UW_TRACE_BUFFERS
	rcv += len;
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Received %8lu bytes, total: %8lu",
		static_cast<unsigned long>(len), rcv);
#endif
	ch->digit = 0;
	ch->len = len;
	memcpy(ch->data, buf, len);
	apr_thread_mutex_lock(mutex);
	if (last)
		last->next = ch;
	else
		first = ch;
	last = ch;
	this->len += len;
	apr_thread_mutex_unlock(mutex);
	return true;
}


bool UniMRCPStreamRxBuffered::SendDTMF(char _digit)
{
	if (!mutex) return false;
	apr_thread_mutex_lock(mutex);
	if (last && !last->digit) {
		last->digit = _digit;
	} else {
		chunk_t* ch = static_cast<chunk_t*>(malloc(CHUNK_SIZE(len)));
		if (ch) {
			ch->next = NULL;
			ch->len = 0;
			ch->digit = _digit;
			if (last)
				last->next = ch;
			else
				first = ch;
			last = ch;
		}
	}
	apr_thread_mutex_unlock(mutex);
	return true;
}


void UniMRCPStreamRxBuffered::Flush()
{
	if (!mutex) return;
	apr_thread_mutex_lock(mutex);
	flush = true;
	apr_thread_mutex_unlock(mutex);
}


bool UniMRCPStreamRxBuffered::ReadFrame()
{
	if (!mutex) return false;
	size_t copied = 0;
	apr_thread_mutex_lock(mutex);
	if ((!flush && (len < frm->codec_frame.size)) || (first && !first->len)) {
		apr_thread_mutex_unlock(mutex);
		if (dtmf_gen)
			mpf_dtmf_generator_put_frame(dtmf_gen, frm);
		return false;
	}
	while (first && ((copied < frm->codec_frame.size) || !first->len)) {
		size_t size = first->len - pos < frm->codec_frame.size - copied ?
			first->len - pos : frm->codec_frame.size - copied;
		memcpy(static_cast<char*>(frm->codec_frame.buffer) + copied, first->data + pos, size);
		pos += size;
		len -= size;
		copied += size;
		if (pos >= first->len) {
			if (first->digit) {
				char const digits[2] = {first->digit, 0};
				if (dtmf_gen)
					mpf_dtmf_generator_enqueue(dtmf_gen, digits);
				apt_log(APT_LOG_MARK, APT_PRIO_INFO, "Sending DTMF: %s", digits);
			}
			pos = 0;
			chunk_t *ch = first;
			first = first->next;
			if (!first) last = NULL;
			free(ch);
		}
	}
	if (!first) flush = false;
	apr_thread_mutex_unlock(mutex);
	if (copied) {
#ifdef UW_TRACE_BUFFERS
		snt += copied;
		apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "Sent %8lu bytes, total: %8lu",
			static_cast<unsigned long>(copied), snt);
#endif
		frm->type |= MEDIA_FRAME_TYPE_AUDIO;
		memset(static_cast<char*>(frm->codec_frame.buffer) + copied, 0,
			frm->codec_frame.size - copied);
	}
	if (dtmf_gen)
		mpf_dtmf_generator_put_frame(dtmf_gen, frm);
	return true;
}


bool UniMRCPStreamRxBuffered::OnOpenInternal(UniMRCPAudioTermination const* term, mpf_audio_stream_t const* stm)
{
	if (!UniMRCPStreamRx::OnOpenInternal(term, stm))
		return false;
	apr_status_t status = apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_DEFAULT,
		mrcp_application_session_pool_get(term->sess));
	if (status != APR_SUCCESS) {
		apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "%s StreamRxBuffered Cannot create mutex: %d %pm",
			swig_target_platform, status, &status);
		return false;
	}
	return true;
}


void UniMRCPStreamRxBuffered::OnCloseInternal()
{
	chunk_t *ch;
	while (first) {
		ch = first;
		first = first->next;
		free(ch);
	}
	first = NULL;
	last = NULL;
	if (mutex) {
		apr_thread_mutex_destroy(mutex);
		mutex = NULL;
	}
	UniMRCPStreamRx::OnCloseInternal();
}


UniMRCPStreamTx::UniMRCPStreamTx() :
	frm(NULL),
	dtmf_det(NULL),
	term(NULL)
{
}


UniMRCPStreamTx::~UniMRCPStreamTx()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s ~UniMRCPStreamTx term(%pp) dtmf_det(%pp) this(%pp)",
		swig_target_platform, term, dtmf_det, this);
	if (term) {
		term->streamTx = NULL;
		term = NULL;
	}
}


char UniMRCPStreamTx::GetDTMF()
{
	if (!dtmf_det) return 0;
	return mpf_dtmf_detector_digit_get(dtmf_det);
}


bool UniMRCPStreamTx::HasAudio() const
{
	if (!frm) return 0;
	return frm->type & MEDIA_FRAME_TYPE_AUDIO;
}


size_t UniMRCPStreamTx::GetDataSize() const
{
	if (!frm) return 0;
	return frm->codec_frame.size;
}


void UniMRCPStreamTx::GetData(void* buf, size_t len)
{
	if (!frm) return;
	if (len > frm->codec_frame.size)
		len = frm->codec_frame.size;
	memcpy(buf, frm->codec_frame.buffer, len);
#ifdef LOG_STREAM_DATA
	printf("%s UniMRCPStreamRx::GetData %lu bytes:\n", swig_target_platform,
		static_cast<unsigned long>(len));
	for (size_t i = 0; i < len; i++)
		printf("%02x", static_cast<unsigned char const*>(buf)[i]);
	putchar('\n');
#endif
}


void UniMRCPStreamTx::OnClose()
{
}


bool UniMRCPStreamTx::WriteFrame()
{
	return false;
}


bool UniMRCPStreamTx::OnOpenInternal(UniMRCPAudioTermination const* term, mpf_audio_stream_t const* stm)
{
	if (term->dd_band >= 0) {
		dtmf_det = mpf_dtmf_detector_create_ex(stm,
			term->dd_band ? static_cast<mpf_dtmf_detector_band_e>(term->dd_band) :
				(stm->tx_event_descriptor ? MPF_DTMF_DETECTOR_OUTBAND : MPF_DTMF_DETECTOR_INBAND),
			mrcp_application_session_pool_get(term->sess));
		if (!dtmf_det) {
			apt_log(APT_LOG_MARK, APT_PRIO_WARNING, "%s StreamOpenRx: Failed to create DTMF detector",
				swig_target_platform);
			return false;
		}
	}
	return true;
}


void UniMRCPStreamTx::OnCloseInternal()
{
	if (dtmf_det) {
		mpf_dtmf_detector_destroy(dtmf_det);
		dtmf_det = NULL;
	}
}


UniMRCPAudioTermination::UniMRCPAudioTermination(UniMRCPClientSession* session) THROWS(UniMRCPException) :
	caps(NULL),
	sess(session->sess),
	streamRx(NULL),
	stmRx(NULL),
	streamTx(NULL),
	stmTx(NULL),
	dg_band(-1),
	dg_tone(70),
	dg_silence(50),
	dd_band(-1)
{
	caps = mpf_stream_capabilities_create(STREAM_DIRECTION_DUPLEX, mrcp_application_session_pool_get(sess));
	if (!caps)
		UNIMRCP_THROW("Cannot initialize UniMRCP stream capabilities");
}


UniMRCPAudioTermination::~UniMRCPAudioTermination()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s ~UniMRCPAudioTermination sess(%pp) streamRx(%pp) stmRx(%pp) streamTx(%pp) stmTx(%pp) this(%pp)",
		swig_target_platform, sess, streamRx, stmRx, streamTx, stmTx, this);
	if (stmRx) {
		stmRx->obj = NULL;
		stmRx = NULL;
	}
	if (stmTx) {
		stmTx->obj = NULL;
		stmTx = NULL;
	}
	if (streamRx) {
		streamRx->term = NULL;
		streamRx = NULL;
	}
	if (streamTx) {
		streamTx->term = NULL;
		streamTx = NULL;
	}
}


void UniMRCPAudioTermination::SetDirection(UniMRCPStreamDirection dir)
{
	caps->direction = static_cast<mpf_stream_direction_e>(dir);
}


bool UniMRCPAudioTermination::AddCapability(char const* codec, UniMRCPSampleRate rates)
{
	return mpf_codec_capabilities_add(&caps->codecs, rates, codec) == TRUE;
}


void UniMRCPAudioTermination::EnableDTMFGenerator(UniMRCPDTMFBand band, unsigned tone_ms, unsigned silence_ms)
{
	dg_band = band;
	dg_tone = tone_ms;
	dg_silence = silence_ms;
}


void UniMRCPAudioTermination::EnableDTMFDetector(UniMRCPDTMFBand band)
{
	dd_band = band;
}


UniMRCPStreamRx* UniMRCPAudioTermination::OnStreamOpenRx(bool enabled, unsigned char payload_type, char const* name,
                                                         char const* format, unsigned char channels, unsigned freq)
{
	(void) enabled;
	(void) payload_type;
	(void) name;
	(void) format;
	(void) channels;
	(void) freq;
	return NULL;
}


UniMRCPStreamTx* UniMRCPAudioTermination::OnStreamOpenTx(bool enabled, unsigned char payload_type, char const* name,
                                                         char const* format, unsigned char channels, unsigned freq)
{
	(void) enabled;
	(void) payload_type;
	(void) name;
	(void) format;
	(void) channels;
	(void) freq;
	return NULL;
}


apt_bool_t UniMRCPAudioTermination::StmDestroy(mpf_audio_stream_t* stream)
{
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamDestroy: stream(%pp) term(%pp) term.stmRx(%pp) term.stmTx(%pp)",
		swig_target_platform, stream, t,
		t ? t->stmRx : NULL, t ? t->stmTx : NULL);
	if (t && (t->stmRx == stream)) t->stmRx = NULL;
	if (t && (t->stmTx == stream)) t->stmTx = NULL;
	return TRUE;
}


apt_bool_t UniMRCPAudioTermination::StmOpenRx(mpf_audio_stream_t* stream, mpf_codec_t* codec)
{
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamOpenRx: stream(%pp) codec(%pp) rx_descriptor(%pp) term(%pp)",
		swig_target_platform, stream, codec, stream->rx_descriptor, t);
	if (!t) return FALSE;
	mpf_codec_descriptor_t const* d = stream->rx_descriptor;
	UniMRCPStreamRx* sr;
	if (d)
		sr = t->OnStreamOpenRx(d->enabled == TRUE, d->payload_type, d->name.buf,
			d->format.buf, d->channel_count, d->sampling_rate);
	else
		sr = t->OnStreamOpenRx(false, 0, NULL, NULL, 0, 0);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamOpenRx: return %pp",
		swig_target_platform, sr);
	if (sr) {
		if (sr->OnOpenInternal(t, stream))
			t->streamRx = sr;
		else
			sr->OnClose();
	}
	if (!sr || !t->streamRx) return FALSE;
	sr->term = t;
	t->stmRx = stream;
	return TRUE;
}


apt_bool_t UniMRCPAudioTermination::StmCloseRx(mpf_audio_stream_t* stream)
{
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamCloseRx: stream(%pp) term(%pp) term.streamRx(%pp)",
		swig_target_platform, stream, t, t ? t->streamRx : NULL);
	if (t && t->streamRx) {
		t->streamRx->OnClose();
		t->streamRx->OnCloseInternal();
		t->streamRx->term = NULL;
	}
	if (t) {
		t->streamRx = NULL;
		t->stmRx = NULL;
	}
	return TRUE;
}


apt_bool_t UniMRCPAudioTermination::StmReadFrame(mpf_audio_stream_t* stream, mpf_frame_t* frame)
{
#ifdef LOG_STREAM_FRAMES
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamReadFrame: stream(%pp)",
		swig_target_platform, stream);
#endif
	bool ret;
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	if (t && t->streamRx) {
		t->streamRx->frm = frame;
		if (t->streamRx->dtmf_gen)
			mpf_dtmf_generator_put_frame(t->streamRx->dtmf_gen, frame);
		ret = t->streamRx->ReadFrame();
	} else
		ret = FALSE;
#ifdef LOG_STREAM_FRAMES
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamReadFrame: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
#endif
	return ret;
}


apt_bool_t UniMRCPAudioTermination::StmOpenTx(mpf_audio_stream_t* stream, mpf_codec_t* codec)
{
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamOpenTx: stream(%pp) codec(%pp) tx_descriptor(%pp) term(%pp)",
		swig_target_platform, stream, codec, stream->tx_descriptor, t);
	if (!t) return FALSE;
	mpf_codec_descriptor_t const* d = stream->tx_descriptor;
	UniMRCPStreamTx* st;
	if (d)
		st = t->OnStreamOpenTx(d->enabled == TRUE, d->payload_type, d->name.buf,
			d->format.buf, d->channel_count, d->sampling_rate);
	else
		st = t->OnStreamOpenTx(false, 0, NULL, NULL, 0, 0);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamOpenTx: return %pp",
		swig_target_platform, st);
	if (st) {
		if (st->OnOpenInternal(t, stream))
			t->streamTx = st;
		else
			st->OnClose();
	}
	if (!st || !t->streamTx) return FALSE;
	st->term = t;
	t->stmTx = stream;
	return TRUE;
}


apt_bool_t UniMRCPAudioTermination::StmCloseTx(mpf_audio_stream_t* stream)
{
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamCloseTx: stream(%pp) term(%pp) term.streamTx(%pp)",
		swig_target_platform, stream, t, t ? t->streamTx : NULL);
	if (t && t->streamTx) {
		t->streamTx->OnClose();
		t->streamTx->OnCloseInternal();
		t->streamTx->term = NULL;
	}
	if (t) {
		t->streamTx = NULL;
		t->stmTx = NULL;
	}
	return TRUE;
}


apt_bool_t UniMRCPAudioTermination::StmWriteFrame(mpf_audio_stream_t* stream, const mpf_frame_t* frame)
{
#ifdef LOG_STREAM_FRAMES
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamWriteFrame: stream(%pp)",
		swig_target_platform, stream);
#endif
	bool ret;
	UniMRCPAudioTermination* t = reinterpret_cast<UniMRCPAudioTermination*>(stream->obj);
	if (t && t->streamTx) {
		t->streamTx->frm = frame;
		if (t->streamTx->dtmf_det)
			mpf_dtmf_detector_get_frame(t->streamTx->dtmf_det, frame);
		ret = t->streamTx->WriteFrame();
	} else
		ret = FALSE;
#ifdef LOG_STREAM_FRAMES
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s StreamWriteFrame: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
#endif
	return ret;
}


UniMRCPClientChannel::UniMRCPClientChannel(UniMRCPClientSession* session, UniMRCPResource resource, UniMRCPAudioTermination* termination) THROWS(UniMRCPException) :
	sess(session->sess),
	chan(NULL)
{
	static const mpf_audio_stream_vtable_t audio_stream_vtable =
	{
		UniMRCPAudioTermination::StmDestroy,
		UniMRCPAudioTermination::StmOpenRx,
		UniMRCPAudioTermination::StmCloseRx,
		UniMRCPAudioTermination::StmReadFrame,
		UniMRCPAudioTermination::StmOpenTx,
		UniMRCPAudioTermination::StmCloseTx,
		UniMRCPAudioTermination::StmWriteFrame,
		NULL          /* Trace */
	};
	mpf_termination_t* term = mrcp_application_audio_termination_create(sess, &audio_stream_vtable, termination->caps, termination);
	if (!term) {
		sess = NULL;
		UNIMRCP_THROW("Cannot create UniMRCP client audio termination");
	}
	chan = mrcp_application_channel_create(sess, resource, term, NULL, this);
	if (!chan) {
		sess = NULL;
		UNIMRCP_THROW("Cannot create UniMRCP client channel");
	}
	if (!mrcp_application_channel_add(sess, chan)) {
		chan = NULL;
		sess = NULL;
		UNIMRCP_THROW("Error adding UniMRCP client channel into session");
	}
}


UniMRCPClientChannel::~UniMRCPClientChannel()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s ~UniMRCPClientChannel sess(%pp) chan(%pp) this(%pp)",
		swig_target_platform, sess, chan, this);
}


mrcp_message_t* UniMRCPClientChannel::CreateMsg(unsigned method) THROWS(UniMRCPException)
{
	mrcp_message_t* msg = mrcp_application_message_create(sess, chan, method);
	if (!msg)
		UNIMRCP_THROW("Cannot create UniMRCP client messge");
	return msg;
}


void UniMRCPClientChannel::Remove()
{
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s Channel Remove sess(%pp) chan(%pp) this(%pp)",
		swig_target_platform, sess, chan, this);
	mrcp_application_channel_remove(sess, chan);
}


bool UniMRCPClientChannel::OnAdd(UniMRCPSigStatusCode status)
{
	(void) status;
	return false;
}


bool UniMRCPClientChannel::OnRemove(UniMRCPSigStatusCode status)
{
	(void) status;
	return true;
}


bool UniMRCPClientChannel::OnMsgReceive(mrcp_message_t* msg)
{
	(void) msg;
	return false;
}


bool UniMRCPClientChannel::OnTerminateEvent()
{
	return true;
}


apt_bool_t UniMRCPClientChannel::AppOnChannelAdd(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, UniMRCPSigStatusCode status)
{
	(void) application;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	UniMRCPClientChannel* c = reinterpret_cast<UniMRCPClientChannel*>(mrcp_application_channel_object_get(channel));
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnChannelAdd: sess(%pp), chan(%pp), status(%d) obj_s(%pp) obj_c(%pp)",
		swig_target_platform, session, channel, static_cast<int>(status), s, c);
	if (!s || !c) return FALSE;
	bool ret = c->OnAdd(status);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnChannelAdd: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	return ret;
}


apt_bool_t UniMRCPClientChannel::AppOnChannelRemove(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, UniMRCPSigStatusCode status)
{
	(void) application;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	UniMRCPClientChannel* c = reinterpret_cast<UniMRCPClientChannel*>(mrcp_application_channel_object_get(channel));
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnChannelRemove: sess(%pp), chan(%pp), status(%d) obj_s(%pp) obj_c(%pp)",
		swig_target_platform, session, channel, static_cast<int>(status), s, c);
	if (!s || !c) return FALSE;
	bool ret = c->OnRemove(status);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnChannelRemove: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	return ret;
}


apt_bool_t UniMRCPClientChannel::AppOnMessageReceive(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, mrcp_message_t* message)
{
	(void) application;
	UniMRCPClientSession* s = reinterpret_cast<UniMRCPClientSession*>(mrcp_application_session_object_get(session));
	UniMRCPClientChannel* c = reinterpret_cast<UniMRCPClientChannel*>(mrcp_application_channel_object_get(channel));
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnMessageReceive: sess(%pp), chan(%pp) obj_s(%pp) obj_c(%pp)",
		swig_target_platform, session, channel, s, c);
	if (!s || !c) return FALSE;
	bool ret = c->OnMsgReceive(message);
	apt_log(APT_LOG_MARK, APT_PRIO_DEBUG, "%s OnMessageReceive: return %s",
		swig_target_platform, ret ? "TRUE" : "FALSE");
	return ret;
}


UniMRCPMessage::UniMRCPMessage(mrcp_session_t* _sess, mrcp_channel_t* _chan, mrcp_message_t* _msg, bool _autoAddProperty) THROWS(UniMRCPException) :
	AutoAddProperty(_autoAddProperty),
	msg(_msg),
	sess(_sess),
	chan(_chan),
	hdr(mrcp_generic_header_get(_msg))
{
	if (!hdr) UNIMRCP_THROW("Cannot access generic header");
	memset(generic_props, 0, sizeof(generic_props));
	memset(resource_props, 0, sizeof(resource_props));
}


UniMRCPMessageType UniMRCPMessage::GetMsgType() const
{
	return static_cast<UniMRCPMessageType>(msg->start_line.message_type);
}


char const* UniMRCPMessage::GetMethodName() const
{
	return msg->start_line.method_name.buf;
}


unsigned UniMRCPMessage::GetMethodID() const THROWS(UniMRCPException)
{
	if (msg->start_line.message_type == MRCP_MESSAGE_TYPE_EVENT)
		UNIMRCP_THROW("Message type is an event, use GetEventID() instead");
	return static_cast<unsigned>(msg->start_line.method_id);
}


unsigned UniMRCPMessage::GetEventID() const THROWS(UniMRCPException)
{
	if (msg->start_line.message_type != MRCP_MESSAGE_TYPE_EVENT)
		UNIMRCP_THROW("Message type is not an event, use GetMethotID() instead");
	return static_cast<unsigned>(msg->start_line.method_id);
}


UniMRCPStatusCode UniMRCPMessage::GetStatusCode() const
{
	return static_cast<UniMRCPStatusCode>(msg->start_line.status_code);
}


UniMRCPRequestState UniMRCPMessage::GetRequestState() const
{
	return static_cast<UniMRCPRequestState>(msg->start_line.request_state);
}


char const* UniMRCPMessage::GetBody() const
{
	return msg->body.buf;
}


void UniMRCPMessage::GetBody(void* buf, size_t len) const
{
	if (len > msg->body.length)
		len = msg->body.length;
	memcpy(buf, msg->body.buf, len);
}


void UniMRCPMessage::SetBody(char const* body)
{
	apt_string_assign(&msg->body, body, msg->pool);
}


void UniMRCPMessage::SetBody(char const* body, size_t len)
{
	apt_string_assign_n(&msg->body, body, len, msg->pool);
}


void UniMRCPMessage::SetBody(void const* buf, size_t len)
{
	SetBody(reinterpret_cast<char const*>(buf), len);
}


bool UniMRCPMessage::Send()
{
	for (unsigned i = 0; i < sizeof(generic_props) * 8; i++)
		if (generic_props[i >> 3] & (1 << (i & 7)))
			mrcp_generic_header_property_add(msg, i);
	for (unsigned i = 0; i < sizeof(resource_props) * 8; i++)
		if (resource_props[i >> 3] & (1 << (i & 7)))
			mrcp_resource_header_property_add(msg, i);
	return mrcp_application_message_send(sess, chan, msg) == TRUE;
}


/**
 * @brief Define string MRCP header accessors and handle lazy property addition
 * @param name  Header name
 * @param type  Unused here
 * @param prop  MRCP property ID (#UniMRCPGenericHeaderId,
                #UniMRCPSynthesizerHeaderId, #UniMRCPRecognizerHeaderId, #UniMRCPRecorderHeaderId)
 * @param class Class the property belongs to (UniMRCPMessage,
 *              #UniMRCPSynthesizerMessage, #UniMRCPRecognizerMessage, #UniMRCPRecorderMessage)
 * @see PROCESS_GENERIC_HEADERS_STR
 * @see PROCESS_SYNTH_HEADERS_STR
 * @see PROCESS_RECOG_HEADERS_STR
 * @see PROCESS_RECORDER_HEADERS_STR
 */
#define STR_HEADER_DEF(name, type, prop, class)            \
	char const* class::name ## _get() const {              \
		return hdr->name.buf;                              \
	}                                                      \
	void class::name ## _set(char const* val) {            \
		apt_string_assign(&hdr->name, val, msg->pool);     \
		if (AutoAddProperty) LazyAddProperty(UW_ ## prop); \
	}

/**
 * @brief Define boolean MRCP header accessors and handle lazy property addition
 * @param name  Header name
 * @param type  Unused here
 * @param prop  MRCP property ID (#UniMRCPGenericHeaderId,
                #UniMRCPSynthesizerHeaderId, #UniMRCPRecognizerHeaderId, #UniMRCPRecorderHeaderId)
 * @param class Class the property belongs to (UniMRCPMessage,
 *              #UniMRCPSynthesizerMessage, #UniMRCPRecognizerMessage, #UniMRCPRecorderMessage)
 * @see PROCESS_SYNTH_HEADERS_BOOL
 * @see PROCESS_RECOG_HEADERS_BOOL
 * @see PROCESS_RECORDER_HEADERS_BOOL
 */
#define BOOL_HEADER_DEF(name, type, prop, class)           \
	bool class::name ## _get() const {                     \
		return hdr->name == TRUE;                          \
	}                                                      \
	void class::name ## _set(bool val) {                   \
		hdr->name = val ? TRUE : FALSE;                    \
		if (AutoAddProperty) LazyAddProperty(UW_ ## prop); \
	}

/**
 * @brief Helper template structure to translate wrapper types to UniMRCP ones
 * @param wrapper_type Wrapper type
 */
template <typename wrapper_type>
struct wrap_to_unimrcp_type {
	/** @brief Original UniMRCP type */
	typedef wrapper_type uni_type;
};
/** @brief UniMRCPRecorderCompletionCause to mrcp_recorder_completion_cause_e type translation */
template<>
struct wrap_to_unimrcp_type<UniMRCPRecorderCompletionCause> {
	typedef mrcp_recorder_completion_cause_e uni_type;
};
/** @brief UniMRCPRecogCompletionCause to mrcp_recog_completion_cause_e type translation */
template<>
struct wrap_to_unimrcp_type<UniMRCPRecogCompletionCause> {
	typedef mrcp_recog_completion_cause_e uni_type;
};
/** @brief UniMRCPSynthesizerCompletionCause to mrcp_synth_completion_cause_e type translation */
template<>
struct wrap_to_unimrcp_type<UniMRCPSynthesizerCompletionCause> {
	typedef mrcp_synth_completion_cause_e uni_type;
};
/** @brief UniMRCPVoiceGender to mrcp_voice_gender_e type translation */
template<>
struct wrap_to_unimrcp_type<UniMRCPVoiceGender> {
	typedef mrcp_voice_gender_e uni_type;
};

/**
 * @brief Define simple-type (e.g. integer) MRCP header accessors and handle lazy property addition
 * @param name  Header name
 * @param type  The simple data type
 * @param prop  MRCP property ID (#UniMRCPGenericHeaderId,
                #UniMRCPSynthesizerHeaderId, #UniMRCPRecognizerHeaderId, #UniMRCPRecorderHeaderId)
 * @param class Class the property belongs to (UniMRCPMessage,
 *              #UniMRCPSynthesizerMessage, #UniMRCPRecognizerMessage, #UniMRCPRecorderMessage)
 * @see PROCESS_GENERIC_HEADERS_SIMPLE
 * @see PROCESS_SYNTH_HEADERS_SIMPLE
 * @see PROCESS_RECOG_HEADERS_SIMPLE
 * @see PROCESS_RECORDER_HEADERS_SIMPLE
 */
#define SIMPLE_HEADER_DEF(name, type, prop, class)         \
	type class::name ## _get() const {                     \
		return static_cast<type>(hdr->name);               \
	}                                                      \
	void class::name ## _set(type val) {                   \
		hdr->name = static_cast<wrap_to_unimrcp_type<type>::uni_type>(val); \
		if (AutoAddProperty) LazyAddProperty(UW_ ## prop); \
	}

/**
 * @brief Define property management methods
 * @param cls    Class to define methods on (UniMRCPMessage)
 * @param prop_t Property type (#UniMRCPGenericHeaderId or recource-dependent [unsigned])
 * @param res    Property kind: generic or resource
 * @see UniMRCPMessage::AddProperty(UniMRCPGenericHeaderId)         @see UniMRCPMessage::AddProperty(unsigned)
 * @see UniMRCPMessage::LazyAddProperty(UniMRCPGenericHeaderId)     @see UniMRCPMessage::LazyAddProperty(unsigned)
 * @see UniMRCPMessage::AddPropertyName(UniMRCPGenericHeaderId)     @see UniMRCPMessage::AddPropertyName(unsigned)
 * @see UniMRCPMessage::CheckProperty(UniMRCPGenericHeaderId) const @see UniMRCPMessage::CheckProperty(unsigned) const
 * @see UniMRCPMessage::RemoveProperty(UniMRCPGenericHeaderId)      @see UniMRCPMessage::RemoveProperty(unsigned)
 */
#define HEADER_DEF(cls, prop_t, res)                                  \
void cls::AddProperty(prop_t prop)                                    \
{                                                                     \
	mrcp_ ## res ## _header_property_add(msg, prop);                  \
}                                                                     \
void cls::LazyAddProperty(prop_t prop)                                \
{                                                                     \
	static size_t const maxprop = sizeof(res ## _props) * 8;          \
	if (prop > maxprop) return;                                       \
	res ## _props[prop >> 3] |= 1 << (prop & 7);                      \
}                                                                     \
void cls::AddPropertyName(prop_t prop)                                \
{                                                                     \
	mrcp_ ## res ## _header_name_property_add(msg, prop);             \
}                                                                     \
bool cls::CheckProperty(prop_t prop) const                            \
{                                                                     \
	return mrcp_ ## res ## _header_property_check(msg, prop) == TRUE; \
}                                                                     \
void cls::RemoveProperty(prop_t prop)                                 \
{                                                                     \
	mrcp_ ## res ## _header_property_remove(msg, prop);               \
}


HEADER_DEF(UniMRCPMessage, UniMRCPGenericHeaderId, generic)

HEADER_DEF(UniMRCPMessage, unsigned, resource)

PROCESS_GENERIC_HEADERS_STR(STR_HEADER_DEF, UniMRCPMessage)

PROCESS_GENERIC_HEADERS_SIMPLE(SIMPLE_HEADER_DEF, UniMRCPMessage)


unsigned UniMRCPMessage::VendorParamCount() const
{
	if (!hdr->vendor_specific_params) return 0;
	int s = apt_pair_array_size_get(hdr->vendor_specific_params);
	return s < 0 ? 0 : s;
}


void UniMRCPMessage::VendorParamAppend(char const* name, char const* value)
{
	apt_str_t sname, svalue;
	if (!hdr->vendor_specific_params) {
		hdr->vendor_specific_params = apt_pair_array_create(2, msg->pool);
		if (!hdr->vendor_specific_params) return;
	}
	if (name)
		apt_string_set(&sname, name);
	else
		apt_string_reset(&sname);
	if (value)
		apt_string_set(&svalue, value);
	else
		apt_string_reset(&svalue);
	apt_pair_array_append(hdr->vendor_specific_params, &sname, &svalue, msg->pool);
	if (AutoAddProperty) LazyAddProperty(UW_HEADER_GENERIC_VENDOR_SPECIFIC_PARAMS);
}


char const* UniMRCPMessage::VendorParamGetName(unsigned i) const
{
	if (!hdr->vendor_specific_params) return NULL;
	apt_pair_t const* p = apt_pair_array_get(hdr->vendor_specific_params, i);
	if (!p) return NULL;
	return p->value.buf;
}


char const* UniMRCPMessage::VendorParamGetValue(unsigned i) const
{
	if (!hdr->vendor_specific_params) return NULL;
	apt_pair_t const* p = apt_pair_array_get(hdr->vendor_specific_params, i);
	if (!p) return NULL;
	return p->name.buf;
}


char const* UniMRCPMessage::VendorParamFind(char const* name) const
{
	apt_str_t sname;
	if (!name) return NULL;
	if (!hdr->vendor_specific_params) return NULL;
	apt_string_set(&sname, name);
	apt_pair_t const* p = apt_pair_array_find(hdr->vendor_specific_params, &sname);
	if (p) return p->value.buf;
	return NULL;
}


unsigned UniMRCPMessage::ActiveRequestIdListMaxSize() const
{
	return MAX_ACTIVE_REQUEST_ID_COUNT;
}


unsigned UniMRCPMessage::ActiveRequestIdListSizeGet() const
{
	return static_cast<unsigned>(hdr->active_request_id_list.count);
}


void UniMRCPMessage::ActiveRequestIdListSizeSet(unsigned size)
{
	hdr->active_request_id_list.count = size;
	if (AutoAddProperty) LazyAddProperty(UW_HEADER_GENERIC_ACTIVE_REQUEST_ID_LIST);
}


UniMRCPRequestId UniMRCPMessage::ActiveRequestIdGet(unsigned index) const
{
	if (index >= MAX_ACTIVE_REQUEST_ID_COUNT) return 0;
	return hdr->active_request_id_list.ids[index];
}

void UniMRCPMessage::ActiveRequestIdSet(unsigned index, UniMRCPRequestId request) THROWS(UniMRCPException)
{
	if (index >= MAX_ACTIVE_REQUEST_ID_COUNT)
		UNIMRCP_THROW("UniMRCP active_request_id_list index out of bounds");
	hdr->active_request_id_list.ids[index] = static_cast<mrcp_request_id>(request);
	if (AutoAddProperty) LazyAddProperty(UW_HEADER_GENERIC_ACTIVE_REQUEST_ID_LIST);
}


UniMRCPResourceMessage<MRCP_RECOGNIZER>::UniMRCPResourceMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException) :
	UniMRCPResourceMessageBase<UniMRCPRecognizerHeaderId, UniMRCPRecognizerMethod, UniMRCPRecognizerEvent>(
		sess, chan, msg, autoAddProperty),
	hdr(reinterpret_cast<mrcp_recog_header_t*>(mrcp_resource_header_get(msg)))
{
	if (!hdr) UNIMRCP_THROW("Cannot access recognizer header");
}

PROCESS_RECOG_HEADERS_STR(STR_HEADER_DEF, UniMRCPResourceMessage<MRCP_RECOGNIZER>)

PROCESS_RECOG_HEADERS_BOOL(BOOL_HEADER_DEF, UniMRCPResourceMessage<MRCP_RECOGNIZER>)

PROCESS_RECOG_HEADERS_SIMPLE(SIMPLE_HEADER_DEF, UniMRCPResourceMessage<MRCP_RECOGNIZER>)


UniMRCPResourceMessage<MRCP_RECORDER>::UniMRCPResourceMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException) :
	UniMRCPResourceMessageBase<UniMRCPRecorderHeaderId, UniMRCPRecorderMethod, UniMRCPRecorderEvent>(
		sess, chan, msg, autoAddProperty),
	hdr(reinterpret_cast<mrcp_recorder_header_t*>(mrcp_resource_header_get(msg)))
{
	if (!hdr) UNIMRCP_THROW("Cannot access recorder header");
}

PROCESS_RECORDER_HEADERS_STR(STR_HEADER_DEF, UniMRCPResourceMessage<MRCP_RECORDER>)

PROCESS_RECORDER_HEADERS_BOOL(BOOL_HEADER_DEF, UniMRCPResourceMessage<MRCP_RECORDER>)

PROCESS_RECORDER_HEADERS_SIMPLE(SIMPLE_HEADER_DEF, UniMRCPResourceMessage<MRCP_RECORDER>)



UniMRCPNumericSpeechLength::UniMRCPNumericSpeechLength(UniMRCPSpeechUnit _unit, size_t _len, bool negative) :
	length(negative ? -static_cast<long>(_len) : static_cast<long>(_len)),
	unit(_unit)
{
}

/** @brief Structured voice parameter name manipulation helper */
#define voice_name    voice_param.name
/** @brief Structured voice parameter age manipulation helper */
#define voice_age     voice_param.age
/** @brief Structured voice parameter gender manipulation helper */
#define voice_gender  voice_param.gender
/** @brief Structured voice parameter variant manipulation helper */
#define voice_variant voice_param.variant

UniMRCPResourceMessage<MRCP_SYNTHESIZER>::UniMRCPResourceMessage(mrcp_session_t* sess, mrcp_channel_t* chan, mrcp_message_t* msg, bool autoAddProperty) THROWS(UniMRCPException) :
	UniMRCPResourceMessageBase<UniMRCPSynthesizerHeaderId, UniMRCPSynthesizerMethod, UniMRCPSynthesizerEvent>(
		sess, chan, msg, autoAddProperty),
	hdr(reinterpret_cast<mrcp_synth_header_t*>(mrcp_resource_header_get(msg)))
{
	if (!hdr) UNIMRCP_THROW("Cannot access synthesizer header");
}

PROCESS_SYNTH_HEADERS_STR(STR_HEADER_DEF, UniMRCPResourceMessage<MRCP_SYNTHESIZER>)

PROCESS_SYNTH_HEADERS_BOOL(BOOL_HEADER_DEF, UniMRCPResourceMessage<MRCP_SYNTHESIZER>)

PROCESS_SYNTH_HEADERS_SIMPLE(SIMPLE_HEADER_DEF, UniMRCPResourceMessage<MRCP_SYNTHESIZER>)

/**
 * @brief Define speech-length MRCP header accessors and handle lazy property addition
 * @param name  Header name (speech_length style)
 * @param NaMe  Header name (SpeechLength style)
 * @param prop  MRCP property ID (#UniMRCPSynthesizerHeaderId)
 * @param class Class the property belongs to (#UniMRCPSynthesizerMessage)
 * @see PROCESS_SYNTH_HEADERS_SPLEN
 * @see SPLEN_HEADER_FUNC_CLASS_DECL
 */
#define SPLEN_HEADER_DEF(name, NaMe, prop, class)                         \
UniMRCPSpeechLengthType class::NaMe ## TypeGet() const {                  \
	if (!CheckProperty(UW_ ## prop))                                      \
		return UW_SPEECH_LENGTH_TYPE_UNKNOWN;                             \
	else if (hdr->name.type == SPEECH_LENGTH_TYPE_NUMERIC_NEGATIVE)       \
		return UW_SPEECH_LENGTH_TYPE_NUMERIC;                             \
	else                                                                  \
		return static_cast<UniMRCPSpeechLengthType>(hdr->name.type);      \
}                                                                         \
UniMRCPNumericSpeechLength const* class::NaMe ## NumericGet() const {     \
	if (NaMe ## TypeGet() != UW_SPEECH_LENGTH_TYPE_NUMERIC)               \
		return NULL;                                                      \
	return new(msg->pool) UniMRCPNumericSpeechLength(                     \
		static_cast<UniMRCPSpeechUnit>(hdr->name.value.numeric.unit),     \
		hdr->name.value.numeric.length,                                   \
		hdr->name.type == SPEECH_LENGTH_TYPE_NUMERIC_NEGATIVE);           \
}                                                                         \
char const* class::NaMe ## TextGet() const {                              \
	if (NaMe ## TypeGet() != UW_SPEECH_LENGTH_TYPE_TEXT)                  \
		return NULL;                                                      \
	return hdr->name.value.tag.buf;                                       \
}                                                                         \
void class::NaMe ## NumericSet(long length, UniMRCPSpeechUnit unit) {     \
	hdr->name.type = length < 0 ? SPEECH_LENGTH_TYPE_NUMERIC_NEGATIVE :   \
		SPEECH_LENGTH_TYPE_NUMERIC_POSITIVE;                              \
	hdr->name.value.numeric.length = length < 0 ? -length : length;       \
	hdr->name.value.numeric.unit = static_cast<mrcp_speech_unit_e>(unit); \
	if (AutoAddProperty) LazyAddProperty(UW_ ## prop);                    \
}                                                                         \
void class::NaMe ## NumericSet(UniMRCPNumericSpeechLength const& nsl) {   \
	NaMe ## NumericSet(nsl.length, nsl.unit) ;                            \
}                                                                         \
void class::NaMe ## TextSet(char const* tag) {                            \
	hdr->name.type = SPEECH_LENGTH_TYPE_TEXT;                             \
	apt_string_assign(&hdr->name.value.tag, tag, msg->pool);              \
	if (AutoAddProperty) LazyAddProperty(UW_ ## prop);                    \
}

/* Doxygen apparently does not load definitions from header file */
#ifndef DOXYGEN
PROCESS_SYNTH_HEADERS_SPLEN(SPLEN_HEADER_DEF, UniMRCPResourceMessage<MRCP_SYNTHESIZER>)
#endif

UniMRCPProsodyRateType UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyRateTypeGet() const
{
	if (!CheckProperty(UW_HEADER_SYNTHESIZER_PROSODY_RATE))
		return UW_PROSODY_RATE_TYPE_UNKNOWN;
	return static_cast<UniMRCPProsodyRateType>(hdr->prosody_param.rate.type);
}

UniMRCPProsodyRateLabel UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyRateLabelGet() const
{
	if (ProsodyRateTypeGet() != UW_PROSODY_RATE_TYPE_LABEL)
		return UW_PROSODY_RATE_UNKNOWN;
	return static_cast<UniMRCPProsodyRateLabel>(hdr->prosody_param.rate.value.label);
}

float UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyRateRelativeGet() const
{
	return hdr->prosody_param.rate.value.relative;
}

void UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyRateLabelSet(UniMRCPProsodyRateLabel label)
{
	hdr->prosody_param.rate.value.label = static_cast<mrcp_prosody_rate_label_e>(label);
	hdr->prosody_param.rate.type = PROSODY_RATE_TYPE_LABEL;
	if (AutoAddProperty) AddProperty(UW_HEADER_SYNTHESIZER_PROSODY_RATE);
}

void UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyRateRelativeSet(float rate)
{
	hdr->prosody_param.rate.value.relative = rate;
	hdr->prosody_param.rate.type = PROSODY_RATE_TYPE_RELATIVE_CHANGE;
	if (AutoAddProperty) AddProperty(UW_HEADER_SYNTHESIZER_PROSODY_RATE);
}


UniMRCPProsodyVolumeType UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeTypeGet() const
{
	if (!CheckProperty(UW_HEADER_SYNTHESIZER_PROSODY_RATE))
		return UW_PROSODY_VOLUME_TYPE_UNKNOWN;
	return static_cast<UniMRCPProsodyVolumeType>(hdr->prosody_param.volume.type);
}

UniMRCPProsodyVolumeLabel UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeLabelGet() const
{
	if (ProsodyVolumeTypeGet() != UW_PROSODY_VOLUME_TYPE_LABEL)
		return UW_PROSODY_VOLUME_UNKNOWN;
	return static_cast<UniMRCPProsodyVolumeLabel>(hdr->prosody_param.volume.value.label);
}

float UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeNumericGet() const
{
	return hdr->prosody_param.volume.value.numeric;
}

float UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeRelativeGet() const
{
	return hdr->prosody_param.volume.value.relative;
}

void UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeLabelSet(UniMRCPProsodyVolumeLabel vol)
{
	hdr->prosody_param.volume.value.label = static_cast<mrcp_prosody_volume_label_e>(vol);
	hdr->prosody_param.volume.type = PROSODY_VOLUME_TYPE_LABEL;
	if (AutoAddProperty) AddProperty(UW_HEADER_SYNTHESIZER_PROSODY_VOLUME);
}

void UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeNumericSet(float absvol)
{
	hdr->prosody_param.volume.value.numeric = absvol;
	hdr->prosody_param.volume.type = PROSODY_VOLUME_TYPE_NUMERIC;
	if (AutoAddProperty) AddProperty(UW_HEADER_SYNTHESIZER_PROSODY_VOLUME);
}

void UniMRCPResourceMessage<MRCP_SYNTHESIZER>::ProsodyVolumeRelativeSet(float relvol)
{
	hdr->prosody_param.volume.value.relative = relvol;
	hdr->prosody_param.volume.type = PROSODY_VOLUME_TYPE_RELATIVE_CHANGE;
	if (AutoAddProperty) AddProperty(UW_HEADER_SYNTHESIZER_PROSODY_VOLUME);
}
