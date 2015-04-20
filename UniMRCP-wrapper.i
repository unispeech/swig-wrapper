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

%module(directors="1") UniMRCP
%{
#include "UniMRCP-wrapper.h"
%}

%feature("director") UniMRCPLogger;
%feature("director") UniMRCPClientSession;
%feature("director") UniMRCPClientChannel;
%feature("director") UniMRCPClientResourceChannel;
%feature("director") UniMRCPAudioTermination;
%feature("director") UniMRCPStreamRx;
%feature("director") UniMRCPStreamRxBuffered;
%feature("director") UniMRCPStreamRxMemory;
%feature("director") UniMRCPStreamRxFile;
%feature("director") UniMRCPStreamTx;

%feature("nodirector") UniMRCPStreamRxBuffered::ReadFrame;
%feature("nodirector") UniMRCPStreamRxMemory::ReadFrame;
%feature("nodirector") UniMRCPStreamRxMemory::Close;
%feature("nodirector") UniMRCPStreamRxFile::ReadFrame;
%feature("nodirector") UniMRCPStreamRxFile::Close;

%ignore TARGET_PLATFORM;
%ignore swig_target_platform;
%ignore unimrcp_client_app_name;
%ignore unimrcp_client_log_name;
%ignore FrameBuffer;
%ignore operator new;
%ignore operator delete;

#if !defined(SWIG)
#elif defined(SWIGCSHARP)

	%typemap(ctype,  out="unsigned short*") unsigned short& "unsigned short*"
	%typemap(imtype, out="IntPtr")          unsigned short& "out ushort"
	%typemap(cstype, out="$csclassname")    unsigned short& "out ushort"
	%typemap(csin)                          unsigned short& "out $csinput"
	%typemap(in)                            unsigned short& %{ $1 = $input; %}

#	ifdef SAFE_ARRAYS
		%typemap(ctype)  void INPUT[] "void*"
		%typemap(cstype) void INPUT[] "byte[]"
		%typemap(imtype,
		         inattributes="[In, MarshalAs(UnmanagedType.LPArray)]")
		                 void INPUT[] "byte[]"
		%typemap(csin)   void INPUT[] "$csinput"

		%typemap(ctype)  void OUTPUT[] "void*"
		%typemap(cstype) void OUTPUT[] "byte[]"
		%typemap(imtype,
		         inattributes="[Out, MarshalAs(UnmanagedType.LPArray)]")
		                 void OUTPUT[] "byte[]"
		%typemap(csin)   void OUTPUT[] "$csinput"

		%apply void INPUT[] {void const*}
		%apply void OUTPUT[] {void*}
#	else  // SAFE_ARRAYS
		%typemap(ctype)   void FIXED[] "void*"
		%typemap(imtype)  void FIXED[] "IntPtr"
		%typemap(cstype)  void FIXED[] "byte[]"
		%typemap(csin, pre="    fixed (byte* swig_ptrTo_$csinput = $csinput)")
		                  void FIXED[] "(IntPtr)swig_ptrTo_$csinput"
		%apply void FIXED[] {void*}

		// Hack to allow pinning in the constructor
		%typemap(imtype, out="unsafe IntPtr")  UniMRCPStreamRxMemory* "HandleRef"

		%csmethodmodifiers UniMRCPStreamRx::SetData "public unsafe"
		%csmethodmodifiers UniMRCPStreamRxBuffered::AddData "public unsafe"
		%csmethodmodifiers UniMRCPStreamRxMemory::SetMemory "public unsafe"
		%csmethodmodifiers UniMRCPStreamTx::GetData "public unsafe"
		%csmethodmodifiers UniMRCPMessage::GetBody "public unsafe"
		%csmethodmodifiers UniMRCPMessage::SetBody "public unsafe"
#	endif  // SAFE_ARRAYS_ELSE

	%typemap(cscode) UniMRCPStreamRx %{
	public void SetData(byte[] buf) {SetData(buf, (uint)buf.Length);}
	%}
	%typemap(cscode) UniMRCPStreamRxBuffered %{
	public void AddData(byte[] buf) {AddData(buf, (uint)buf.Length);}
	%}
	%typemap(cscode) UniMRCPStreamRxMemory %{
	public UniMRCPStreamRxMemory(byte[] mem, bool copy, UniMRCPStreamRxMemory.StreamRxMemoryEnd onend) : this(mem, (uint)mem.Length, copy, onend) {}
	public UniMRCPStreamRxMemory(byte[] mem, bool copy) : this(mem, (uint)mem.Length, copy) {}
	public UniMRCPStreamRxMemory(byte[] mem) : this(mem, (uint)mem.Length) {}

	public void SetMemory(byte[] mem, bool copy, UniMRCPStreamRxMemory.StreamRxMemoryEnd onend) {SetMemory(mem, (uint)mem.Length, copy, onend);}
	public void SetMemory(byte[] mem, bool copy) {SetMemory(mem, (uint)mem.Length, copy);}
	public void SetMemory(byte[] mem) {SetMemory(mem, (uint)mem.Length);}
	%}
	%typemap(cscode) UniMRCPStreamTx %{
	public void GetData(byte[] buf) {GetData(buf, (uint)buf.Length);}
	%}
	%typemap(cscode) UniMRCPMessage %{
	public void GetBody(byte[] buf) {GetBody(buf, (uint)buf.Length);}
	public void SetBody(byte[] buf) {SetBody(buf, (uint)buf.Length);}
	%}

	%ignore UniMRCPException;
	%typemap(throws, canthrow=1) UniMRCPException {
		SWIG_CSharpSetPendingException(SWIG_CSharpApplicationException, $1.msg);
		return $null;
	}

#elif defined(SWIGPYTHON)  // SWIGCSHARP

	%apply unsigned short& OUTPUT {unsigned short& major, unsigned short& minor, unsigned short& patch};

/*
	%include "pybuffer.i"
	%pybuffer_binary(char const* buf, size_t len)
	%apply (char const* buf, size_t len) {(void const* buf, size_t len)}
	%pybuffer_mutable_binary(char* buf, size_t len)
*/
	%typemap(in) (void const* buf, size_t len)
			(int res, Py_ssize_t size = 0, const void *buff = 0) {
		res = PyObject_AsReadBuffer($input, &buff, &size);
		if (res<0) {
			PyErr_Clear();
			%argument_fail(res, "(void const* buf, size_t len)", $symname, $argnum);
		}
		$1 = ($1_ltype) buff;
		$2 = ($2_ltype) size;
	}
	%apply (void const* buf, size_t len) { (void const* mem, size_t size) }

	%typemap(in) (void* buf, size_t len)
			(int res, Py_ssize_t size = 0, void *buff = 0) {
		res = PyObject_AsWriteBuffer($input, &buff, &size);
		if (res<0) {
			PyErr_Clear();
			%argument_fail(res, "(void* bbuf, size_t len)", $symname, $argnum);
		}
		$1 = ($1_ltype) buff;
		$2 = ($2_ltype) size;
	}

	%ignore UniMRCPException;
	%typemap(throws, canthrow=1) UniMRCPException {
		PyErr_SetString(PyExc_RuntimeError, $1.msg);
		return NULL;
	}
	%{
	#define APT_PRIO_EMERGENCY PyAPT_PRIO_EMERGENCY
	#define APT_PRIO_ALERT     PyAPT_PRIO_ALERT
	#define APT_PRIO_CRITICAL  PyAPT_PRIO_CRITICAL
	#define APT_PRIO_ERROR     PyAPT_PRIO_ERROR
	#define APT_PRIO_WARNING   PyAPT_PRIO_WARNING
	#define APT_PRIO_NOTICE    PyAPT_PRIO_NOTICE
	#define APT_PRIO_INFO      PyAPT_PRIO_INFO
	#define APT_PRIO_DEBUG     PyAPT_PRIO_DEBUG
	#define APT_LOG_OUTPUT_NONE    PyAPT_LOG_OUTPUT_NONE
	#define APT_LOG_OUTPUT_CONSOLE PyAPT_LOG_OUTPUT_CONSOLE
	#define APT_LOG_OUTPUT_FILE    PyAPT_LOG_OUTPUT_FILE
	#define APT_LOG_OUTPUT_BOTH    PyAPT_LOG_OUTPUT_BOTH
	#include "apt_log.h"
	#undef APT_PRIO_EMERGENCY
	#undef APT_PRIO_ALERT
	#undef APT_PRIO_CRITICAL
	#undef APT_PRIO_ERROR
	#undef APT_PRIO_WARNING
	#undef APT_PRIO_NOTICE
	#undef APT_PRIO_INFO
	#undef APT_PRIO_DEBUG
	#undef APT_LOG_OUTPUT_NONE
	#undef APT_LOG_OUTPUT_CONSOLE
	#undef APT_LOG_OUTPUT_FILE
	#undef APT_LOG_OUTPUT_BOTH
	static void log_python_exception() {
		PyObject *ptype, *pvalue, *ptraceback;
		PyObject *sptype = NULL, *spvalue = NULL, *sptraceback = NULL;
		const char *stype, *svalue, *straceback;
		PyErr_Fetch(&ptype, &pvalue, &ptraceback);
		if (ptype && (sptype = PyObject_Str(ptype)))
			stype = PyString_AS_STRING(sptype);
		else
			stype = "";
		if (pvalue && (spvalue = PyObject_Str(pvalue)))
			svalue = PyString_AS_STRING(spvalue);
		else
			svalue = "";
		if (ptraceback && (sptraceback = PyObject_Str(ptraceback)))
			straceback = PyString_AS_STRING(sptraceback);
		else
			straceback = "";
		apt_log(APT_LOG_MARK, PyAPT_PRIO_CRITICAL, "Python exception "
			"Type(%s) Value(%s) Traceback(%s)", stype, svalue, straceback);
		Py_XDECREF(sptype);
		Py_XDECREF(spvalue);
		Py_XDECREF(sptraceback);
		PyErr_Restore(ptype, pvalue, ptraceback);
	}
	%}
	%feature("director:except") {
		if ($error != NULL) {
			log_python_exception();
 			PyErr_Print();
		}
	}

#elif defined(SWIGJAVA)  // SWIGPYTHON

	%include "enums.swg"
	%javaconst(1);

	%include <typemaps.i>
	%apply unsigned short& OUTPUT {unsigned short& major, unsigned short& minor, unsigned short& patch};

	%apply (char *STRING, size_t LENGTH) { (void const* mem, size_t size) }
	%apply (char *STRING, size_t LENGTH) { (void const* buf, size_t len) }
	%apply (char *STRING, size_t LENGTH) { (void* buf, size_t len) }

	%ignore UniMRCPException;
	%typemap(throws, canthrow=1) UniMRCPException {
		jclass clazz = jenv->FindClass("java/lang/Exception");
		jenv->ThrowNew(clazz, $1.msg);
		return $null;
	}

#else  // SWIGJAVA
#	warning "Arrays passing or exceptions probably not implemented for current target platform (" TARGET_PLATFORM ")!"
#endif  // SWIG

%include "UniMRCP-wrapper.h"

%template(UniMRCPSynthesizerMessageBase) UniMRCPResourceMessageBase<UniMRCPSynthesizerHeaderId, UniMRCPSynthesizerMethod, UniMRCPSynthesizerEvent>;
%template(UniMRCPSynthesizerMessage) UniMRCPResourceMessage<MRCP_SYNTHESIZER>;
%template(UniMRCPRecognizerMessageBase) UniMRCPResourceMessageBase<UniMRCPRecognizerHeaderId, UniMRCPRecognizerMethod, UniMRCPRecognizerEvent>;
%template(UniMRCPRecognizerMessage) UniMRCPResourceMessage<MRCP_RECOGNIZER>;
%template(UniMRCPRecorderMessageBase) UniMRCPResourceMessageBase<UniMRCPRecorderHeaderId, UniMRCPRecorderMethod, UniMRCPRecorderEvent>;
%template(UniMRCPRecorderMessage) UniMRCPResourceMessage<MRCP_RECORDER>;
%template(UniMRCPSynthesizerChannel) UniMRCPClientResourceChannel<MRCP_SYNTHESIZER>;
%template(UniMRCPRecognizerChannel) UniMRCPClientResourceChannel<MRCP_RECOGNIZER>;
%template(UniMRCPRecorderChannel) UniMRCPClientResourceChannel<MRCP_RECORDER>;
