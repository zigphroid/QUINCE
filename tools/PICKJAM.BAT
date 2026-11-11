@ECHO OFF
REM PICKJAM OWNER/REPO NAME: fetch NAME.JAM over the WiFi from the dist
REM folder of the GitHub repo OWNER/REPO, its main branch, into J:\JAMS.
REM make dist puts the program there; the repo must be public. GitHub
REM sends PICK on to raw.githubusercontent.com. It's https all the way:
REM a plain http hop would let the network hand back any program.
IF "%2"=="" GOTO USAGE
PICK /O https://github.com/%1/raw/main/dist/%2.JAM J:\JAMS\%2.NEW
IF ERRORLEVEL 1 GOTO FAILED
REM Only a whole download replaces the copy that works.
IF EXIST J:\JAMS\%2.JAM COMPOST J:\JAMS\%2.JAM
RELABEL J:\JAMS\%2.NEW %2.JAM
IF EXIST J:\JAMS\%2.NEW GOTO BUSY
ECHO %2 is fresh in J:\JAMS. Type %2 to run it.
GOTO END
:USAGE
ECHO PICKJAM OWNER/REPO NAME, the name in capitals: PICKJAM SAM/PEACH PEACH
GOTO END
:BUSY
ECHO Couldn't replace J:\JAMS\%2.JAM (is it running in another burner?); the new one is %2.NEW.
GOTO END
:FAILED
IF EXIST J:\JAMS\%2.NEW COMPOST J:\JAMS\%2.NEW
ECHO Couldn't pick %2. (is the repo public, dist\%2.JAM merged into main, and the name in capitals?)
:END
