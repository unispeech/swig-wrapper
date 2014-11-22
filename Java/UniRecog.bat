@ECHO OFF
SETLOCAL

:: Add library directories to path
SET PATH=%PATH%;..\..\..\..\trunk\Release\bin;..\..\..\..\trunk\Debug\bin
SET PATH=%PATH%;..\..\..\..\UniMRCP\Release\bin;..\..\..\..\UniMRCP\Debug\bin

:: Find JDK binaries
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
IF NOT "%JAVA_HOME%"=="" (
	SET JAVA=%JAVA_HOME%\bin\java.exe
	SET JAVAC=%JAVA_HOME%\bin\javac.exe
	IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
)
SET JAVA=java.exe
SET JAVAC=javac.exe
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
CALL :try_jdk "%ProgramFiles(x86)%"
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
CALL :try_jdk "%ProgramFiles%"
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
CALL :try_jdk "%ProgramW6432%"
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
CALL :try_jdk "C:\Program Files (x86)"
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
CALL :try_jdk "C:\Program Files"
IF EXIST "%JAVA%" IF EXIST "%JAVAC%" GOTO :jdk_found
ECHO Java JDK (java.exe and javac.exe) not found.
ECHO Add it to PATH or set their location to
ECHO JAVA and JAVAC environment variables respectively.
EXIT /B 1

:jdk_found
ECHO Using Java compiler: %JAVAC%
ECHO Press enter to compile UniRecog
PAUSE
"%JAVAC%" -classpath . UniRecog.java
IF ERRORLEVEL 1 (
	ECHO Error compiling UniRecog
	EXIT /B 1
)
ECHO Press enter to run UniRecog %*
PAUSE
"%JAVA%" -classpath . -Djava.library.path=Release;RelWithDebInfo;MinSizeRel;Debug;. UniRecoog %*
EXIT /B %ERRORLEVEL%

:try_jdk
FOR /D %%d IN ("%~1\Java\jdk*") DO (
	IF EXIST "%%d\bin\java.exe" IF EXIST "%%d\bin\javac.exe" (
		SET JAVA=%%d\bin\java.exe
		SET JAVAC=%%d\bin\javac.exe
		EXIT /B 0
	)
)
