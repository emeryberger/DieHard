/* rexx */
/* $Id: build.cmd 5477 1996-03-15 18:01:49Z mikes $	*/
/* ------------------------------------------------------------------------ */
/*									    */
/*             T H E    P I N E    M A I L   S Y S T E M		    */
/*									    */
/*    David Nugent <davidn@unique.blaze.net.au>				    */
/*    Networks and Distributed Computing				    */
/*    Computing and Communications					    */
/*    University of Washington						    */
/*    Administration Builiding, AG-44					    */
/*    Seattle, Washington, 98195, USA					    */
/*    Internet: lgl@CAC.Washington.EDU					    */
/*              mikes@CAC.Washington.EDU				    */
/*									    */
/*    Please address all bugs and comments to "pine-bugs@cac.washington.edu"*/
/*									    */
/*									    */
/*    Pine and Pico are registered trademarks of the University of	    */
/*    Washington.  No commercial use of these trademarks may be made	    */
/*    without prior written permission of the University of Washington.     */
/*									    */
/*    Pine, Pico, and Pilot software and its included text are Copyright    */
/*    1989-1996 by the University of Washington.			    */
/*									    */
/*    The full text of our legal notices is contained in the file called    */
/*    CPYRIGHT, included with this distribution.			    */
/*									    */
/*									    */
/*    This is a build script (rexx) for PineOS2				    */
/*									    */
/*									    */
/* -------------------------------------------------------------------------*/

maketarget=''
makeargs=''
PHOME=directory()

parse arg args
n=words(args)

do while i=0 to n
    arg=word(args,i+1)

    select

        when arg = 'help' | arg = 'HELP' then do
            SAY 'Usage: build <make-options>'
            SAY ''
            SAY 'See the document doc/pine-ports for a list of other platforms that'
            SAY 'Pine has been ported to and for details about these and other ports.'
            SAY ''
            SAY '<make-options> are generally not needed. They are flags (anything'
            SAY 'beginning with -) and are passed to make. "-n" is probably the most'
            SAY 'useful, as it tells make to just print out what it is going to do and'
            SAY 'not actually do it.'
            SAY ''
            SAY 'The executables built by this are:'
            SAY ''
            SAY ' pine   The Pine mailer. Once compiled this should work just fine on'
            SAY '        your system with no other files than this binary, and no'
            SAY '        modifications to your system.'
            SAY ''
            SAY ' pico   The standalone editor similar to the Pine message composer.'
            SAY '        This is a very simple straight forward text editor.'
            SAY ''
            SAY ''
            SAY 'In general you should be able to just copy the Pine and Pico binaries'
            SAY 'to the place you keep your other local binaries.'
            SAY ''
            exit 1
            end

        when substr(arg,1,1) = '-' then do
            makeargs=makeargs arg
            end

        when arg = 'clean' | arg = 'CLEAN' then do
            maketarget='clean'
            end

        when length(arg) == 3 then do
            if maketarget <> '' then do
                SAY 'Do not specify a target - this script builds for OS/2 only'
                exit 1
                end
            end

        otherwise do
            makeargs=makeargs arg
            end
        end
    end

if maketarget = '' then maketarget='os2'
SAY 'make args="'makeargs'" target="'maketarget'"'

select

    when length(maketarget) = 3 then do
        say ''
        savedir=directory(PHOME'\c-client')
        if directory() <> PHOME'\c-client' then do
            call directory PHOME
            'move imap\ANSI\c-client c-client'
            end
        call directory PHOME
        say "Making c-client library & mtest"
        call directory PHOME'\c-client'
        'make -f makefile.'maketarget makeargs
        if rc <> 0 then SAY 'Stop.'
        else do
            say ''
            say "Making Pico"
            call directory PHOME'\pico'
            'make -f makefile.'maketarget makeargs
            if rc <> 0 then SAY 'Stop.'
            else do
                say ''
                say "Making Pine".
                call directory PHOME'\pine'
                'make -f makefile.'maketarget makeargs
                if rc <> 0 then SAY 'Stop.'
                else do
                    call directory PHOME'\bin'
                    if directory() <> PHOME'\bin' then do
                    call directory PHOME
                    'mkdir bin'
                    call directory bin
                    end
                'rm -f *.exe *.dll *.hlp *.ndx'
                'cp ../pine/pine.exe .'
                'cp ../pine/pine.hlp .'
                'cp ../pine/pine.ndx .'
                'cp ../c-client/mtest.exe .'
                'cp ../c-client/c-client.dll .'
                'cp ../pico/pico.exe .'
                'cp ../pico/pilot.exe .'
                'cp ../pico/picolib.dll .'
                say ''
                say "Copies of the executables are in bin directory:"
                'dir'
                say 'Done.'
                end
            end
        end
    call directory PHOME
    end

    when maketarget = 'clean' then do
        SAY 'Cleaning c-client and imapd'
        call directory PHOME'\c-client'
        'make clean'
        SAY 'Cleaning Pine'
        call directory PHOME'\pine'
        'make -f makefile.ult clean'
        SAY 'Cleaning pico'
        call directory PHOME'\pico'
        'make' makeargs '-f makefile.ult clean'
        SAY 'Done.'
        call directory PHOME
        end

    when maketarget = '' then do
        SAY 'No target plaform for which to build Pine given.'
        SAY 'Give command "build help" for help.'
        end

    otherwise do
        SAY 'Do not know how to make Pine for target "'maketarget'".'
        end

    end

exit 0
