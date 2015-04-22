@ECHO OFF
SETLOCAL

:: Add library directories to path
SET PATH=%PATH%;Release;RelWithDebInfo;MinSizeRel;Debug
SET PATH=%PATH%;..\..\..\..\trunk\Release\bin;..\..\..\..\trunk\Debug\bin
SET PATH=%PATH%;..\..\..\..\UniMRCP\Release\bin;..\..\..\..\UniMRCP\Debug\bin

:: Find .NET C# compiler
IF EXIST "%CSC%" GOTO :csc_found
SET CSC=csc.exe
IF EXIST "%CSC%" GOTO :csc_found
:: Try various .NET versions
CALL :try_csc 4.5.50938
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 4.5.50709
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 4.0.30319
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 3.5.21022
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 3.0.4506
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 2.0.50727
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 1.1.4322
IF EXIST "%CSC%" GOTO :csc_found
CALL :try_csc 1.0.3705
IF EXIST "%CSC%" GOTO :csc_found
ECHO Microsoft .NET C# compiler (csc.exe) not found.
ECHO Add it to PATH or set its location to CSC environment variable.
EXIT /B 1


:csc_found
ECHO Using C# compiler: %CSC%
ECHO Press enter to compile UniRecog
PAUSE
"%CSC%" /platform:x86 UniRecog.cs wrapper\*.cs
IF ERRORLEVEL 1 (
	ECHO Error compiling UniRecog
	EXIT /B 1
)
ECHO Press enter to run UniRecog %*
PAUSE
UniRecog %*
EXIT /B 0


:try_csc
IF EXIST "%windir%\Microsoft.NET\Framework\v%1\csc.exe" (
	SET CSC="%windir%\Microsoft.NET\Framework\v%1\csc.exe"
	GOTO :EOF
)
IF EXIST "%windir%\Microsoft.NET\Framework64\v%1\csc.exe" (
	SET CSC="%windir%\Microsoft.NET\Framework64\v%1\csc.exe"
	GOTO :EOF
)
GOTO :EOF
