/* rexx */
/* $Id: cmplhelp.cmd 5447 1996-03-14 23:39:13Z mikes $
/*
 * cmplhelp.sh -- This script take the pine.help file and turns it into
 * a .c file defining lots of strings
 */

parse arg infile c_file h_file

in_text=0
count=0;
crlf=d2c(13)||d2c(10)

call stream infile, 'C', 'open read'
call stream c_file, 'C', 'open write'
call stream h_file, 'C', 'open write'

call lineout c_file, '#include <stdlib.h>'||crlf||crlf


do while lines(infile)
  line=linein(infile)
/*SAY 'read: "'line'"'*/
  select
    when substr(line,1,4) = '====' then do
      if in_text then 
        call lineout c_file, 'NULL'||crlf||'};'||crlf
      parse var line prefix string suffix
      call lineout c_file, 'char *'||string||'[] = {'
      call lineout h_file, 'extern char *'||string||'[];'
      count = count + 1
      texts.count = string
      in_text = 1
      end
    when line = '' then do
      if in_text then
        call lineout c_file, '" ",'
      end
    otherwise do
      if in_text then
        call lineout c_file, '"'||line||'",'
      end
    end
  end

call stream infile, 'C', 'close'

if in_text then
  call lineout c_file, 'NULL'||crlf||'};'||crlf
call lineout c_file, 'char **h_texts[] = {'
call lineout h_file, crlf||'extern char **h_texts[];'||crlf
do i=1 to count
  call lineout c_file, texts.i||','
  end
call lineout c_file, 'NULL'||crlf||'};'||crlf

call stream c_file, 'C', 'close'
call stream h_file, 'C', 'close'

