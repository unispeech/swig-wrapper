#! /bin/sh

find_mono() {
	if ! [ -z "$MONO" ] && [ -x "$MONO" ]; then
		return 0;
	fi;
	MONO=`which mono`;
	if ! [ -z "$MONO" ] && [ -x "$MONO" ]; then
		return 0;
	fi;
	MONO=`which mint`;
	if ! [ -z "$MONO" ] && [ -x "$MONO" ]; then
		return 0;
	fi;
	return 1;
}

find_csc() {
	if ! [ -z "$CSC" ] && [ -x "$CSC" ]; then
		return 0;
	fi;
	CSC=`which mcs`;
	if ! [ -z "$CSC" ] && [ -x "$CSC" ]; then
		return 0;
	fi;
	CSC=`which gmcs`;
	if ! [ -z "$CSC" ] && [ -x "$CSC" ]; then
		return 0;
	fi;
	CSC=`which dmcs`;
	if ! [ -z "$CSC" ] && [ -x "$CSC" ]; then
		return 0;
	fi;
	return 1;
}

if ! find_mono; then
	echo "Mono not found.";
	echo "If installed, add it to the PATH or set MONO variable.";
	exit 1;
fi;

if ! find_csc; then
	echo "C# compiler not found.";
	echo "If installed, add it to the PATH or set CSC variable.";
	exit 1;
fi;

echo "Mono runtime: $MONO";
echo "Using C# compiler: $CSC";
read -p "Press enter to compile UniSynth" x;
$CSC UniSynth.cs wrapper/*.cs
if [ $? -ne 0 ]; then
	echo "Error compiling UniSynth";
	exit $?;
fi;

read -p "Press enter to run UniSynth $*" x;
$MONO UniSynth.exe $*
