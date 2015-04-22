@ECHO OFF
SETLOCAL

:: Add library directories to path
SET PATH=%PATH%;Release;RelWithDebInfo;MinSizeRel;Debug
SET PATH=%PATH%;..\..\..\..\trunk\Release\bin;..\..\..\..\trunk\Debug\bin
SET PATH=%PATH%;..\..\..\..\UniMRCP\Release\bin;..\..\..\..\UniMRCP\Debug\bin

UniRecog_Cpp %*
