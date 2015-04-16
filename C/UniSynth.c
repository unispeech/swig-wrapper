#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "apr_general.h"
#include "apr_thread_mutex.h"
#include "apr_thread_cond.h"
#include "apt_pool.h"
#include "apt_dir_layout.h"
#include "apt_log.h"
#include "unimrcp_client.h"
#include "mrcp_application.h"
#include "mrcp_message.h"
#include "mrcp_generic_header.h"
#include "mrcp_synth_header.h"
#include "mrcp_synth_resource.h"

/* UniMRCP client root directory */
static char const ROOT_DIR1[] = "../../../../trunk";
static char const ROOT_DIR2[] = "../../../../UniMRCP";
static char const ROOT_DIR3[] = "../../../../unimrcp";
static char const*ROOT_DIR = ROOT_DIR1;
/* UniMRCP profile to use for communication with server */
static char const MRCP_PROFILE[] = "uni1";
/* File to write synthesized text to (ROOT_DIR/data/PCM_OUT_FILE) */
static char const PCM_OUT_FILE[] = "UniSynth.pcm";

/* Nasty global variables */
static int err = -1;  /* >=0 means finished */
/* Set to TRUE upon SPEAK's IN-PROGRESS */
static apt_bool_t stream_started = FALSE;
/* Output file descriptor */
static FILE* f = NULL;
/* Text to synthesize */
static char const* text = NULL;
/* Wait objects */
apr_thread_mutex_t* mutex = NULL;
apr_thread_cond_t* cond = NULL;

#define MAX_LOG_ENTRY_SIZE 512


/* User-defined logger */
int UniSynth_logger(char const* file, int line, char const* id, apt_log_priority_e priority, char const* format, va_list arg_ptr)
{
	char buf[MAX_LOG_ENTRY_SIZE];
	(void) file;
	(void) line;
	(void) id;
	(void) priority;
	buf[0] = 0;
	apr_vsnprintf(buf, MAX_LOG_ENTRY_SIZE, format, arg_ptr);
	printf("  %s\n", buf);
	return TRUE;
}


static apt_bool_t OnSessionTerminate(mrcp_application_t* application, mrcp_session_t* session, mrcp_sig_status_code_e status)
{
	(void) application;
	printf("Session terminted with code %d\n", status);
	mrcp_application_session_destroy(session);
	return TRUE;
}


static apt_bool_t OnTerminateEvent(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel)
{
	(void) application;
	(void) channel;
	puts("Session terminted unexpectedly");
	mrcp_application_session_destroy(session);
	return TRUE;
}


/* Called when an audio frame arrives */
apt_bool_t OnStreamWriteFrame(mpf_audio_stream_t* stream, const mpf_frame_t* frame)
{
	(void) stream;
	if (!stream_started)
		return FALSE;
	if (!(frame->type & MEDIA_FRAME_TYPE_AUDIO))
		return FALSE;
	fwrite(frame->codec_frame.buffer, 1, frame->codec_frame.size, f);
	return TRUE;
}


/* Shorthand for graceful fail: Write message, release semaphore and return FALSE */
static apt_bool_t sess_failed(char const* msg)
{
	puts(msg);
	err = 1;
	apr_thread_cond_signal(cond);
	return FALSE;
}


/* MRCP connection established, start communication */
static apt_bool_t OnChannelAdd(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, mrcp_sig_status_code_e status)
{
	mrcp_message_t* msg;
	mrcp_generic_header_t* hdr;
	(void) application;
	if (status != MRCP_SIG_STATUS_CODE_SUCCESS)
		return sess_failed("Failed to add channel");
	/* Start processing here */
	msg = mrcp_application_message_create(session, channel, SYNTHESIZER_SPEAK);
	hdr = mrcp_generic_header_get(msg);
	apt_string_set(&hdr->content_type, "text/plain");
	mrcp_generic_header_property_add(msg, GENERIC_HEADER_CONTENT_TYPE);
	apt_string_set(&msg->body, text);
	return mrcp_application_message_send(session, channel, msg);
}


/* Response or event from the server arrived */
static apt_bool_t OnMessageReceive(mrcp_application_t* application, mrcp_session_t* session, mrcp_channel_t* channel, mrcp_message_t* message)
{
	(void) application;
	(void) session;
	(void) channel;
	/* Analyze, update your application state and reply messages here */
	if (message->start_line.message_type == MRCP_MESSAGE_TYPE_RESPONSE) {
		if (message->start_line.status_code != MRCP_STATUS_CODE_SUCCESS)
			return sess_failed("SPEAK request failed");
		if (message->start_line.request_state != MRCP_REQUEST_STATE_INPROGRESS)
			return sess_failed("Failed to start SPEAK processing");
		/* Start writing audio to file */
		stream_started = TRUE;
		return TRUE;  /* Does not actually matter */
	}
	if (message->start_line.message_type != MRCP_MESSAGE_TYPE_EVENT)
		return sess_failed("Unexpected message from the server");
	if (message->start_line.method_id == SYNTHESIZER_SPEAK_COMPLETE) {
		mrcp_synth_header_t* hdr = (mrcp_synth_header_t*) mrcp_resource_header_get(message);
		printf("Speak complete: %d %.*s", hdr->completion_cause,
			(int) hdr->completion_reason.length, hdr->completion_reason.buf);
		stream_started = FALSE;
		err = 0;
		apr_thread_cond_signal(cond);
		return TRUE;  /* Does not actually matter */
	}
	return sess_failed("Unknown message recived");
}


static apt_bool_t UniSynthAppMsgHandler(mrcp_app_message_t const* msg)
{
	static const mrcp_app_message_dispatcher_t appDisp =
	{
		NULL,  // Update
		OnSessionTerminate,
		OnChannelAdd,
		NULL,  /* OnChannelRemove */
		OnMessageReceive,
		OnTerminateEvent,
		NULL   /* Resource discover */
	};
	return mrcp_application_message_dispatch(&appDisp, msg);
}

static const mpf_audio_stream_vtable_t stream_vtable = {
	NULL,  /* Destroy */
	NULL,  /* OpenRx */
	NULL,  /* CloseRx */
	NULL,  /* ReadFrame */
	NULL,  /* OpenTx */
	NULL,  /* CloseTx */
	OnStreamWriteFrame,
	NULL   /* Trace */
};


#define FAIL(msg) {  \
	puts(msg);       \
	err = 1;         \
	goto cleanup;    \
}

int main(int argc, char const* argv[])
{
	apr_pool_t* pool = NULL;
	apr_pool_t* spool = NULL;
	int i;
	struct iovec cattext[101];
	static char const SP = ' ';
	char const* outfile;
	apr_status_t status;
	apt_dir_layout_t* dirLayout = NULL;
	mrcp_client_t* client = NULL;
	mrcp_application_t* app = NULL;
	mrcp_session_t* sess = NULL;
	mpf_stream_capabilities_t* caps = NULL;
	mpf_termination_t* term = NULL;
	mrcp_channel_t* chan = NULL;
	struct stat info;

	if (argc < 2) {
		puts("Usage:");
		printf("\t%s \"This is a synthetic voice.\"", argv[0]);
		exit(1);
	}

	/* Just detect various directory layout constellations */
	if (stat(ROOT_DIR, &info))
		ROOT_DIR = ROOT_DIR2;
	if (stat(ROOT_DIR, &info))
		ROOT_DIR = ROOT_DIR3;

	/* Initialize platform first */
	if (apr_initialize() != APR_SUCCESS) FAIL("Cannot initialize APR platform");
	pool = apt_pool_create();
	if (!pool) FAIL("Not enough memory");
	for (i = 0; (i < argc - 2) && (i < 50); i += 2) {
		cattext[2 * i].iov_base = (void*) argv[i + 1];
		cattext[2 * i].iov_len = strlen(argv[i + 1]);
		cattext[2 * i + 1].iov_base = (void*) &SP;
		cattext[2 * i + 1].iov_len = 1;
	}
	cattext[2 * i].iov_base = (void*) argv[i + 1];
	cattext[2 * i].iov_len = strlen(argv[i + 1]);
	text = apr_pstrcatv(pool, cattext, 2 * i + 1, NULL);
	if (!text) FAIL("Not enough memory");
	outfile = apr_pstrcat(pool, ROOT_DIR, "/data/", PCM_OUT_FILE, NULL);
	
	printf("This is a sample C UniMRCP client synthesizer scenario.\n");
	printf("Use client configuration from %s/conf/unimrcpclient.xml\n", ROOT_DIR);
	printf("Use profile %s\n", MRCP_PROFILE);
	printf("Synthesize text: `%s'\n", text);
	printf("Write output to file: %s\n", outfile);
	printf("\n");
	printf("Press enter to start the session...\n");
	(void) getchar();

	apt_log_instance_create(APT_LOG_OUTPUT_NONE, APT_PRIO_DEBUG, pool);
	apt_log_ext_handler_set(UniSynth_logger);
	dirLayout = apt_default_dir_layout_create(ROOT_DIR, pool);

	/* Create and start the client in a root dir */
	client = unimrcp_client_create(dirLayout);
	if (!client) FAIL("Cannot create UniMRCP client");
	app = mrcp_application_create(UniSynthAppMsgHandler, NULL, mrcp_client_memory_pool_get(client));
	if (!app) FAIL("Cannot create MRCP application");
	if (!mrcp_client_application_register(client, app, "Sample C app"))
		FAIL("Cannot register MRCP application");
	if (!mrcp_client_start(client)) FAIL("Cannot start MRCP client");

	/* Create a session using MRCP profile MRCP_PROFILE */
	sess = mrcp_application_session_create(app, MRCP_PROFILE, NULL);
	if (!sess) FAIL("Cannot create session");
	spool = mrcp_application_session_pool_get(sess);
	/* Create audio termination with capabilities */
	caps = mpf_stream_capabilities_create(STREAM_DIRECTION_SEND, spool);
	if (!caps) FAIL("Error creating capabilities");
	if (!mpf_codec_capabilities_add(&caps->codecs, MPF_SAMPLE_RATE_8000, "LPCM"))
		FAIL("Error adding codec capabilities");
	term = mrcp_application_audio_termination_create(sess, &stream_vtable, caps, NULL);
	if (!term) FAIL("Cannot create audio termination");
	/* Add signaling channel (and start processing in OnAdd method */
	f = fopen(outfile, "wb");
	if (!f) FAIL("Cannot open output file");
	status = apr_thread_mutex_create(&mutex, APR_THREAD_MUTEX_DEFAULT, pool);
	if (status != APR_SUCCESS) FAIL("Cannot create mutex");
	status = apr_thread_cond_create(&cond, pool);
	if (status != APR_SUCCESS) FAIL("Cannot create condition variable");
	chan = mrcp_application_channel_create(sess, MRCP_SYNTHESIZER_RESOURCE, term, NULL, NULL);
	if (!chan) FAIL("Cannot create channel");
	if (!mrcp_application_channel_add(sess, chan))
		FAIL("Cannot add channel");

	/* Now wait until the processing finishes */
	apr_thread_mutex_lock(mutex);
	while (err < 0) apr_thread_cond_wait(cond, mutex);
	apr_thread_mutex_unlock(mutex);

cleanup:
	if (sess) mrcp_application_session_terminate(sess);
	if (f) fclose(f);
	if (client) mrcp_client_shutdown(client);
	if (app) mrcp_application_destroy(app);
	if (client) mrcp_client_destroy(client);
	apt_log_instance_destroy();
	if (pool) apr_pool_destroy(pool);
	apr_terminate();
	puts("Program finished, memory released. Press any key to exit.");
	(void) getchar();
	return err;
}
