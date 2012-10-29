#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: init.c 14080 2005-09-12 18:53:17Z hubert@u.washington.edu $";
#endif
/*----------------------------------------------------------------------

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"


   Pine and Pico are registered trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior written
   permission of the University of Washington.

   Pine, Pico, and Pilot software and its included text are Copyright
   1989-2005 by the University of Washington.

   The full text of our legal notices is contained in the file called
   CPYRIGHT, included with this distribution.


   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
     init.c
     Routines for pine start up and initialization
       init_vars
       free_vars
       init_username
       init_hostname
       read_pinerc
       write_pinerc
       init_mail_dir

       sent-mail expiration
 
       open debug files 

      - open and set up the debug files
      - fetch username, password, and home directory
      - get host and domain name
      - read and write the users .pinerc config file
      - create the "mail" subdirectory
      - expire old sent-mail

  ====*/


#include "headers.h"
#include "../c-client/imap4r1.h"  /* for LEVELSTATUS() */


typedef enum {Sapling, Seedling, Seasoned} FeatureLevel;

#define	TO_BAIL_THRESHOLD	60

#define METASTR "\nremote-abook-metafile="
static char meta_prefix[] = ".ab";

/* data stored in a line in the metadata file */
typedef struct remote_data_meta {
    char         *local_cache_file;
    unsigned long uidvalidity;
    unsigned long uidnext;
    unsigned long uid;
    unsigned long nmsgs;
    char          read_status;	/* 'R' for readonly, 'W' for readwrite */
    char         *date;
} REMDATA_META_S;


/*
 * Internal prototypes
 */
void	 read_pinerc PROTO((PINERC_S *, struct variable *, ParsePinerc));
int	 compare_sm_files PROTO((const QSType *, const QSType *));
/* AIX gives warning here 'cause it can't quite cope with enums */
void	 process_init_cmds PROTO((struct pine *, char **));
void	 process_feature_list PROTO((struct pine *, char **, int , int, int));
void     display_init_err PROTO((char *, int));
void     rd_grope_for_metaname PROTO((void));
char    *rd_metadata_name PROTO((void));
REMDATA_META_S *rd_find_our_metadata PROTO((char *, unsigned long *));
int      rd_init_remote PROTO((REMDATA_S *, int));
int      rd_add_hdr_msg PROTO((REMDATA_S *, char *));
int      rd_store_fake_hdrs PROTO((REMDATA_S *, char *, char *, char *));
int      rd_chk_for_hdr_msg PROTO((MAILSTREAM **, REMDATA_S *, char **));
int      rd_meta_is_broken PROTO((FILE *));
char    *rd_derived_cachename PROTO((char *));
int      rd_upgrade_cookies PROTO((REMDATA_S *, long, int));
int      copy_localfile_to_remotefldr PROTO((RemType, char *, char *, void *,
					     char **));
char    *skip_over_this_var PROTO((char *, char *));
int      var_is_in_rest_of_file PROTO((char *, char *));
char    *read_remote_pinerc PROTO((PINERC_S *, ParsePinerc));
void     set_current_pattern_vals PROTO((struct pine *));
void     convert_pattern_data PROTO((void));
void     convert_filts_pattern_data PROTO((void));
void     convert_scores_pattern_data PROTO((void));
void     convert_pinerc_patterns PROTO((long));
void     convert_pinerc_filts_patterns PROTO((long));
void     convert_pinerc_scores_patterns PROTO((long));
int      add_to_pattern PROTO((PAT_S *, long));
void     convert_pinerc_to_remote PROTO((struct pine *, char *));
char    *native_nl PROTO((char *));
int      rd_check_for_suspect_data PROTO((REMDATA_S *));
int      rd_prompt_about_forged_remote_data PROTO((int, REMDATA_S *, char *));
int      rd_answer_forge_warning PROTO((int, MSGNO_S *, SCROLL_S *));


#if	defined(DOS_EXTRA) && !(defined(WIN32) || defined (_WINDOWS))
#define	CONF_TXT_T	char __based(__segname("_CNFT"))
#else
#define	CONF_TXT_T	char
#endif

/*------------------------------------
Some definitions to keep the static "variable" array below 
a bit more readable...
  ----*/
CONF_TXT_T cf_text_comment[] = "#\n# Pine configuration file\n#\n# This file sets the configuration options used by Pine and PC-Pine. These\n# options are usually set from within Pine or PC-Pine. There may be a\n# system-wide configuration file which sets the defaults for some of the\n# variables. On Unix, run pine -conf to see how system defaults have been set.\n# For variables that accept multiple values, list elements are\
 separated by\n# commas. A line beginning with a space or tab is considered to be a\n# continuation of the previous line. For a variable to be unset its value must\n# be blank. To set a variable to the empty string its value should be \"\".\n# You can override system defaults by setting a variable to the empty string.\n# Lines beginning with \"#\" are comments, and ignored by Pine.\n";


CONF_TXT_T cf_text_personal_name[] =	"Over-rides your full name from Unix password file. Required for PC-Pine.";

CONF_TXT_T cf_text_user_id[] =		"Your login/e-mail user name";

CONF_TXT_T cf_text_user_domain[] =		"Sets domain part of From: and local addresses in outgoing mail.";

CONF_TXT_T cf_text_smtp_server[] =		"List of SMTP servers for sending mail. If blank: Unix Pine uses sendmail.";

CONF_TXT_T cf_text_nntp_server[] =		"NNTP server for posting news. Also sets news-collections for news reading.";

#ifdef	ENABLE_LDAP
CONF_TXT_T cf_text_ldap_server[] =		"LDAP servers for looking up addresses.";
#endif	/* ENABLE_LDAP */

CONF_TXT_T cf_text_inbox_path[] =		"Path of (local or remote) INBOX, e.g. ={mail.somewhere.edu}inbox\n# Normal Unix default is the local INBOX (usually /usr/spool/mail/$USER).";

CONF_TXT_T cf_text_incoming_folders[] =	"List of incoming msg folders besides INBOX, e.g. ={host2}inbox, {host3}inbox\n# Syntax: optnl-label {optnl-imap-host-name}folder-path";

CONF_TXT_T cf_text_folder_collections[] =	"List of directories where saved-message folders may be. First one is\n# the default for Saves. Example: Main {host1}mail/[], Desktop mail\\[]\n# Syntax: optnl-label {optnl-imap-hostname}optnl-directory-path[]";

CONF_TXT_T cf_text_news_collections[] =	"List, only needed if nntp-server not set, or news is on a different host\n# than used for NNTP posting. Examples: News *[] or News *{host3/nntp}[]\n# Syntax: optnl-label *{news-host/protocol}[]";

CONF_TXT_T cf_text_pruned_folders[] =	"List of folders, assumed to be in first folder collection,\n# offered for pruning each month.  For example: mumble";

CONF_TXT_T cf_text_default_fcc[] =		"Over-rides default path for sent-mail folder, e.g. =old-mail (using first\n# folder collection dir) or ={host2}sent-mail or =\"\" (to suppress saving).\n# Default: sent-mail (Unix) or SENTMAIL.MTX (PC) in default folder collection.";

CONF_TXT_T cf_text_default_saved[] =	"Over-rides default path for saved-msg folder, e.g. =saved-messages (using 1st\n# folder collection dir) or ={host2}saved-mail or =\"\" (to suppress saving).\n# Default: saved-messages (Unix) or SAVEMAIL.MTX (PC) in default collection.";

CONF_TXT_T cf_text_postponed_folder[] =	"Over-rides default path for postponed messages folder, e.g. =pm (which uses\n# first folder collection dir) or ={host4}pm (using home dir on host4).\n# Default: postponed-msgs (Unix) or POSTPOND.MTX (PC) in default fldr coltn.";

CONF_TXT_T cf_text_mail_directory[] =	"Pine compares this value with the first folder collection directory.\n# If they match (or no folder collections are defined), and the directory\n# does not exist, Pine will create and use it. Default: ~/mail";

CONF_TXT_T cf_text_read_message_folder[] =	"If set, specifies where already-read messages will be moved upon quitting.";

CONF_TXT_T cf_text_form_letter_folder[] =	"If set, specifies where form letters should be stored.";

CONF_TXT_T cf_text_signature_file[] =	"Over-rides default path for signature file. Default is ~/.signature";

CONF_TXT_T cf_text_literal_sig[] =	"Contains the actual signature contents as opposed to the signature filename.\n# If defined, this overrides the signature-file. Default is undefined.";

CONF_TXT_T cf_text_global_address_book[] =	"List of file or path names for global/shared addressbook(s).\n# Default: none\n# Syntax: optnl-label path-name";

CONF_TXT_T cf_text_address_book[] =	"List of file or path names for personal addressbook(s).\n# Default: ~/.addressbook (Unix) or \\PINE\\ADDRBOOK (PC)\n# Syntax: optnl-label path-name";

CONF_TXT_T cf_text_feature_list[] =	"List of features; see Pine's Setup/options menu for the current set.\n# e.g. feature-list= select-without-confirm, signature-at-bottom\n# Default condition for all of the features is no-.";

CONF_TXT_T cf_text_initial_keystroke_list[] =	"Pine executes these keys upon startup (e.g. to view msg 13: i,j,1,3,CR,v)";

CONF_TXT_T cf_text_default_composer_hdrs[] =	"Only show these headers (by default) when composing messages";

CONF_TXT_T cf_text_customized_hdrs[] =	"Add these customized headers (and possible default values) when composing";

CONF_TXT_T cf_text_view_headers[] =	"When viewing messages, include this list of headers";

CONF_TXT_T cf_text_view_margin_left[] =	"When viewing messages, number of blank spaces between left display edge and text";

CONF_TXT_T cf_text_view_margin_right[] =	"When viewing messages, number of blank spaces between right display edge and text";

CONF_TXT_T cf_text_quote_suppression[] =	"When viewing messages, number of lines of quote displayed before suppressing";

CONF_TXT_T cf_text_color_style[] =	"Controls display of color";

CONF_TXT_T cf_text_current_indexline_style[] =	"Controls display of color for current index line";

CONF_TXT_T cf_text_titlebar_color_style[] =	"Controls display of color for the titlebar at top of screen";

CONF_TXT_T cf_text_view_hdr_color[] =	"When viewing messages, these are the header colors";

CONF_TXT_T cf_text_save_msg_name_rule[] =	"Determines default folder name for Saves...\n# Choices: default-folder, by-sender, by-from, by-recipient, last-folder-used.\n# Default: \"default-folder\", i.e. \"saved-messages\" (Unix) or \"SAVEMAIL\" (PC).";

CONF_TXT_T cf_text_fcc_name_rule[] =	"Determines default name for Fcc...\n# Choices: default-fcc, by-recipient, last-fcc-used.\n# Default: \"default-fcc\" (see also \"default-fcc=\" variable.)";

CONF_TXT_T cf_text_sort_key[] =		"Sets presentation order of messages in Index. Choices:\n# Subject, From, Arrival, Date, Size, To, Cc, OrderedSubj, Score, and Thread.\n# Order may be reversed by appending /Reverse. Default: \"Arrival\".";

CONF_TXT_T cf_text_addrbook_sort_rule[] =	"Sets presentation order of address book entries. Choices: dont-sort,\n# fullname-with-lists-last, fullname, nickname-with-lists-last, nickname\n# Default: \"fullname-with-lists-last\".";

CONF_TXT_T cf_text_folder_sort_rule[] =	"Sets presentation order of folder list entries. Choices: alphabetical,\n# alpha-with-dirs-last, alpha-with-dirs-first.\n# Default: \"alpha-with-directories-last\".";

CONF_TXT_T cf_text_character_set[] =	"Reflects capabilities of the display you have. Default: US-ASCII.\n# Typical alternatives include ISO-8859-x, (x is a number between 1 and 9).";

CONF_TXT_T cf_text_editor[] =		"Specifies the program invoked by ^_ in the Composer,\n# or the \"enable-alternate-editor-implicitly\" feature.";

CONF_TXT_T cf_text_speller[] =		"Specifies the program invoked by ^T in the Composer.";

CONF_TXT_T cf_text_deadlets[] =		"Specifies the number of dead letter files to keep when canceling.";

CONF_TXT_T cf_text_fillcol[] =		"Specifies the column of the screen where the composer should wrap.";

CONF_TXT_T cf_text_replystr[] =		"Specifies the string to insert when replying to a message.";

CONF_TXT_T cf_text_quotereplstr[] =    	"Specifies the string to replace quotes with when viewing a message.";

CONF_TXT_T cf_text_replyintro[] =	"Specifies the introduction to insert when replying to a message.";

CONF_TXT_T cf_text_emptyhdr[] =		"Specifies the string to use when sending a  message with no to or cc.";

CONF_TXT_T cf_text_image_viewer[] =	"Program to view images (e.g. GIF or TIFF attachments).";

CONF_TXT_T cf_text_browser[] =		"List of programs to open Internet URLs (e.g. http or ftp references).";

CONF_TXT_T cf_text_inc_startup[] =	"Sets message which cursor begins on. Choices: first-unseen, first-recent,\n# first-important, first-important-or-unseen, first-important-or-recent,\n# first, last. Default: \"first-unseen\".";

CONF_TXT_T cf_pruning_rule[] =		"Allows a default answer for the prune folder questions. Choices: yes-ask,\n# yes-no, no-ask, no-no, ask-ask, ask-no. Default: \"ask-ask\".";

CONF_TXT_T cf_reopen_rule[] =		"Controls behavior when reopening an already open folder.";

CONF_TXT_T cf_text_thread_disp_style[] = "Style that MESSAGE INDEX is displayed in when threading.";

CONF_TXT_T cf_text_thread_index_style[] = "Style of THREAD INDEX or default MESSAGE INDEX when threading.";

CONF_TXT_T cf_text_thread_more_char[] =	"When threading, character used to indicate collapsed messages underneath.";

CONF_TXT_T cf_text_thread_exp_char[] =	"When threading, character used to indicate expanded messages underneath.";

CONF_TXT_T cf_text_thread_lastreply_char[] =	"When threading, character used to indicate this is the last reply\n# to the parent of this message.";

CONF_TXT_T cf_text_use_only_domain_name[] = "If \"user-domain\" not set, strips hostname in FROM address. (Unix only)";

CONF_TXT_T cf_text_printer[] =		"Your default printer selection";

CONF_TXT_T cf_text_personal_print_command[] =	"List of special print commands";

CONF_TXT_T cf_text_personal_print_cat[] =	"Which category default print command is in";

CONF_TXT_T cf_text_standard_printer[] =	"The system wide standard printers";

CONF_TXT_T cf_text_last_time_prune_quest[] =	"Set by Pine; controls beginning-of-month sent-mail pruning.";

CONF_TXT_T cf_text_last_version_used[] =	"Set by Pine; controls display of \"new version\" message.";

CONF_TXT_T cf_text_disable_drivers[] =		"List of mail drivers to disable.";

CONF_TXT_T cf_text_disable_auths[] =		"List of SASL authenticators to disable.";

CONF_TXT_T cf_text_remote_abook_metafile[] =	"Set by Pine; contains data for caching remote address books.";

CONF_TXT_T cf_text_old_patterns[] =		"Patterns is obsolete, use patterns-xxx";

CONF_TXT_T cf_text_old_filters[] =		"Patterns-filters is obsolete, use patterns-filters2";

CONF_TXT_T cf_text_old_scores[] =		"Patterns-scores is obsolete, use patterns-scores2";

CONF_TXT_T cf_text_patterns[] =			"Patterns and their actions are stored here.";

CONF_TXT_T cf_text_remote_abook_history[] =	"How many extra copies of remote address book should be kept. Default: 3";

CONF_TXT_T cf_text_remote_abook_validity[] =	"Minimum number of minutes between checks for remote address book changes.\n# 0 means never check except when opening a remote address book.\n# -1 means never check. Default: 5";

CONF_TXT_T cf_text_bugs_fullname[] =	"Full name for bug report address used by \"Report Bug\" command";

CONF_TXT_T cf_text_bugs_address[] =	"Email address used to send bug reports";

CONF_TXT_T cf_text_bugs_extras[] =		"Program/Script used by \"Report Bug\" command. No default.";

CONF_TXT_T cf_text_suggest_fullname[] =	"Full name for suggestion address used by \"Report Bug\" command";

CONF_TXT_T cf_text_suggest_address[] =	"Email address used to send suggestions";

CONF_TXT_T cf_text_local_fullname[] =	"Full name for \"local support\" address used by \"Report Bug\" command.\n# Default: Local Support";

CONF_TXT_T cf_text_local_address[] =	"Email address used to send to \"local support\".\n# Default: postmaster";

CONF_TXT_T cf_text_forced_abook[] =	"Force these address book entries into all writable personal address books.\n# Syntax is   forced-abook-entry=nickname|fullname|address\n# This is a comma-separated list of entries, each with syntax above.\n# Existing entries with same nickname are not replaced.\n# Example: help|Help Desk|help@ourdomain.com";

CONF_TXT_T cf_text_kblock_passwd[] =	"This is a number between 1 and 5.  It is the number of times a user will\n# have to enter a password when they run the keyboard lock command in the\n# main menu.  Default is 1.";

CONF_TXT_T cf_text_sendmail_path[] =	"This names the path to an alternative program, and any necessary arguments,\n# to be used in posting mail messages.  Example:\n#                    /usr/lib/sendmail -oem -t -oi\n# or,\n#                    /usr/local/bin/sendit.sh\n# The latter a script found in Pine distribution's contrib/util directory.\n# NOTE: The program MUST read the message to be posted on standard input,\n#       AND operate in the style of sendmail's \"-t\" option.";

CONF_TXT_T cf_text_oper_dir[] =	"This names the root of the tree to which the user is restricted when reading\n# and writing folders and files.  For example, on Unix ~/work confines the\n# user to the subtree beginning with their work subdirectory.\n# (Note: this alone is not sufficient for preventing access.  You will also\n# need to restrict shell access and so on, see Pine Technical Notes.)\n# Default: not set (so no restriction)";

CONF_TXT_T cf_text_in_fltr[] = 		"This variable takes a list of programs that message text is piped into\n# after MIME decoding, prior to display.";

CONF_TXT_T cf_text_out_fltr[] =		"This defines a program that message text is piped into before MIME\n# encoding, prior to sending";

CONF_TXT_T cf_text_alt_addrs[] =	"A list of alternate addresses the user is known by";

CONF_TXT_T cf_text_keywords[] =		"A list of keywords for use in categorizing messages";

CONF_TXT_T cf_text_kw_colors[] =	"Colors used to display keywords in the index";

CONF_TXT_T cf_text_kw_braces[] =	"Characters which surround keywords in SUBJKEY token.\n# Default is \"{\" \"} \"";

CONF_TXT_T cf_text_abook_formats[] =	"This is a list of formats for address books.  Each entry in the list is made\n# up of space-delimited tokens telling which fields are displayed and in\n# which order.  See help text";

CONF_TXT_T cf_text_index_format[] =	"This gives a format for displaying the index.  It is made\n# up of space-delimited tokens telling which fields are displayed and in\n# which order.  See help text";

CONF_TXT_T cf_text_overlap[] =		"The number of lines of overlap when scrolling through message text";

CONF_TXT_T cf_text_maxremstreams[] =	"The maximum number of non-stayopen remote connections that pine will use";

CONF_TXT_T cf_text_permlocked[] =	"A list of folders that should be left open once opened (INBOX is implicit)";

CONF_TXT_T cf_text_margin[] =		"Number of lines from top and bottom of screen where single\n# line scrolling occurs.";

CONF_TXT_T cf_text_stat_msg_delay[] =	"The number of seconds to sleep after writing a status message";

CONF_TXT_T cf_text_mailcheck[] =	"The approximate number of seconds between checks for new mail";

CONF_TXT_T cf_text_mailchecknoncurr[] =	"The approximate number of seconds between checks for new mail in folders\n# other than the current folder and inbox.\n# Default is same as mail-check-interval";

CONF_TXT_T cf_text_maildropcheck[] =	"The minimum number of seconds between checks for new mail in a Mail Drop.\n# This is always effectively at least as large as the mail-check-interval";

CONF_TXT_T cf_text_nntprange[] =	"For newsgroups accessed using NNTP, only messages numbered in the range\n# lastmsg-range+1 to lastmsg will be considered";

CONF_TXT_T cf_text_news_active[] =	"Path and filename of news configuration's active file.\n# The default is typically \"/usr/lib/news/active\".";

CONF_TXT_T cf_text_news_spooldir[] =	"Directory containing system's news data.\n# The default is typically \"/usr/spool/news\"";

CONF_TXT_T cf_text_upload_cmd[] =	"Path and filename of the program used to upload text from your terminal\n# emulator's into Pine's composer.";

CONF_TXT_T cf_text_upload_prefix[] =	"Text sent to terminal emulator prior to invoking the program defined by\n# the upload-command variable.\n# Note: _FILE_ will be replaced with the temporary file used in the upload.";

CONF_TXT_T cf_text_download_cmd[] =	"Path and filename of the program used to download text via your terminal\n# emulator from Pine's export and save commands.";

CONF_TXT_T cf_text_download_prefix[] =	"Text sent to terminal emulator prior to invoking the program defined by\n# the download-command variable.\n# Note: _FILE_ will be replaced with the temporary file used in the downlaod.";

CONF_TXT_T cf_text_goto_default[] =	"Sets the default folder and collectionoffered at the Goto Command's prompt.";

CONF_TXT_T cf_text_mailcap_path[] =	"Sets the search path for the mailcap configuration file.\n# NOTE: colon delimited under UNIX, semi-colon delimited under DOS/Windows/OS2.";

CONF_TXT_T cf_text_mimetype_path[] =	"Sets the search path for the mimetypes configuration file.\n# NOTE: colon delimited under UNIX, semi-colon delimited under DOS/Windows/OS2.";

CONF_TXT_T cf_text_newmail_fifo_path[] = "Sets the filename for the newmail fifo (named pipe). Unix only.";

CONF_TXT_T cf_text_nmw_width[] = "Sets the width for the NewMail screen.";

CONF_TXT_T cf_text_user_input_timeo[] =	"If no user input for this many hours, Pine will exit if in an idle loop\n# waiting for a new command.  If set to zero (the default), then there will\n# be no timeout.";

CONF_TXT_T cf_text_debug_mem[] =	"Debug-memory is obsolete";

CONF_TXT_T cf_text_tcp_open_timeo[] =	"Sets the time in seconds that Pine will attempt to open a network\n# connection.  The default is 30, the minimum is 5, and the maximum is\n# system defined (typically 75).";

CONF_TXT_T cf_text_tcp_read_timeo[] =	"Network read warning timeout. The default is 15, the minimum is 5, and the\n# maximum is 1000.";

CONF_TXT_T cf_text_tcp_write_timeo[] =	"Network write warning timeout. The default is 0 (unset), the minimum\n# is 5 (if not 0), and the maximum is 1000.";

CONF_TXT_T cf_text_tcp_query_timeo[] =	"If this much time has elapsed at the time of a tcp read or write\n# timeout, pine will ask if you want to break the connection.\n# Default is 60 seconds, minimum is 5, maximum is 1000.";

CONF_TXT_T cf_text_rsh_open_timeo[] =	"Sets the time in seconds that Pine will attempt to open a UNIX remote\n# shell connection.  The default is 15, min is 5, and max is unlimited.\n# Zero disables rsh altogether.";

CONF_TXT_T cf_text_rsh_path[] =		"Sets the name of the command used to open a UNIX remote shell connection.\n# The default is typically /usr/ucb/rsh.";

CONF_TXT_T cf_text_rsh_command[] =	"Sets the format of the command used to open a UNIX remote\n# shell connection.  The default is \"%s %s -l %s exec /etc/r%sd\"\n# NOTE: the 4 (four) \"%s\" entries MUST exist in the provided command\n# where the first is for the command's path, the second is for the\n# host to connect to, the third is for the user to connect as, and the\n# fourth is for the connection method (typically \"imap\")";

CONF_TXT_T cf_text_ssh_open_timeo[] =	"Sets the time in seconds that Pine will attempt to open a UNIX secure\n# shell connection.  The default is 15, min is 5, and max is unlimited.\n# Zero disables ssh altogether.";

CONF_TXT_T cf_text_ssh_path[] =		"Sets the name of the command used to open a UNIX secure shell connection.\n# Typically this is /usr/bin/ssh.";

CONF_TXT_T cf_text_ssh_command[] =	"Sets the format of the command used to open a UNIX secure\n# shell connection.  The default is \"%s %s -l %s exec /etc/r%sd\"\n# NOTE: the 4 (four) \"%s\" entries MUST exist in the provided command\n# where the first is for the command's path, the second is for the\n# host to connect to, the third is for the user to connect as, and the\n# fourth is for the connection method (typically \"imap\")";

CONF_TXT_T cf_text_version_threshold[] = "Sets the version number Pine will use as a threshold for offering\n# its new version message on startup.";

CONF_TXT_T cf_text_archived_folders[] =	"List of folder pairs; the first indicates a folder to archive, and the\n# second indicates the folder read messages in the first should\n# be moved to.";

CONF_TXT_T cf_text_elm_style_save[] =	"Elm-style-save is obsolete, use saved-msg-name-rule";

CONF_TXT_T cf_text_header_in_reply[] =	"Header-in-reply is obsolete, use include-header-in-reply in feature-list";

CONF_TXT_T cf_text_feature_level[] =	"Feature-level is obsolete, use feature-list";

CONF_TXT_T cf_text_old_style_reply[] =	"Old-style-reply is obsolete, use signature-at-bottom in feature-list";

CONF_TXT_T cf_text_compose_mime[] =	"Compose-mime is obsolete";

CONF_TXT_T cf_text_show_all_characters[] =	"Show-all-characters is obsolete";

CONF_TXT_T cf_text_save_by_sender[] =	"Save-by-sender is obsolete, use saved-msg-name-rule";

CONF_TXT_T cf_text_file_dir[] =		"Default directory used for Attachment handling (attach and save)\n# and Export command output";

CONF_TXT_T cf_text_folder_extension[] =	"Folder-extension is obsolete";

CONF_TXT_T cf_text_normal_foreground_color[] =	"Choose: black, blue, green, cyan, red, magenta, yellow, or white.";

CONF_TXT_T cf_text_window_position[] =	"Window position in the format: CxR+X+Y\n# Where C and R are the window size in characters and X and Y are the\n# screen position of the top left corner of the window.\n# This is no longer used unless position is not set in registry.";

CONF_TXT_T cf_text_newsrc_path[] =		"Full path and name of NEWSRC file";

/* these are used to report folder directory creation problems */
CONF_TXT_T init_md_exists[] =	"The \"%s\" subdirectory already exists, but it is not writable by Pine so Pine cannot run.  Please correct the permissions and restart Pine.";

CONF_TXT_T init_md_file[] =	"Pine requires a directory called \"%s\" and usualy creates it.  You already have a regular file by that name which means Pine cannot create the directory.  Please move or remove it and start Pine again.";

CONF_TXT_T init_md_create[] =	"Creating subdirectory \"%s\" where Pine will store its mail folders.";


/*----------------------------------------------------------------------
These are the variables that control a number of pine functions.  They
come out of the .pinerc and the /usr/local/lib/pine.conf files.  Some can
be set by the user while in Pine.  Eventually all the local ones should
be so and maybe the global ones too.

Each variable can have a command-line, user, global, and current value.
All of these values are malloc'd.  The user value is the one read out of
the user's .pinerc, the global value is the one from the system pine
configuration file.  There are often defaults for the global values, set
at the start of init_vars().  Perhaps someday there will be group values.
The current value is the one that is actually in use.
  ----*/
/* name                                                remove_quotes
                                                     is_outermost  |
                                                   is_onlymain  |  |
                                                   is_fixed  |  |  |
                                                 is_list  |  |  |  |
                                            is_global  |  |  |  |  |
                                           is_user  |  |  |  |  |  |
                                   been_written  |  |  |  |  |  |  |
                                     is_used  |  |  |  |  |  |  |  |
                              is_obsolete  |  |  |  |  |  |  |  |  |
                                        |  |  |  |  |  |  |  |  |  |
  (on following line) description       |  |  |  |  |  |  |  |  |  |
                                |       |  |  |  |  |  |  |  |  |  |
                                |       |  |  |  |  |  |  |  |  |  |  */
static struct variable variables[] = {
{"personal-name",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_personal_name},
#if defined(DOS) || defined(OS2)
                        /* Have to have this on DOS, PC's, Macs, etc... */
{"user-id",				0, 1, 0, 1, 0, 0, 0, 0, 0, 1,
#else			/* Don't allow on UNIX machines for some security */
{"user-id",				0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
#endif
				cf_text_user_id},
{"user-domain",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_user_domain},
{"smtp-server",				0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_smtp_server},
{"nntp-server",				0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_nntp_server},
{"inbox-path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_inbox_path},
{"incoming-archive-folders",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_archived_folders},
{"pruned-folders",			0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_pruned_folders},
{"default-fcc",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_default_fcc},
{"default-saved-msg-folder",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_default_saved},
{"postponed-folder",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_postponed_folder},
{"read-message-folder",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_read_message_folder},
{"form-letter-folder",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_form_letter_folder},
{"literal-signature",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_literal_sig},
{"signature-file",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_signature_file},
{"feature-list",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_feature_list},
{"initial-keystroke-list",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_initial_keystroke_list},
{"default-composer-hdrs",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_default_composer_hdrs},
{"customized-hdrs",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_customized_hdrs},
{"viewer-hdrs",				0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_view_headers},
{"viewer-margin-left",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_view_margin_left},
{"viewer-margin-right",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_view_margin_right},
{"quote-suppression-threshold",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_quote_suppression},
{"saved-msg-name-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_save_msg_name_rule},
{"fcc-name-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_fcc_name_rule},
{"sort-key",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_sort_key},
{"addrbook-sort-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_addrbook_sort_rule},
{"folder-sort-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_folder_sort_rule},
{"goto-default-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_goto_default},
{"incoming-startup-rule",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_inc_startup},
{"pruning-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_pruning_rule},
{"folder-reopen-rule",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_reopen_rule},
{"threading-display-style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_thread_disp_style},
{"threading-index-style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_thread_index_style},
{"threading-indicator-character",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_thread_more_char},
{"threading-expanded-character",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_thread_exp_char},
{"threading-lastreply-character",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_thread_lastreply_char},
{"character-set",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_character_set},
{"editor",				0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_editor},
{"speller",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_speller},
{"composer-wrap-column",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_fillcol},
{"reply-indent-string",			0, 1, 0, 1, 1, 0, 0, 0, 0, 0,
				cf_text_replystr},
{"reply-leadin",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_replyintro},
{"quote-replace-string",		0, 1, 0, 1, 1, 0, 0, 0, 0, 0,
				cf_text_quotereplstr},
{"empty-header-message",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_emptyhdr},
{"image-viewer",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_image_viewer},
{"use-only-domain-name",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_use_only_domain_name},
{"bugs-fullname",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_bugs_fullname},
{"bugs-address",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_bugs_address},
{"bugs-additional-data",		0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_bugs_extras},
{"suggest-fullname",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_suggest_fullname},
{"suggest-address",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_suggest_address},
{"local-fullname",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_local_fullname},
{"local-address",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_local_address},
{"forced-abook-entry",			0, 1, 0, 0, 1, 1, 0, 0, 0, 0,
				cf_text_forced_abook},
{"kblock-passwd-count",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_kblock_passwd},
{"display-filters",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_in_fltr},
{"sending-filters",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_out_fltr},
{"alt-addresses",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_alt_addrs},
{"keywords",				0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_keywords},
{"keyword-surrounding-chars",		0, 1, 0, 1, 1, 0, 0, 0, 0, 0,
				cf_text_kw_braces},
{"addressbook-formats",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_abook_formats},
{"index-format",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_index_format},
{"viewer-overlap",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_overlap},
{"scroll-margin",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_margin},
{"status-message-delay",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_stat_msg_delay},
{"mail-check-interval",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_mailcheck},
{"mail-check-interval-noncurrent",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_mailchecknoncurr},
{"maildrop-check-minimum",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_maildropcheck},
{"nntp-range",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_nntprange},
{"newsrc-path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_newsrc_path},
{"news-active-file-path",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_news_active},
{"news-spool-directory",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_news_spooldir},
{"upload-command",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_upload_cmd},
{"upload-command-prefix",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_upload_prefix},
{"download-command",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_download_cmd},
{"download-command-prefix",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_download_prefix},
{"mailcap-search-path",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_mailcap_path},
{"mimetype-search-path",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_mimetype_path},
{"url-viewers",				0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_browser},
{"max-remote-connections",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_maxremstreams},
{"stay-open-folders",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_permlocked},
{"dead-letter-files",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_deadlets},
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
{"newmail-fifo-path",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_newmail_fifo_path},
#endif
{"newmail-window-width",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_nmw_width},
/*
 * Starting here, the variables are hidden in the Setup/Config screen.
 * They are exposed if feature expose-hidden-config is set.
 */
{"incoming-folders",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_incoming_folders},
{"mail-directory",			0, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_mail_directory},
{"folder-collections",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_folder_collections},
{"news-collections",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_news_collections},
{"address-book",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_address_book},
{"global-address-book",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_global_address_book},
{"standard-printer",			0, 1, 0, 0, 1, 1, 0, 0, 0, 0,
				cf_text_standard_printer},
{"last-time-prune-questioned",		0, 1, 0, 1, 0, 0, 0, 1, 0, 1,
				cf_text_last_time_prune_quest},
{"last-version-used",			0, 1, 0, 1, 0, 0, 0, 0, 1, 1,
				cf_text_last_version_used},
{"sendmail-path",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_sendmail_path},
{"operating-dir",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_oper_dir},
{"user-input-timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_user_input_timeo},
/* OBSOLETE */
#ifdef DEBUGJOURNAL
{"debug-memory",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_debug_mem},
#endif

{"tcp-open-timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_tcp_open_timeo},
{"tcp-read-warning-timeout",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_tcp_read_timeo},
{"tcp-write-warning-timeout",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_tcp_write_timeo},
{"tcp-query-timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_tcp_query_timeo},
{"rsh-command",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_rsh_command},
{"rsh-path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_rsh_path},
{"rsh-open-timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_rsh_open_timeo},
{"ssh-command",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_ssh_command},
{"ssh-path",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_ssh_path},
{"ssh-open-timeout",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_ssh_open_timeo},
{"new-version-threshold",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_version_threshold},
{"disable-these-drivers",		0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_disable_drivers},
{"disable-these-authenticators",	0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_disable_auths},
{"remote-abook-metafile",		0, 1, 0, 1, 0, 0, 0, 0, 1, 1,
				cf_text_remote_abook_metafile},
{"remote-abook-history",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_remote_abook_history},
{"remote-abook-validity",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_remote_abook_validity},
{"printer",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_printer},
{"personal-print-command",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_personal_print_command},
{"personal-print-category",		0, 1, 0, 1, 0, 0, 0, 0, 0, 1,
				cf_text_personal_print_cat},
{"patterns",				1, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_old_patterns},
{"patterns-roles",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_patterns},
{"patterns-filters2",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_patterns},
{"patterns-filters",			1, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_old_filters},
{"patterns-scores2",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_patterns},
{"patterns-scores",			1, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_old_scores},
{"patterns-indexcolors",		0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_patterns},
{"patterns-other",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_patterns},

/* OBSOLETE VARS */
{"elm-style-save",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_elm_style_save},
{"header-in-reply",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_header_in_reply},
{"feature-level",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_feature_level},
{"old-style-reply",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_old_style_reply},
{"compose-mime",			1, 1, 0, 0, 1, 0, 0, 0, 0, 1,
				cf_text_compose_mime},
{"show-all-characters",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_show_all_characters},
{"save-by-sender",			1, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_save_by_sender},
#if defined(DOS) || defined(OS2)
{"file-directory",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_file_dir},
{"folder-extension",			1, 0, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_folder_extension},
#endif
#ifndef	_WINDOWS
{"color-style",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_color_style},
#endif
{"current-indexline-style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_current_indexline_style},
{"titlebar-color-style",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_titlebar_color_style},
{"normal-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				cf_text_normal_foreground_color},
{"normal-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"reverse-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"reverse-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"title-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"title-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"status-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"status-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"keylabel-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"keylabel-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"keyname-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"keyname-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"selectable-item-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"selectable-item-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"quote1-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"quote1-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"quote2-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"quote2-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"quote3-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"quote3-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"signature-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"signature-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"prompt-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"prompt-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-to-me-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-to-me-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-important-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-important-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-deleted-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-deleted-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-answered-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-answered-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-new-foreground-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-new-background-color",		0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-recent-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-recent-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-unseen-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-unseen-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-arrow-foreground-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"index-arrow-background-color",	0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"viewer-hdr-colors",			0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_view_hdr_color},
{"keyword-colors",			0, 1, 0, 1, 1, 1, 0, 0, 0, 1,
				cf_text_kw_colors},
#if defined(DOS) || defined(OS2)
#ifdef _WINDOWS
{"font-name",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				"Name and size of font."},
{"font-size",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"font-style",				0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"font-char-set",      			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"print-font-name",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1,
				"Name and size of printer font."},
{"print-font-size",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"print-font-style",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"print-font-char-set",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
{"window-position",			0, 1, 0, 1, 1, 0, 0, 0, 1, 1,
				cf_text_window_position},
{"cursor-style",			0, 1, 0, 1, 1, 0, 0, 0, 0, 1, NULL},
#endif	/* _WINDOWS */
#endif	/* DOS */
#ifdef	ENABLE_LDAP
{"ldap-servers",			0, 1, 0, 1, 1, 1, 0, 0, 0, 0,
				cf_text_ldap_server},
#endif	/* ENABLE_LDAP */
{NULL,					0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL}
};


#ifdef	DEBUG
/*
 * Debug level and output file defined here, referenced globally.
 * The debug file is opened and initialized below...
 */
int   debug	= DEFAULT_DEBUG;
FILE *debugfile = NULL;
#endif


void
init_init_vars(ps)
     struct pine *ps;
{
    ps->vars = variables;
}


/* this is just like dprint except it prints to a char * */
#ifdef DEBUG
#define   mprint(n,x) {			\
	       if(debug >= (n)){	\
		   sprintf x ;		\
		   db += strlen(db);	\
	       }			\
	   }
#else
#define   mprint(n,x)
#endif

/*
 * this was split out from init_vars so we can get at the
 * pinerc location sooner.
 */
void
init_pinerc(ps, debug_out)
     struct pine *ps;
     char       **debug_out;
{
    char      buf[MAXPATH+1], *p, *db;
#if defined(DOS) || defined(OS2)
    char      buf2[MAXPATH+1], l_pinerc[MAXPATH+1];
    int nopinerc = 0, confregset = -1;
    register struct variable *vars = ps->vars;
#endif

#ifdef DEBUG
    /*
     * Since this routine is called before we've had a chance to set up
     * the debug file for output, we put the debugging into memory and
     * pass it back to the caller for use after init_debug(). We just
     * allocate plenty of space.
     */
    if(debug_out){
	db = *debug_out = (char *)fs_get(25000 * sizeof(char));
	db[0] = '\0';
    }
#endif

    mprint(2, (db, "\n -- init_pinerc --\n\n"));

#if defined(DOS) || defined(OS2)
    /*
     * Rules for the config/support file locations under DOS are:
     *
     * 1) The location of the PINERC is searched for in the following
     *    order of precedence:
     *	     - File pointed to by '-p' command line option
     *       - File pointed to by PINERC environment variable
     *       - $HOME\pine
     *       - same dir as argv[0]
     *
     * 2) The HOME environment variable, if not set, defaults to 
     *    root of the current working drive (see pine.c)
     * 
     * 3) The default for external files (PINE.SIG and ADDRBOOK) is the
     *    same directory as the pinerc
     *
     * 4) The support files (PINE.HLP and PINE.NDX) are expected to be in
     *    the same directory as PINE.EXE.
     */

    if(ps->prc){
      mprint(2, (db,
	     "Personal config \"%.100s\" comes from command line\n",
	     (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
    }
    else{
      mprint(2, (db,
	     "Personal config not set on cmdline, checking for $PINERC\n"));
    }

    /*
     * First, if prc hasn't been set by a command-line -p, check to see
     * if PINERC is in the environment. If so, treat it just like we
     * would have treated it if it were a command-line arg.
     */
    if(!ps->prc && (p = getenv("PINERC")) && *p){
	char path[MAXPATH], dir[MAXPATH];

	if(IS_REMOTE(p) || is_absolute_path(p)){
	    strncpy(path, p, sizeof(path)-1);
	    path[sizeof(path)-1] = '\0';
	}
	else{
	    getcwd(dir, sizeof(dir));
	    build_path(path, dir, p, sizeof(path));
	}

	if(!IS_REMOTE(p))
	  ps->pinerc = cpystr(path);

	ps->prc = new_pinerc_s(path);

	if(ps->prc){
	  mprint(2, (db,
	  "  yes, personal config \"%.100s\" comes from $PINERC\n",
		 (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
	}
    }

    /*
     * Pinerc used to be the name of the pinerc file. Then we added
     * the possibility of the pinerc file being remote, and we replaced
     * the variable pinerc with the structure prc. Unfortunately, some
     * parts of pine rely on the fact that pinerc is the name of the
     * pinerc _file_, and use the directory that the pinerc file is located
     * in for their own purposes. We want to preserve that so things will
     * keep working. So, even if the real pinerc is remote, we need to
     * put the name of a pinerc file in the pinerc variable so that the
     * directory which contains that file is writable. The file itself
     * doesn't have to exist for this purpose, since we are really only
     * using the name of the directory containing the file. Twisted.
     * (Alternatively, we could fix all of the code that uses the pinerc
     * variable for this purpose to use a new variable which really is
     * just a directory.) hubert 2000-sep
     *
     * There are 3 cases. If pinerc is already set that means that the user
     * gave either a -p pinerc or an environment pinerc that is a local file,
     * and we are done. If pinerc is not set, then either prc is set or not.
     * If prc is set then the -p arg or PINERC value is a remote pinerc.
     * In that case we need to find a local directory to use, and put that
     * directory in the pinerc variable (with a fake filename tagged on).
     * If prc is not set, then user hasn't told us anything so we have to
     * try to find the default pinerc file by looking down the path of
     * possibilities. When we find it, we'll also use that directory.
     */
    if(!ps->pinerc){
	*l_pinerc = '\0';
	*buf = '\0';

	if(ps->prc){				/* remote pinerc case */
	    /*
	     * We don't give them an l_pinerc unless they tell us where
	     * to put it.
	     */
	    if(ps->aux_files_dir)
	      build_path(l_pinerc, ps->aux_files_dir, SYSTEM_PINERC,
			 sizeof(l_pinerc));
	    else{
		/*
		 * Search for a writable directory.
		 * Mimic what happens in !prc for local case, except we
		 * don't need to look for the actual file.
		 */

		/* check if $HOME\PINE is writable */
		build_path(buf2, ps->home_dir, DF_PINEDIR, sizeof(buf2));
		if(is_writable_dir(buf2) == 0)
		  build_path(l_pinerc, buf2, SYSTEM_PINERC, sizeof(l_pinerc));
		else{			/* $HOME\PINE not a writable dir */
		    /* use this unless registry redirects us */
		    build_path(l_pinerc, ps->pine_dir, SYSTEM_PINERC,
			       sizeof(l_pinerc));
#ifdef	_WINDOWS
		    /* if in registry, use that value */
		    if(mswin_reg(MSWR_OP_GET, MSWR_PINE_RC, buf2, sizeof(buf2))
		       && !IS_REMOTE(buf2)){
			strncpy(l_pinerc, buf2, sizeof(l_pinerc)-1);
			l_pinerc[sizeof(l_pinerc)-1] = '\0';
		    }
#endif
		}
	    }
	}
	else{			/* searching for pinerc file to use */
	    /*
	     * Buf2 is $HOME\PINE. If $HOME is not explicitly set,
	     * it defaults to the current working drive (often C:).
	     * See pine.c to see how it is initially set.
	     */

	    mprint(2, (db, "  no, searching...\n"));
	    build_path(buf2, ps->home_dir, DF_PINEDIR, sizeof(buf2));
	    mprint(2, (db,
	      "  checking for writable %.100s dir \"%.100s\" off of homedir\n",
		   DF_PINEDIR, buf2));
	    if(is_writable_dir(buf2) == 0){
		/*
		 * $HOME\PINE exists and is writable.
		 * See if $HOME\PINE\PINERC exists.
		 */
		build_path(buf, buf2, SYSTEM_PINERC, sizeof(buf));
		strncpy(l_pinerc, buf, sizeof(l_pinerc)-1);
		l_pinerc[sizeof(l_pinerc)-1] = '\0';
		mprint(2, (db, "  yes, now checking for file \"%.100s\"\n",
		       buf));
		if(can_access(buf, ACCESS_EXISTS) == 0){	/* found it! */
		    /*
		     * Buf is what we were looking for.
		     * It is local and can be used for the directory, too.
		     */
		    mprint(2, (db, "  found it\n"));
		}
		else{
		    /*
		     * No $HOME\PINE\PINERC, look for
		     * one in same dir as PINE.EXE.
		     */
		    build_path(buf2, ps->pine_dir, SYSTEM_PINERC,
			       sizeof(buf2));
		    mprint(2, (db,
			   "  no, checking for \"%.100s\" in pine.exe dir\n",
			   buf2));
		    if(can_access(buf2, ACCESS_EXISTS) == 0){
			/* found it! */
			mprint(2, (db, "  found it\n"));
			strncpy(buf, buf2, sizeof(buf)-1);
			buf[sizeof(buf)-1] = '\0';
			strncpy(l_pinerc, buf2, sizeof(l_pinerc)-1);
			l_pinerc[sizeof(l_pinerc)-1] = '\0';
		    }
		    else{
#ifdef	_WINDOWS
			mprint(2, (db, "  no, checking in registry\n"));
			if(mswin_reg(MSWR_OP_GET, MSWR_PINE_RC,
				     buf2, sizeof(buf2))){
			    strncpy(buf, buf2, sizeof(buf)-1);
			    buf[sizeof(buf)-1] = '\0';
			    if(!IS_REMOTE(buf2)){
				strncpy(l_pinerc, buf2, sizeof(l_pinerc)-1);
				l_pinerc[sizeof(l_pinerc)-1] = '\0';
			    }
			    /*
			     * Now buf is the pinerc to be used, l_pinerc is
			     * the directory, which may be either same as buf
			     * or it may be $HOME\PINE if registry gives us
			     * a remote pinerc.
			     */
			    mprint(2, (db, "  found \"%.100s\" in registry\n",
				   buf));
			}
			else{
			    nopinerc = 1;
			    mprint(2, (db, "  not found, asking user\n"));
			}
#else
			mprint(2, (db, "  not found\n"));
#endif
		    }
		}

		/*
		 * Buf is the pinerc (could be remote if from registry)
		 * and l_pinerc is the local pinerc, which may not exist.
		 */
	    }
	    else{			/* $HOME\PINE not a writable dir */
		/*
		 * We notice that the order of checking in the registry
		 * and checking in the PINE.EXE directory are different
		 * in this case versus the is_writable_dir(buf2) case, and
		 * that does sort of look like a bug. However,
		 * we don't think this is a bug since we did it on purpose
		 * a long time ago. So even though we can't remember why
		 * it is this way, we think we would rediscover why if we
		 * changed it! So we won't change it.
		 */

		/*
		 * Change the default to use to the PINE.EXE directory.
		 */
		build_path(buf, ps->pine_dir, SYSTEM_PINERC, sizeof(buf));
		strncpy(l_pinerc, buf, sizeof(l_pinerc)-1);
		l_pinerc[sizeof(l_pinerc)-1] = '\0';
#ifdef	_WINDOWS
		mprint(2, (db, "  no, not writable, checking in registry\n"));
		/* if in registry, use that value */
		if(mswin_reg(MSWR_OP_GET, MSWR_PINE_RC, buf2, sizeof(buf2))){
		    strncpy(buf, buf2, sizeof(buf)-1);
		    buf[sizeof(buf)-1] = '\0';
		    mprint(2, (db, "  found \"%.100s\" in registry\n",
			   buf));
		    if(!IS_REMOTE(buf)){
			strncpy(l_pinerc, buf, sizeof(l_pinerc)-1);
			l_pinerc[sizeof(l_pinerc)-1] = '\0';
		    }
		}
		else{
		    mprint(2, (db,
			"  no, checking for \"%.100s\" in pine.exe dir\n",
			buf));

		    if(can_access(buf, ACCESS_EXISTS) == 0){
			mprint(2, (db, "  found it\n"));
		    }
		    else{
			nopinerc = 1;
			mprint(2, (db, "  not found, asking user\n"));
		    }
		}
#else
		mprint(2, (db,
			"  no, checking for \"%.100s\" in pine.exe dir\n",
			buf));

		if(can_access(buf, ACCESS_EXISTS) == 0){
		    mprint(2, (db, "  found it\n"));
		}
		else{
		    mprint(2, (db, "  not found, creating it\n"));
		}
#endif
	    }

	    /*
	     * When we get here we have buf set to the name of the
	     * pinerc, which could be local or remote. We have l_pinerc
	     * set to the same as buf if buf is local, and set to another
	     * name otherwise, hopefully contained in a writable directory.
	     */
#ifdef _WINDOWS
	    if(nopinerc || ps_global->install_flag){
		char buf3[MAXPATH+1];

		confregset = 0;
		strncpy(buf3, buf, MAXPATH);
		buf3[MAXPATH] = '\0';
		if(os_config_dialog(buf3, MAXPATH,
				    &confregset, nopinerc) == 0){
		    strncpy(buf, buf3, MAXPATH);
		    buf[MAXPATH] = '\0';
		    mprint(2, (db, "  not found, creating it\n"));
		    mprint(2, (db, "  user says use \"%.100s\"\n", buf));
		    if(!IS_REMOTE(buf)){
			strncpy(l_pinerc, buf, MAXPATH);
			l_pinerc[MAXPATH] = '\0';
		    }
		}
		else{
		    exit(-1);
		}
	    }
#endif
	    ps->prc = new_pinerc_s(buf);
	}

	ps->pinerc = cpystr(l_pinerc);
    }

#if defined(DOS) || defined(OS2)
    /* 
     * The goal here is to set the auxiliary directory in the pinerc variable.
     * We are making the assumption that any reference to the pinerc variable
     * after this point is used only as a directory in which to store things,
     * with the prc variable being the preferred place to store pinerc location.
     * If -aux isn't set, then there is no change. -jpf 08/2001
     */
    if(ps->aux_files_dir){
	l_pinerc[0] = '\0';
	build_path(l_pinerc, ps->aux_files_dir, SYSTEM_PINERC,
		   sizeof(l_pinerc));
	if(ps->pinerc) fs_give((void **)&ps->pinerc);
	ps->pinerc = cpystr(l_pinerc);
	mprint(2, (db, "Setting aux_files_dir to \"%.100s\"\n",
	       ps->aux_files_dir));
    }
#endif

#ifdef	_WINDOWS
    if(confregset && (ps->update_registry != UREG_NEVER_SET))
      mswin_reg(MSWR_OP_SET | ((ps->update_registry == UREG_ALWAYS_SET)
			       || confregset == 1 ? MSWR_OP_FORCE : 0),
		MSWR_PINE_RC, 
		(ps->prc && ps->prc->name) ?
		ps->prc->name : ps->pinerc, NULL);
#endif

    /*
     * Now that we know the default for the PINERC, build NEWSRC default.
     * Backward compatibility makes this kind of funky.  If what the
     * c-client thinks the NEWSRC should be exists *AND* it doesn't
     * already exist in the PINERC's dir, use c-client's default, otherwise
     * use the one next to the PINERC...
     */
    p = last_cmpnt(ps->pinerc);
    buf[0] = '\0';
    if(p != NULL){
	strncpy(buf, ps->pinerc, min(p - ps->pinerc, sizeof(buf)-1));
	buf[min(p - ps->pinerc, sizeof(buf)-1)] = '\0';
    }

    mprint(2, (db, "Using directory \"%.100s\" for auxiliary files\n", buf));
    strncat(buf, "NEWSRC", sizeof(buf)-1-strlen(buf));

    if(!(p = (void *) mail_parameters(NULL, GET_NEWSRC, (void *)NULL))
       || can_access(p, ACCESS_EXISTS) < 0
       || can_access(buf, ACCESS_EXISTS) == 0){
	mail_parameters(NULL, SET_NEWSRC, (void *)buf);
	GLO_NEWSRC_PATH = cpystr(buf);
    }
    else
      GLO_NEWSRC_PATH = cpystr(p);

    if(ps->pconf){
      mprint(2, (db, "Global config \"%.100s\" comes from command line\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
    }
    else{
      mprint(2, (db,
	     "Global config not set on cmdline, checking for $PINECONF\n"));
    }

    if(!ps->pconf && (p = getenv("PINECONF"))){
	ps->pconf = new_pinerc_s(p);
	if(ps->pconf){
	  mprint(2, (db,
	     "  yes, global config \"%.100s\" comes from $PINECONF\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
	}
    }
#ifdef _WINDOWS
    else if(!ps->pconf
	    && mswin_reg(MSWR_OP_GET, MSWR_PINE_CONF, buf2, sizeof(buf2))){
	ps->pconf = new_pinerc_s(buf2);
	if(ps->pconf){
	    mprint(2, (db,
	     "  yes, global config \"%.100s\" comes from Registry\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
	}
    }
#endif
    if(!ps->pconf){
      mprint(2, (db, "  no, there is no global config\n"));
    }
#ifdef _WINDOWS
    else if (ps->pconf && ps->pconf->name && 
	     (ps->update_registry != UREG_NEVER_SET)){
	mswin_reg(MSWR_OP_SET | ((ps->update_registry == UREG_ALWAYS_SET)
				 ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_CONF,
		  ps->pconf->name, NULL);
    }
#endif
    
    if(!ps->prc)
      ps->prc = new_pinerc_s(ps->pinerc);

    if(ps->exceptions){
      mprint(2, (db,
	     "Exceptions config \"%.100s\" comes from command line\n",
	     ps->exceptions));
    }
    else{
      mprint(2, (db,
	     "Exceptions config not set on cmdline, checking for $PINERCEX\n"));
    }

    /*
     * Exceptions is done slightly differently from pinerc. Instead of setting
     * post_prc in args.c we just set the string and use it here. We do
     * that so that we can put it in the same directory as the pinerc if
     * exceptions is a relative name, and pinerc may not be set until here.
     *
     * First, just like for pinerc, check environment variable if it wasn't
     * set on the command line.
     */
    if(!ps->exceptions && (p = getenv("PINERCEX")) && *p){
	ps->exceptions = cpystr(p);
	if(ps->exceptions){
	  mprint(2, (db,
		 "  yes, exceptions config \"%.100s\" comes from $PINERCEX\n",
		 ps->exceptions));
	}
    }

    /*
     * If still not set, try specific file in same dir as pinerc.
     * Only use it if the file exists.
     */
    if(!ps->exceptions){
	p = last_cmpnt(ps->pinerc);
	buf[0] = '\0';
	if(p != NULL){
	    strncpy(buf, ps->pinerc, min(p - ps->pinerc, sizeof(buf)-1));
	    buf[min(p - ps->pinerc, sizeof(buf)-1)] = '\0';
	}

	strncat(buf, "PINERCEX", sizeof(buf)-1-strlen(buf));

	mprint(2, (db,
	       "  no, checking for default \"%.100s\" in pinerc dir\n", buf));
	if(can_access(buf, ACCESS_EXISTS) == 0)		/* found it! */
	  ps->exceptions = cpystr(buf);

	if(ps->exceptions){
	  mprint(2, (db,
		 "  yes, exceptions config \"%.100s\" comes from default\n",
		 ps->exceptions));
	}
	else{
	  mprint(2, (db, "  no, there is no exceptions config\n"));
	}
    }

#else /* unix */

    if(ps->pconf){
      mprint(2, (db, "Global config \"%.100s\" comes from command line\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
    }

    if(!ps->pconf){
	ps->pconf = new_pinerc_s(SYSTEM_PINERC);
	if(ps->pconf){
	  mprint(2, (db, "Global config \"%.100s\" is default\n",
	     (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<no name>"));
	}
    }

    if(!ps->pconf){
      mprint(2, (db, "No global config!\n"));
    }

    if(ps->prc){
      mprint(2, (db, "Personal config \"%.100s\" comes from command line\n",
	     (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
    }

    if(!ps->pinerc){
      build_path(buf, ps->home_dir, ".pinerc", sizeof(buf));
      ps->pinerc = cpystr(buf);
    }

    if(!ps->prc){
	ps->prc = new_pinerc_s(ps->pinerc);
	if(ps->prc){
	  mprint(2, (db, "Personal config \"%.100s\" is default\n",
	     (ps->prc && ps->prc->name) ? ps->prc->name : "<no name>"));
	}
    }

    if(!ps->prc){
      mprint(2, (db, "No personal config!\n"));
    }

    if(ps->exceptions){
      mprint(2, (db,
	     "Exceptions config \"%.100s\" comes from command line\n",
	     ps->exceptions));
    }

    /*
     * If not set, try specific file in same dir as pinerc.
     * Only use it if the file exists.
     */
    if(!ps->exceptions){
	p = last_cmpnt(ps->pinerc);
	buf[0] = '\0';
	if(p != NULL){
	    strncpy(buf, ps->pinerc, min(p - ps->pinerc, sizeof(buf)-1));
	    buf[min(p - ps->pinerc, sizeof(buf)-1)] = '\0';
	}

	strncat(buf, ".pinercex", sizeof(buf)-1-strlen(buf));
        mprint(2, (db, "Exceptions config not set on cmdline\n  checking for default \"%.100s\" in pinerc dir\n", buf));

	if(can_access(buf, ACCESS_EXISTS) == 0)		/* found it! */
	  ps->exceptions = cpystr(buf);

	if(ps->exceptions){
	  mprint(2, (db,
		 "  yes, exceptions config \"%.100s\" is default\n",
		 ps->exceptions));
	}
	else{
	  mprint(2, (db, "  no, there is no exceptions config\n"));
	}
    }

#endif /* unix */

    if(ps->exceptions){

	if(!IS_REMOTE(ps->exceptions) &&
	   !is_absolute_path(ps->exceptions)){
#if defined(DOS) || defined(OS2)
	    p = last_cmpnt(ps->pinerc);
	    buf[0] = '\0';
	    if(p != NULL){
		strncpy(buf, ps->pinerc, min(p - ps->pinerc, sizeof(buf)-1));
		buf[min(p - ps->pinerc, sizeof(buf)-1)] = '\0';
	    }

	    strncat(buf, ps->exceptions, sizeof(buf)-1-strlen(buf));
#else
	    build_path(buf, ps->home_dir, ps->exceptions, sizeof(buf));
#endif
	}
	else{
	    strncpy(buf, ps->exceptions, sizeof(buf)-1);
	    buf[sizeof(buf)-1] = '\0';
	}

	ps->post_prc = new_pinerc_s(buf);

	fs_give((void **)&ps->exceptions);
    }

    mprint(2, (db, "\n  Global config:     %.100s\n",
	   (ps->pconf && ps->pconf->name) ? ps->pconf->name : "<none>"));
    mprint(2, (db, "  Personal config:   %.100s\n",
	   (ps->prc && ps->prc->name) ? ps->prc->name : "<none>"));
    mprint(2, (db, "  Exceptions config: %.100s\n",
	   (ps->post_prc && ps->post_prc->name) ? ps->post_prc->name
						: "<none>"));
#if !defined(DOS) && !defined(OS2)
    if(SYSTEM_PINERC_FIXED){
      mprint(2, (db, "  Fixed config:      %.100s\n", SYSTEM_PINERC_FIXED));
    }
#endif

    mprint(2, (db, "\n"));
}
    

/*----------------------------------------------------------------------
     Initialize the variables

 Args:   ps   -- The usual pine structure

 Result: 

  This reads the system pine configuration file and the user's pine
configuration file ".pinerc" and places the results in the variables 
structure.  It sorts out what was read and sets a few other variables 
based on the contents.
  ----*/
void 
init_vars(ps)
     struct pine *ps;
{
    char	 buf[MAXPATH+1], *p, *q, **s;
    register struct variable *vars = ps->vars;
    int		 obs_header_in_reply,     /* the obs_ variables are to       */
		 obs_old_style_reply,     /* support backwards compatibility */
		 obs_save_by_sender, i, def_sort_rev;
    long         rvl;
    PINERC_S    *fixedprc = NULL;
    FeatureLevel obs_feature_level;

    dprint(5, (debugfile, "init_vars:\n"));

    /*--- The defaults here are defined in os-xxx.h so they can vary
          per machine ---*/

    GLO_PRINTER			= cpystr(DF_DEFAULT_PRINTER);
    GLO_ELM_STYLE_SAVE		= cpystr(DF_ELM_STYLE_SAVE);
    GLO_SAVE_BY_SENDER		= cpystr(DF_SAVE_BY_SENDER);
    GLO_HEADER_IN_REPLY		= cpystr(DF_HEADER_IN_REPLY);
    GLO_INBOX_PATH		= cpystr("inbox");
    GLO_DEFAULT_FCC		= cpystr(DF_DEFAULT_FCC);
    GLO_DEFAULT_SAVE_FOLDER	= cpystr(DEFAULT_SAVE);
    GLO_POSTPONED_FOLDER	= cpystr(POSTPONED_MSGS);
    GLO_USE_ONLY_DOMAIN_NAME	= cpystr(DF_USE_ONLY_DOMAIN_NAME);
    GLO_FEATURE_LEVEL		= cpystr(DF_FEATURE_LEVEL);
    GLO_OLD_STYLE_REPLY		= cpystr(DF_OLD_STYLE_REPLY);
    GLO_SORT_KEY		= cpystr(DF_SORT_KEY);
    GLO_SAVED_MSG_NAME_RULE	= cpystr(DF_SAVED_MSG_NAME_RULE);
    GLO_FCC_RULE		= cpystr(DF_FCC_RULE);
    GLO_AB_SORT_RULE		= cpystr(DF_AB_SORT_RULE);
    GLO_FLD_SORT_RULE		= cpystr(DF_FLD_SORT_RULE);
    GLO_SIGNATURE_FILE		= cpystr(DF_SIGNATURE_FILE);
    GLO_MAIL_DIRECTORY		= cpystr(DF_MAIL_DIRECTORY);
    GLO_REMOTE_ABOOK_HISTORY	= cpystr(DF_REMOTE_ABOOK_HISTORY);
    GLO_REMOTE_ABOOK_VALIDITY	= cpystr(DF_REMOTE_ABOOK_VALIDITY);
    GLO_GOTO_DEFAULT_RULE	= cpystr(DF_GOTO_DEFAULT_RULE);
    GLO_INCOMING_STARTUP	= cpystr(DF_INCOMING_STARTUP);
    GLO_PRUNING_RULE		= cpystr(DF_PRUNING_RULE);
    GLO_REOPEN_RULE		= cpystr(DF_REOPEN_RULE);
    GLO_THREAD_DISP_STYLE	= cpystr(DF_THREAD_DISP_STYLE);
    GLO_THREAD_INDEX_STYLE	= cpystr(DF_THREAD_INDEX_STYLE);
    GLO_THREAD_MORE_CHAR	= cpystr(DF_THREAD_MORE_CHAR);
    GLO_THREAD_EXP_CHAR		= cpystr(DF_THREAD_EXP_CHAR);
    GLO_THREAD_LASTREPLY_CHAR	= cpystr(DF_THREAD_LASTREPLY_CHAR);
    GLO_BUGS_FULLNAME		= cpystr("Sorry No Address");
    GLO_BUGS_ADDRESS		= cpystr("nobody");
    GLO_SUGGEST_FULLNAME	= cpystr("Sorry No Address");
    GLO_SUGGEST_ADDRESS		= cpystr("nobody");
    GLO_LOCAL_FULLNAME		= cpystr(DF_LOCAL_FULLNAME);
    GLO_LOCAL_ADDRESS		= cpystr(DF_LOCAL_ADDRESS);
    GLO_OVERLAP			= cpystr(DF_OVERLAP);
    GLO_MAXREMSTREAM		= cpystr(DF_MAXREMSTREAM);
    GLO_MARGIN			= cpystr(DF_MARGIN);
    GLO_FILLCOL			= cpystr(DF_FILLCOL);
    GLO_DEADLETS		= cpystr(DF_DEADLETS);
    GLO_NMW_WIDTH		= cpystr(DF_NMW_WIDTH);
    GLO_REPLY_STRING		= cpystr("> ");
    GLO_REPLY_INTRO		= cpystr(DEFAULT_REPLY_INTRO);
    GLO_EMPTY_HDR_MSG		= cpystr("undisclosed-recipients");
    GLO_STATUS_MSG_DELAY	= cpystr("0");
    GLO_USERINPUTTIMEO		= cpystr("0");
    GLO_MAILCHECK		= cpystr(DF_MAILCHECK);
    GLO_MAILCHECKNONCURR	= cpystr("0");
    GLO_MAILDROPCHECK		= cpystr(DF_MAILDROPCHECK);
    GLO_NNTPRANGE		= cpystr("0");
    GLO_KBLOCK_PASSWD_COUNT	= cpystr(DF_KBLOCK_PASSWD_COUNT);
    GLO_INDEX_COLOR_STYLE	= cpystr("flip-colors");
    GLO_TITLEBAR_COLOR_STYLE	= cpystr("default");
#ifdef	DF_FOLDER_EXTENSION
    GLO_FOLDER_EXTENSION	= cpystr(DF_FOLDER_EXTENSION);
#endif
#ifdef	DF_SMTP_SERVER
    GLO_SMTP_SERVER		= parse_list(DF_SMTP_SERVER, 1,
					     PL_REMSURRQUOT, NULL);
#endif
#ifdef	DF_SSHPATH
    GLO_SSHPATH			= cpystr(DF_SSHPATH);
#endif
#ifdef	DF_SSHCMD
    GLO_SSHCMD			= cpystr(DF_SSHCMD);
#endif
#ifndef	_WINDOWS
    GLO_COLOR_STYLE		= cpystr("no-color");
    GLO_NORM_FORE_COLOR		= cpystr(colorx(DEFAULT_NORM_FORE));
    GLO_NORM_BACK_COLOR		= cpystr(colorx(DEFAULT_NORM_BACK));
#endif
    GLO_VIEW_MARGIN_LEFT	= cpystr("0");
    GLO_VIEW_MARGIN_RIGHT	= cpystr(DF_VIEW_MARGIN_RIGHT);
    GLO_QUOTE_SUPPRESSION	= cpystr(DF_QUOTE_SUPPRESSION);
    GLO_KW_BRACES		= cpystr("\"{\" \"} \"");

    /*
     * Default first value for addrbook list if none set.
     * We also want to be sure to set global_val to the default
     * if is_fixed, so that address-book= will cause the default to happen.
     */
    if(!GLO_ADDRESSBOOK && !FIX_ADDRESSBOOK)
      GLO_ADDRESSBOOK = parse_list(DF_ADDRESSBOOK, 1, 0, NULL);

    /*
     * Default first value if none set.
     */
    if(!GLO_STANDARD_PRINTER && !FIX_STANDARD_PRINTER)
      GLO_STANDARD_PRINTER = parse_list(DF_STANDARD_PRINTER, 1, 0, NULL);

#if !defined(DOS) && !defined(OS2)
    /*
     * This is here instead of in init_pinerc so that we can get by without
     * having a global fixedprc, since we don't need it anymore after this.
     */
    fixedprc = new_pinerc_s(SYSTEM_PINERC_FIXED);
#endif

    /*
     * cache-remote-pinerc is experimental and unannounced as of 4.40.
     * (Set it with cmdline -feature-list=cache-remote-pinerc.)
     * This performance hack was put in before the stream re-use thing
     * was put in in pine_mail_open and pine_mail_close and it wasn't
     * redone. It needs some tuning if we want it to work smoothly with
     * the re-use of streams. hubert 2001-08-01
     * Performance hack. We're going to check to see if any of the pinercs
     * are remote. The problem with a remote pinerc is that we open a
     * connection to see if it exists, then open another connection to
     * check if we're up to date. So we're going to open a stream here
     * and try to re-use it. We use mail_stream because that is unused at
     * this point and the stream functions already know how to re-use it.
     */
    if(ps->pconf && ps->pconf->type == RemImap &&
       ps->cache_remote_pinerc){
	dprint(7, (debugfile,
	       "init_vars: preemptive halfopen of remote stream: %s\n",
	       ps->pconf->name ? ps->pconf->name : "?"));
	ps->mail_stream = pine_mail_open(NULL, ps->pconf->name,
					 OP_HALFOPEN|OP_SILENT|SP_USEPOOL,
					 NULL);
    }

    if(!ps->mail_stream && ps->prc &&
       ps->prc->type == RemImap && ps->cache_remote_pinerc){
	dprint(7, (debugfile,
	       "init_vars: preemptive halfopen of remote stream: %s\n",
	       ps->prc->name ? ps->prc->name : "?"));
	ps->mail_stream = pine_mail_open(NULL, ps->prc->name,
					 OP_HALFOPEN|OP_SILENT|SP_USEPOOL,
					 NULL);
    }

    if(!ps->mail_stream && fixedprc && fixedprc->type == RemImap &&
       ps->cache_remote_pinerc){
	dprint(7, (debugfile,
	       "init_vars: preemptive halfopen of remote stream: %s\n",
	       fixedprc->name ? fixedprc->name : "?"));
	ps->mail_stream = pine_mail_open(NULL, fixedprc->name,
					 OP_HALFOPEN|OP_SILENT|SP_USEPOOL,
					 NULL);
    }
    /* end performance hack */

    /*
     * If there are some remote config files and we're caching their contents,
     * we're going to need the name of the metadata file. We can find it in
     * the cache file for the personal pinerc. If we have a remote global
     * pinerc but local personal pinerc then we can find it in the real
     * personal pinerc.
     */
    if(ps->mail_stream)
      rd_grope_for_metaname();

    if(ps->pconf){
	read_pinerc(ps->pconf, vars, ParseGlobal);
	if(ps->pconf->type != Loc)
	  rd_close_remote(ps->pconf->rd);
    }

    if(ps->prc){
	read_pinerc(ps->prc, vars, ParsePers);
	if(ps->prc->type != Loc)
	  rd_close_remote(ps->prc->rd);
    }

    if(ps->post_prc){
	read_pinerc(ps->post_prc, vars, ParsePersPost);
	if(ps->post_prc->type != Loc)
	  rd_close_remote(ps->post_prc->rd);
    }

    if(fixedprc){
	read_pinerc(fixedprc, vars, ParseFixed);
	free_pinerc_s(&fixedprc);
    }

    ps->ew_for_except_vars = ps->post_prc ? Post : Main;

    /*
     * If we already set this while reading the remote pinerc, don't
     * change it.
     */
    if(!VAR_REMOTE_ABOOK_METADATA || !VAR_REMOTE_ABOOK_METADATA[0])
      set_current_val(&vars[V_REMOTE_ABOOK_METADATA], TRUE, TRUE);

    if(ps->pconf && ps->pconf->type != Loc &&
       ps->pconf->rd && ps->pconf->rd->flags & DERIVE_CACHE){
	char *metafile;

	ps->pconf->rd->flags &= ~DERIVE_CACHE;
	/* if the metadata name still isn't set, we'll create it here */
	if((metafile = rd_metadata_name()) != NULL){
	    fs_give((void **)&metafile);
	    rd_write_metadata(ps->pconf->rd, 0);
	}
    }

    if(ps->prc && ps->prc->type != Loc &&
       ps->prc->rd && ps->prc->rd->flags & DERIVE_CACHE){
	char *metafile;

	ps->prc->rd->flags &= ~DERIVE_CACHE;
	/* if the metadata name still isn't set, we'll create it here */
	if((metafile = rd_metadata_name()) != NULL){
	    fs_give((void **)&metafile);
	    rd_write_metadata(ps->prc->rd, 0);
	}
    }

    /* clean up after performance hack */
    if(ps->mail_stream){
	pine_mail_close(ps->mail_stream);
	ps->mail_stream = NULL;
    }


    if(ps->exit_if_no_pinerc && ps->first_time_user){

	fprintf(stderr,
	"Exiting because -bail option is set and config file doesn't exist.\n");
	exit(-1);
    }

    /*
     * mail-directory variable is obsolete, put its value in
     * default folder-collection list
     */
    set_current_val(&vars[V_MAIL_DIRECTORY], TRUE, TRUE);
    if(!GLO_FOLDER_SPEC){
	build_path(tmp_20k_buf, VAR_MAIL_DIRECTORY, "[]", SIZEOF_20KBUF);
	GLO_FOLDER_SPEC = parse_list(tmp_20k_buf, 1, 0, NULL);
    }

    set_current_val(&vars[V_FOLDER_SPEC], TRUE, TRUE);

    set_current_val(&vars[V_NNTP_SERVER], TRUE, TRUE);
    for(i = 0; VAR_NNTP_SERVER && VAR_NNTP_SERVER[i]; i++)
      removing_quotes(VAR_NNTP_SERVER[i]);

    set_news_spec_current_val(TRUE, TRUE);

    set_current_val(&vars[V_INBOX_PATH], TRUE, TRUE);

    set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
    if(VAR_USER_DOMAIN
       && VAR_USER_DOMAIN[0]
       && (p = strrindex(VAR_USER_DOMAIN, '@'))){
	if(*(++p)){
	    char *q;

	    sprintf(tmp_20k_buf,
		    "User-domain (%s) cannot contain \"@\", using \"%s\"",
		    VAR_USER_DOMAIN, p);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	    q = VAR_USER_DOMAIN;
	    while((*q++ = *p++) != '\0')
	      ;/* do nothing */
	}
	else{
	    sprintf(tmp_20k_buf,
		    "User-domain (%s) cannot contain \"@\", deleting",
		    VAR_USER_DOMAIN);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	    if(ps->vars[V_USER_DOMAIN].post_user_val.p){
		fs_give((void **)&ps->vars[V_USER_DOMAIN].post_user_val.p);
		set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
	    }

	    if(VAR_USER_DOMAIN
	       && VAR_USER_DOMAIN[0]
	       && (p = strrindex(VAR_USER_DOMAIN, '@'))){
		if(ps->vars[V_USER_DOMAIN].main_user_val.p){
		    fs_give((void **)&ps->vars[V_USER_DOMAIN].main_user_val.p);
		    set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
		}
	    }
	}
    }

    set_current_val(&vars[V_USE_ONLY_DOMAIN_NAME], TRUE, TRUE);
    set_current_val(&vars[V_REPLY_STRING], TRUE, TRUE);
    set_current_val(&vars[V_QUOTE_REPLACE_STRING], TRUE, TRUE);
    set_current_val(&vars[V_REPLY_INTRO], TRUE, TRUE);
    set_current_val(&vars[V_EMPTY_HDR_MSG], TRUE, TRUE);

#ifdef	ENABLE_LDAP
    set_current_val(&vars[V_LDAP_SERVERS], TRUE, TRUE);
#endif	/* ENABLE_LDAP */

    /* obsolete, backwards compatibility */
    set_current_val(&vars[V_HEADER_IN_REPLY], TRUE, TRUE);
    obs_header_in_reply=!strucmp(VAR_HEADER_IN_REPLY, "yes");

    set_current_val(&vars[V_PERSONAL_PRINT_COMMAND], TRUE, TRUE);
    set_current_val(&vars[V_STANDARD_PRINTER], TRUE, TRUE);
    set_current_val(&vars[V_PRINTER], TRUE, TRUE);
    /*
     * We don't want the user to be able to edit their pinerc and set
     * printer to whatever they want if personal-print-command is fixed.
     * So make sure printer is set to something legitimate.
     */
    if(vars[V_PERSONAL_PRINT_COMMAND].is_fixed && !vars[V_PRINTER].is_fixed){
	char **tt;
	char   aname[100], wname[100];
	int    ok = 0;

	strncat(strncpy(aname, ANSI_PRINTER, 60), "-no-formfeed", 30);
	strncat(strncpy(wname, WYSE_PRINTER, 60), "-no-formfeed", 30);
	if(strucmp(VAR_PRINTER, ANSI_PRINTER) == 0
	  || strucmp(VAR_PRINTER, aname) == 0
	  || strucmp(VAR_PRINTER, WYSE_PRINTER) == 0
	  || strucmp(VAR_PRINTER, wname) == 0)
	  ok++;
	else if(VAR_STANDARD_PRINTER && VAR_STANDARD_PRINTER[0]){
	    for(tt = VAR_STANDARD_PRINTER; *tt; tt++)
	      if(strucmp(VAR_PRINTER, *tt) == 0)
		break;
	    
	    if(*tt)
	      ok++;
	}

	if(!ok){
	    char            *val;
	    struct variable *v;

	    if(VAR_STANDARD_PRINTER && VAR_STANDARD_PRINTER[0])
	      val = VAR_STANDARD_PRINTER[0];
	    else
	      val = ANSI_PRINTER;
	    
	    v = &vars[V_PRINTER];
	    if(v->main_user_val.p)
	      fs_give((void **)&v->main_user_val.p);
	    if(v->post_user_val.p)
	      fs_give((void **)&v->post_user_val.p);
	    if(v->current_val.p)
	      fs_give((void **)&v->current_val.p);
	    
	    v->main_user_val.p = cpystr(val);
	    v->current_val.p = cpystr(val);
	}
    }

    set_current_val(&vars[V_LAST_TIME_PRUNE_QUESTION], TRUE, TRUE);
    if(VAR_LAST_TIME_PRUNE_QUESTION != NULL){
        /* The month value in the file runs from 1-12, the variable here
           runs from 0-11; the value in the file used to be 0-11, but we're 
           fixing it in January */
        ps->last_expire_year  = atoi(VAR_LAST_TIME_PRUNE_QUESTION);
        ps->last_expire_month =
			atoi(strindex(VAR_LAST_TIME_PRUNE_QUESTION, '.') + 1);
        if(ps->last_expire_month == 0){
            /* Fix for 0 because of old bug */
            sprintf(buf, "%d.%d", ps_global->last_expire_year,
              ps_global->last_expire_month + 1);
            set_variable(V_LAST_TIME_PRUNE_QUESTION, buf, 1, 1, Main);
        }else{
            ps->last_expire_month--; 
        } 
    }else{
        ps->last_expire_year  = -1;
        ps->last_expire_month = -1;
    }

    set_current_val(&vars[V_BUGS_FULLNAME], TRUE, TRUE);
    set_current_val(&vars[V_BUGS_ADDRESS], TRUE, TRUE);
    set_current_val(&vars[V_SUGGEST_FULLNAME], TRUE, TRUE);
    set_current_val(&vars[V_SUGGEST_ADDRESS], TRUE, TRUE);
    set_current_val(&vars[V_LOCAL_FULLNAME], TRUE, TRUE);
    set_current_val(&vars[V_LOCAL_ADDRESS], TRUE, TRUE);
    set_current_val(&vars[V_BUGS_EXTRAS], TRUE, TRUE);
    set_current_val(&vars[V_KBLOCK_PASSWD_COUNT], TRUE, TRUE);
    set_current_val(&vars[V_DEFAULT_FCC], TRUE, TRUE);
    set_current_val(&vars[V_POSTPONED_FOLDER], TRUE, TRUE);
    set_current_val(&vars[V_READ_MESSAGE_FOLDER], TRUE, TRUE);
    set_current_val(&vars[V_FORM_FOLDER], TRUE, TRUE);
    set_current_val(&vars[V_EDITOR], TRUE, TRUE);
    set_current_val(&vars[V_SPELLER], TRUE, TRUE);
    set_current_val(&vars[V_IMAGE_VIEWER], TRUE, TRUE);
    set_current_val(&vars[V_BROWSER], TRUE, TRUE);
    set_current_val(&vars[V_SMTP_SERVER], TRUE, TRUE);
    set_current_val(&vars[V_COMP_HDRS], TRUE, TRUE);
    set_current_val(&vars[V_CUSTOM_HDRS], TRUE, TRUE);
    set_current_val(&vars[V_SENDMAIL_PATH], TRUE, TRUE);
    set_current_val(&vars[V_DISPLAY_FILTERS], TRUE, TRUE);
    set_current_val(&vars[V_SEND_FILTER], TRUE, TRUE);
    set_current_val(&vars[V_ALT_ADDRS], TRUE, TRUE);
    set_current_val(&vars[V_ABOOK_FORMATS], TRUE, TRUE);
    set_current_val(&vars[V_KW_BRACES], TRUE, TRUE);

    set_current_val(&vars[V_KEYWORDS], TRUE, TRUE);
    ps_global->keywords = init_keyword_list(VAR_KEYWORDS);

    set_current_val(&vars[V_OPER_DIR], TRUE, TRUE);
    if(VAR_OPER_DIR && !VAR_OPER_DIR[0]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
 "Setting operating-dir to the empty string is not allowed.  Will be ignored.");
	fs_give((void **)&VAR_OPER_DIR);
	if(FIX_OPER_DIR)
	  fs_give((void **)&FIX_OPER_DIR);
	if(GLO_OPER_DIR)
	  fs_give((void **)&GLO_OPER_DIR);
	if(COM_OPER_DIR)
	  fs_give((void **)&COM_OPER_DIR);
	if(ps_global->vars[V_OPER_DIR].post_user_val.p)
	  fs_give((void **)&ps_global->vars[V_OPER_DIR].post_user_val.p);
	if(ps_global->vars[V_OPER_DIR].main_user_val.p)
	  fs_give((void **)&ps_global->vars[V_OPER_DIR].main_user_val.p);
    }

    set_current_val(&vars[V_INDEX_FORMAT], TRUE, TRUE);
    init_index_format(VAR_INDEX_FORMAT, &ps->index_disp_format);

    set_current_val(&vars[V_PERSONAL_PRINT_CATEGORY], TRUE, TRUE);
    ps->printer_category = -1;
    if(VAR_PERSONAL_PRINT_CATEGORY != NULL)
      ps->printer_category = atoi(VAR_PERSONAL_PRINT_CATEGORY);

    if(ps->printer_category < 1 || ps->printer_category > 3){
	char **tt;
	char aname[100], wname[100];

	strncat(strncpy(aname, ANSI_PRINTER, 60), "-no-formfeed", 30);
	strncat(strncpy(wname, WYSE_PRINTER, 60), "-no-formfeed", 30);
	if(strucmp(VAR_PRINTER, ANSI_PRINTER) == 0
	  || strucmp(VAR_PRINTER, aname) == 0
	  || strucmp(VAR_PRINTER, WYSE_PRINTER) == 0
	  || strucmp(VAR_PRINTER, wname) == 0)
	  ps->printer_category = 1;
	else if(VAR_STANDARD_PRINTER && VAR_STANDARD_PRINTER[0]){
	    for(tt = VAR_STANDARD_PRINTER; *tt; tt++)
	      if(strucmp(VAR_PRINTER, *tt) == 0)
		break;
	    
	    if(*tt)
	      ps->printer_category = 2;
	}

	/* didn't find it yet */
	if(ps->printer_category < 1 || ps->printer_category > 3){
	    if(VAR_PERSONAL_PRINT_COMMAND && VAR_PERSONAL_PRINT_COMMAND[0]){
		for(tt = VAR_PERSONAL_PRINT_COMMAND; *tt; tt++)
		  if(strucmp(VAR_PRINTER, *tt) == 0)
		    break;
		
		if(*tt)
		  ps->printer_category = 3;
	    }
	}
    }

    set_current_val(&vars[V_OVERLAP], TRUE, TRUE);
    ps->viewer_overlap = i = atoi(DF_OVERLAP);
    if(SVAR_OVERLAP(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->viewer_overlap = i;

    set_current_val(&vars[V_MARGIN], TRUE, TRUE);
    ps->scroll_margin = i = atoi(DF_MARGIN);
    if(SVAR_MARGIN(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->scroll_margin = i;

    set_current_val(&vars[V_FILLCOL], TRUE, TRUE);
    ps->composer_fillcol = i = atoi(DF_FILLCOL);
    if(SVAR_FILLCOL(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->composer_fillcol = i;

    set_current_val(&vars[V_QUOTE_SUPPRESSION], TRUE, TRUE);
    ps->quote_suppression_threshold = i = atoi(DF_QUOTE_SUPPRESSION);
    if(SVAR_QUOTE_SUPPRESSION(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else{
	if(i > 0 && i < Q_SUPP_LIMIT){
	    sprintf(tmp_20k_buf,
		"Ignoring Quote-Suppression-Threshold value of %.50s, see help",
		VAR_QUOTE_SUPPRESSION);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	}
	else{
	    if(i < 0 && i != Q_DEL_ALL)
	      ps->quote_suppression_threshold = -i;
	    else
	      ps->quote_suppression_threshold = i;
	}
    }
    
    set_current_val(&vars[V_DEADLETS], TRUE, TRUE);
    ps->deadlets = i = atoi(DF_DEADLETS);
    if(SVAR_DEADLETS(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->deadlets = i;
    
    set_current_val(&vars[V_STATUS_MSG_DELAY], TRUE, TRUE);
    ps->status_msg_delay = i = 0;
    if(SVAR_MSGDLAY(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->status_msg_delay = i;

    set_current_val(&vars[V_REMOTE_ABOOK_HISTORY], TRUE, TRUE);
    ps->remote_abook_history = i = atoi(DF_REMOTE_ABOOK_HISTORY);
    if(SVAR_AB_HIST(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->remote_abook_history = i;

    set_current_val(&vars[V_REMOTE_ABOOK_VALIDITY], TRUE, TRUE);
    ps->remote_abook_validity = i = atoi(DF_REMOTE_ABOOK_VALIDITY);
    if(SVAR_AB_VALID(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->remote_abook_validity = i;

    set_current_val(&vars[V_USERINPUTTIMEO], TRUE, TRUE);
    ps->hours_to_timeout = i = 0;
    if(SVAR_USER_INPUT(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->hours_to_timeout = i;

    /* timeo is a regular extern int because it is referenced in pico */
    set_current_val(&vars[V_MAILCHECK], TRUE, TRUE);
    timeo = i = 15;
    if(SVAR_MAILCHK(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      timeo = i;

    set_current_val(&vars[V_MAILCHECKNONCURR], TRUE, TRUE);
    ps->check_interval_for_noncurr = i = 0;
    if(SVAR_MAILCHKNONCURR(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->check_interval_for_noncurr = i;

#ifdef DEBUGJOURNAL
    ps->debugmem = 1;
#else
    ps->debugmem = 0;
#endif

    i = 30;
    set_current_val(&vars[V_TCPOPENTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_TCPOPENTIMEO && SVAR_TCP_OPEN(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 15;
    set_current_val(&vars[V_TCPREADWARNTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_TCPREADWARNTIMEO && SVAR_TCP_READWARN(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 0;
    set_current_val(&vars[V_TCPWRITEWARNTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_TCPWRITEWARNTIMEO && SVAR_TCP_WRITEWARN(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 15;
    set_current_val(&vars[V_RSHOPENTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_RSHOPENTIMEO && SVAR_RSH_OPEN(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    i = 15;
    set_current_val(&vars[V_SSHOPENTIMEO], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_SSHOPENTIMEO && SVAR_SSH_OPEN(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    rvl = 60L;
    set_current_val(&vars[V_MAILDROPCHECK], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_MAILDROPCHECK && SVAR_MAILDCHK(ps, rvl, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    rvl = 0L;
    set_current_val(&vars[V_NNTPRANGE], TRUE, TRUE);
    /* this is just for the error, we don't save the result */
    if(VAR_NNTPRANGE && SVAR_NNTPRANGE(ps, rvl, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);

    set_current_val(&vars[V_TCPQUERYTIMEO], TRUE, TRUE);
    ps->tcp_query_timeout = i = TO_BAIL_THRESHOLD;
    if(VAR_TCPQUERYTIMEO && SVAR_TCP_QUERY(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->tcp_query_timeout = i;

    set_current_val(&vars[V_NEWSRC_PATH], TRUE, TRUE);
    if(VAR_NEWSRC_PATH && VAR_NEWSRC_PATH[0])
      mail_parameters(NULL, SET_NEWSRC, (void *)VAR_NEWSRC_PATH);

    set_current_val(&vars[V_NEWS_ACTIVE_PATH], TRUE, TRUE);
    if(VAR_NEWS_ACTIVE_PATH)
      mail_parameters(NULL, SET_NEWSACTIVE,
		      (void *)VAR_NEWS_ACTIVE_PATH);

    set_current_val(&vars[V_NEWS_SPOOL_DIR], TRUE, TRUE);
    if(VAR_NEWS_SPOOL_DIR)
      mail_parameters(NULL, SET_NEWSSPOOL,
		      (void *)VAR_NEWS_SPOOL_DIR);

    /* guarantee a save default */
    set_current_val(&vars[V_DEFAULT_SAVE_FOLDER], TRUE, TRUE);
    if(!VAR_DEFAULT_SAVE_FOLDER || !VAR_DEFAULT_SAVE_FOLDER[0])
      set_variable(V_DEFAULT_SAVE_FOLDER,
		   (GLO_DEFAULT_SAVE_FOLDER && GLO_DEFAULT_SAVE_FOLDER[0])
		     ? GLO_DEFAULT_SAVE_FOLDER
		     : DEFAULT_SAVE, 1, 0, Main);

    /* obsolete, backwards compatibility */
    set_current_val(&vars[V_FEATURE_LEVEL], TRUE, TRUE);
    if(strucmp(VAR_FEATURE_LEVEL, "seedling") == 0)
      obs_feature_level = Seedling;
    else if(strucmp(VAR_FEATURE_LEVEL, "old-growth") == 0)
      obs_feature_level = Seasoned;
    else
      obs_feature_level = Sapling;

    /* obsolete, backwards compatibility */
    set_current_val(&vars[V_OLD_STYLE_REPLY], TRUE, TRUE);
    obs_old_style_reply = !strucmp(VAR_OLD_STYLE_REPLY, "yes");

    set_feature_list_current_val(&vars[V_FEATURE_LIST]);
    process_feature_list(ps, VAR_FEATURE_LIST,
           (obs_feature_level == Seasoned) ? 1 : 0,
	   obs_header_in_reply, obs_old_style_reply);

    set_current_val(&vars[V_SIGNATURE_FILE], TRUE, TRUE);
    set_current_val(&vars[V_LITERAL_SIG], TRUE, TRUE);
    set_current_val(&vars[V_CHAR_SET], TRUE, TRUE);
    set_current_val(&vars[V_GLOB_ADDRBOOK], TRUE, TRUE);
    set_current_val(&vars[V_ADDRESSBOOK], TRUE, TRUE);
    set_current_val(&vars[V_FORCED_ABOOK_ENTRY], TRUE, TRUE);
    set_current_val(&vars[V_DISABLE_DRIVERS], TRUE, TRUE);
    set_current_val(&vars[V_DISABLE_AUTHS], TRUE, TRUE);

    set_current_val(&vars[V_VIEW_HEADERS], TRUE, TRUE);
    /* strip spaces and colons */
    if(ps->VAR_VIEW_HEADERS){
	for(s = ps->VAR_VIEW_HEADERS; (q = *s) != NULL; s++){
	    if(q[0]){
		removing_leading_white_space(q);
		/* look for colon or space or end */
		for(p = q; *p && !isspace((unsigned char)*p) && *p != ':'; p++)
		  ;/* do nothing */
		
		*p = '\0';
		if(strucmp(q, ALL_EXCEPT) == 0)
		  ps->view_all_except = 1;
	    }
	}
    }

    set_current_val(&vars[V_VIEW_MARGIN_LEFT], TRUE, TRUE);
    set_current_val(&vars[V_VIEW_MARGIN_RIGHT], TRUE, TRUE);
    set_current_val(&vars[V_UPLOAD_CMD], TRUE, TRUE);
    set_current_val(&vars[V_UPLOAD_CMD_PREFIX], TRUE, TRUE);
    set_current_val(&vars[V_DOWNLOAD_CMD], TRUE, TRUE);
    set_current_val(&vars[V_DOWNLOAD_CMD_PREFIX], TRUE, TRUE);
    set_current_val(&vars[V_MAILCAP_PATH], TRUE, TRUE);
    set_current_val(&vars[V_MIMETYPE_PATH], TRUE, TRUE);
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    set_current_val(&vars[V_FIFOPATH], TRUE, TRUE);
#endif

    set_current_val(&vars[V_RSHPATH], TRUE, TRUE);
    if(VAR_RSHPATH
       && is_absolute_path(VAR_RSHPATH)
       && can_access(VAR_RSHPATH, EXECUTE_ACCESS) == 0){
	mail_parameters(NULL, SET_RSHPATH, (void *) VAR_RSHPATH);
    }

    set_current_val(&vars[V_RSHCMD], TRUE, TRUE);
    if(VAR_RSHCMD){
	mail_parameters(NULL, SET_RSHCOMMAND, (void *) VAR_RSHCMD);
    }

    set_current_val(&vars[V_SSHPATH], TRUE, TRUE);
    if(VAR_SSHPATH
       && is_absolute_path(VAR_SSHPATH)
       && can_access(VAR_SSHPATH, EXECUTE_ACCESS) == 0){
	mail_parameters(NULL, SET_SSHPATH, (void *) VAR_SSHPATH);
    }

    set_current_val(&vars[V_SSHCMD], TRUE, TRUE);
    if(VAR_SSHCMD){
	mail_parameters(NULL, SET_SSHCOMMAND, (void *) VAR_SSHCMD);
    }

#if	defined(DOS) || defined(OS2)

    set_current_val(&vars[V_FILE_DIR], TRUE, TRUE);

#ifdef	_WINDOWS
    set_current_val(&vars[V_FONT_NAME], TRUE, TRUE);
    set_current_val(&vars[V_FONT_SIZE], TRUE, TRUE);
    set_current_val(&vars[V_FONT_STYLE], TRUE, TRUE);
    set_current_val(&vars[V_FONT_CHAR_SET], TRUE, TRUE);
    set_current_val(&vars[V_CURSOR_STYLE], TRUE, TRUE);
    set_current_val(&vars[V_WINDOW_POSITION], TRUE, TRUE);

    if(F_OFF(F_STORE_WINPOS_IN_CONFIG, ps_global)){
	/* if win position is in the registry, use it */
	if(mswin_reg(MSWR_OP_GET, MSWR_PINE_POS, buf, sizeof(buf))){
	    if(VAR_WINDOW_POSITION)
	      fs_give((void **)&VAR_WINDOW_POSITION);
	    
	    VAR_WINDOW_POSITION = cpystr(buf);
	}
	else if(VAR_WINDOW_POSITION
	    && (ps->update_registry != UREG_NEVER_SET)){
	    /* otherwise, put it there */
	    mswin_reg(MSWR_OP_SET | ((ps->update_registry == UREG_ALWAYS_SET)
				     ? MSWR_OP_FORCE : 0),
		      MSWR_PINE_POS, 
		      VAR_WINDOW_POSITION, NULL);
	}
    }

    mswin_setwindow (VAR_FONT_NAME, VAR_FONT_SIZE, 
		     VAR_FONT_STYLE, VAR_WINDOW_POSITION,
		     VAR_CURSOR_STYLE, VAR_FONT_CHAR_SET);

    /* this is no longer used */
    if(VAR_WINDOW_POSITION)
      fs_give((void **)&VAR_WINDOW_POSITION);

    set_current_val(&vars[V_PRINT_FONT_NAME], TRUE, TRUE);
    set_current_val(&vars[V_PRINT_FONT_SIZE], TRUE, TRUE);
    set_current_val(&vars[V_PRINT_FONT_STYLE], TRUE, TRUE);
    set_current_val(&vars[V_PRINT_FONT_CHAR_SET], TRUE, TRUE);
    mswin_setprintfont (VAR_PRINT_FONT_NAME,
			VAR_PRINT_FONT_SIZE,
			VAR_PRINT_FONT_STYLE,
			VAR_PRINT_FONT_CHAR_SET);

    mswin_setgenhelptextcallback(pcpine_general_help);

    mswin_setclosetext ("Use the \"Q\" command to exit Pine.");

    {
	char foreColor[64], backColor[64];

	mswin_getwindow (NULL, NULL, NULL, NULL, foreColor, backColor,
			 NULL, NULL);
	if(!GLO_NORM_FORE_COLOR)
	  GLO_NORM_FORE_COLOR = cpystr(foreColor);

	if(!GLO_NORM_BACK_COLOR)
	  GLO_NORM_BACK_COLOR = cpystr(backColor);
    }
#endif	/* _WINDOWS */
#endif	/* DOS */

    set_current_val(&vars[V_LAST_VERS_USED], TRUE, TRUE);
    /* Check for special cases first */
    if(VAR_LAST_VERS_USED
          /* Special Case #1: 3.92 use is effectively the same as 3.92 */
       && (strcmp(VAR_LAST_VERS_USED, "3.92") == 0
	   /*
	    * Special Case #2:  We're not really a new version if our
	    * version number looks like: <number><dot><number><number><alpha>
	    * The <alpha> on the end is key meaning its just a bug-fix patch.
	    */
	   || (isdigit((unsigned char)pine_version[0])
	       && pine_version[1] == '.'
	       && isdigit((unsigned char)pine_version[2])
	       && isdigit((unsigned char)pine_version[3])
	       && isalpha((unsigned char)pine_version[4])
	       && strncmp(VAR_LAST_VERS_USED, pine_version, 4) >= 0))){
	ps->show_new_version = 0;
    }
    /* Otherwise just do lexicographic comparision... */
    else if(VAR_LAST_VERS_USED
	    && strcmp(VAR_LAST_VERS_USED, pine_version) >= 0){
	ps->show_new_version = 0;
    }
    else{
        ps->pre390 = !(VAR_LAST_VERS_USED
		       && strcmp(VAR_LAST_VERS_USED, "3.90") >= 0);

#ifdef	_WINDOWS
	/*
	 * If this is the first time we've run a version > 4.40, and there
	 * is evidence that the config file has not been used by unix pine,
	 * then we convert color008 to colorlgr, color009 to colormgr, and
	 * color010 to colordgr. If the config file is being used by
	 * unix pine then color009 may really supposed to be red, etc.
	 * Same if we've already run 4.41 or higher. We don't have to do
	 * anything if we are new to pine.
	 */
	ps->pre441 = (VAR_LAST_VERS_USED
		      && strcmp(VAR_LAST_VERS_USED, "4.40") <= 0);
#endif	/* _WINDOWS */

	/*
	 * Don't offer the new version message if we're told not to.
	 */
	set_current_val(&vars[V_NEW_VER_QUELL], TRUE, TRUE);
	ps->show_new_version = !(VAR_NEW_VER_QUELL
			         && strcmp(pine_version,
					   VAR_NEW_VER_QUELL) < 0);

#ifdef _WINDOWS
	if(!ps_global->install_flag)
#endif /* _WINDOWS */
	{
	if(VAR_LAST_VERS_USED){
	    strncpy(ps_global->pine_pre_vers, VAR_LAST_VERS_USED,
		    sizeof(ps_global->pine_pre_vers));
	    ps_global->pine_pre_vers[sizeof(ps_global->pine_pre_vers)-1] = '\0';
	}

	set_variable(V_LAST_VERS_USED, pine_version, 1, 1,
		     ps_global->ew_for_except_vars);
	}
    }

    /* Obsolete, backwards compatibility */
    set_current_val(&vars[V_ELM_STYLE_SAVE], TRUE, TRUE);
    /* Also obsolete */
    set_current_val(&vars[V_SAVE_BY_SENDER], TRUE, TRUE);
    if(!strucmp(VAR_ELM_STYLE_SAVE, "yes"))
      set_variable(V_SAVE_BY_SENDER, "yes", 1, 1, Main);
    obs_save_by_sender = !strucmp(VAR_SAVE_BY_SENDER, "yes");

    set_current_pattern_vals(ps);

    /* this should come after pre441 is set or not */
    set_current_color_vals(ps);

    set_current_val(&vars[V_PRUNED_FOLDERS], TRUE, TRUE);
    set_current_val(&vars[V_ARCHIVED_FOLDERS], TRUE, TRUE);
    set_current_val(&vars[V_INCOMING_FOLDERS], TRUE, TRUE);
    set_current_val(&vars[V_SORT_KEY], TRUE, TRUE);
    if(decode_sort(VAR_SORT_KEY, &ps->def_sort, &def_sort_rev) == -1){
	sprintf(tmp_20k_buf, "Sort type \"%.200s\" is invalid", VAR_SORT_KEY);
	init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	ps->def_sort = SortArrival;
	ps->def_sort_rev = 0;
    }
    else
      ps->def_sort_rev = def_sort_rev;

    cur_rule_value(&vars[V_SAVED_MSG_NAME_RULE], TRUE, TRUE);
    {NAMEVAL_S *v; int i;
    for(i = 0; v = save_msg_rules(i); i++)
      if(v->value == ps_global->save_msg_rule)
	break;
     
     /* if save_msg_rule is not default, or is explicitly set to default */
     if((ps_global->save_msg_rule != MSG_RULE_DEFLT) ||
	(v && v->name &&
	 (!strucmp(ps_global->vars[V_SAVED_MSG_NAME_RULE].post_user_val.p,
		   v->name) ||
	  !strucmp(ps_global->vars[V_SAVED_MSG_NAME_RULE].main_user_val.p,
		   v->name))))
       obs_save_by_sender = 0;  /* don't overwrite */
    }

    cur_rule_value(&vars[V_FCC_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_AB_SORT_RULE], TRUE, TRUE);

#ifndef	_WINDOWS
    cur_rule_value(&vars[V_COLOR_STYLE], TRUE, TRUE);
#endif

    cur_rule_value(&vars[V_INDEX_COLOR_STYLE], TRUE, TRUE);
    cur_rule_value(&vars[V_TITLEBAR_COLOR_STYLE], TRUE, TRUE);
    cur_rule_value(&vars[V_FLD_SORT_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_INCOMING_STARTUP], TRUE, TRUE);
    cur_rule_value(&vars[V_PRUNING_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_REOPEN_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_GOTO_DEFAULT_RULE], TRUE, TRUE);
    cur_rule_value(&vars[V_THREAD_DISP_STYLE], TRUE, TRUE);
    cur_rule_value(&vars[V_THREAD_INDEX_STYLE], TRUE, TRUE);

    set_current_val(&vars[V_THREAD_MORE_CHAR], TRUE, TRUE);
    if(VAR_THREAD_MORE_CHAR[0] && VAR_THREAD_MORE_CHAR[1]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
	  "Only using first character of threading-indicator-character option");
	VAR_THREAD_MORE_CHAR[1] = '\0';
    }

    set_current_val(&vars[V_THREAD_EXP_CHAR], TRUE, TRUE);
    if(VAR_THREAD_EXP_CHAR[0] && VAR_THREAD_EXP_CHAR[1]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
	   "Only using first character of threading-expanded-character option");
	VAR_THREAD_EXP_CHAR[1] = '\0';
    }

    set_current_val(&vars[V_THREAD_LASTREPLY_CHAR], TRUE, TRUE);
    if(!VAR_THREAD_LASTREPLY_CHAR[0])
      VAR_THREAD_LASTREPLY_CHAR = cpystr(DF_THREAD_LASTREPLY_CHAR);

    if(VAR_THREAD_LASTREPLY_CHAR[0] && VAR_THREAD_LASTREPLY_CHAR[1]){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
	  "Only using first character of threading-lastreply-character option");
	VAR_THREAD_LASTREPLY_CHAR[1] = '\0';
    }

    set_current_val(&vars[V_MAXREMSTREAM], TRUE, TRUE);
    ps->s_pool.max_remstream = i = atoi(DF_MAXREMSTREAM);
    if(SVAR_MAXREMSTREAM(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->s_pool.max_remstream = i;
    
    set_current_val(&vars[V_PERMLOCKED], TRUE, TRUE);

    set_current_val(&vars[V_NMW_WIDTH], TRUE, TRUE);
    ps->nmw_width = i = atoi(DF_NMW_WIDTH);
    if(SVAR_NMW_WIDTH(ps, i, tmp_20k_buf))
      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    else
      ps->nmw_width = i;

    /* backwards compatibility */
    if(obs_save_by_sender){
        ps->save_msg_rule = MSG_RULE_FROM;
	set_variable(V_SAVED_MSG_NAME_RULE, "by-from", 1, 1, Main);
    }

    /* this should come after process_feature_list because of use_fkeys */
    if(!ps->start_in_index)
        set_current_val(&vars[V_INIT_CMD_LIST], FALSE, TRUE);
    if(VAR_INIT_CMD_LIST && VAR_INIT_CMD_LIST[0] && VAR_INIT_CMD_LIST[0][0])
        process_init_cmds(ps, VAR_INIT_CMD_LIST);

#ifdef	_WINDOWS
    mswin_set_quit_confirm (F_OFF(F_QUIT_WO_CONFIRM, ps_global));
    if(ps_global->update_registry != UREG_NEVER_SET){
	mswin_reg(MSWR_OP_SET
		  | ((ps_global->update_registry == UREG_ALWAYS_SET)
		   ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_DIR, ps_global->pine_dir, NULL);
	mswin_reg(MSWR_OP_SET
		  | ((ps_global->update_registry == UREG_ALWAYS_SET)
		     ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_EXE, ps_global->pine_name, NULL);
    }
#endif	/* _WINDOWS */

#ifdef DEBUG
    if(debugfile && debug){
	gf_io_t pc;

	gf_set_writec(&pc, debugfile, 0L, FileStar);
	if(do_debug(debugfile))
	  dump_config(ps, pc, 0);
    }
#endif /* DEBUG */
}


/*
 * free_vars -- give back resources acquired when we defined the
 *		variables list
 */
void
free_vars(ps)
    struct pine *ps;
{
    register int i;

    for(i = 0; ps && i <= V_LAST_VAR; i++)
      free_variable_values(&ps->vars[i]);
}


void
free_variable_values(var)
    struct variable *var;
{
    if(var){
	if(var->is_list){
	    free_list_array(&var->current_val.l);
	    free_list_array(&var->main_user_val.l);
	    free_list_array(&var->post_user_val.l);
	    free_list_array(&var->global_val.l);
	    free_list_array(&var->fixed_val.l);
	    free_list_array(&var->cmdline_val.l);
	}
	else{
	    if(var->current_val.p)
	      fs_give((void **)&var->current_val.p);
	    if(var->main_user_val.p)
	      fs_give((void **)&var->main_user_val.p);
	    if(var->post_user_val.p)
	      fs_give((void **)&var->post_user_val.p);
	    if(var->global_val.p)
	      fs_give((void **)&var->global_val.p);
	    if(var->fixed_val.p)
	      fs_give((void **)&var->fixed_val.p);
	    if(var->cmdline_val.p)
	      fs_give((void **)&var->cmdline_val.p);
	}
    }
}


/*
 * Standard feature name sections
 */
char *
feature_list_section(feature)
    FEATURE_S *feature;
{
#define	PREF_NONE      -1
    static char *feat_sect[] = {
#define PREF_MISC	0
	"Advanced User Preferences",
#define PREF_FLDR	1
	"Folder Preferences",
#define PREF_ADDR	2
	"Address Book Preferences",
#define PREF_COMP	3
	"Composer Preferences",
#define PREF_NEWS	4
	"News Preferences",
#define PREF_VIEW	5
	"Viewer Preferences",
#define PREF_ACMD	6
	"Advanced Command Preferences",
#define PREF_PRNT	7
	"Printer Preferences",
#define	PREF_RPLY	8
	"Reply Preferences",
#define	PREF_SEND	9
	"Sending Preferences",
#define	PREF_INDX	10
	"Message Index Preferences",
#define	PREF_HIDDEN	11
	HIDDEN_PREF
};

    return((feature && feature->section > PREF_NONE
	    && feature->section < (sizeof(feat_sect)/sizeof(feat_sect[0])))
	   ? feat_sect[feature->section] : NULL);
}



/* any os-specific exclusions */
#if	defined(DOS) || defined(OS2)
#define	PREF_OS_LWSD	PREF_NONE
#define	PREF_OS_LCLK	PREF_NONE
#define	PREF_OS_STSP	PREF_NONE
#define	PREF_OS_SPWN	PREF_NONE
#define	PREF_OS_XNML	PREF_NONE
#define	PREF_OS_USFK	PREF_MISC
#define	PREF_OS_MOUSE	PREF_NONE
#else
#define	PREF_OS_LWSD	PREF_MISC
#define	PREF_OS_LCLK	PREF_COMP
#define	PREF_OS_STSP	PREF_MISC
#define	PREF_OS_SPWN	PREF_MISC
#define	PREF_OS_XNML	PREF_MISC
#define	PREF_OS_USFK	PREF_NONE
#define	PREF_OS_MOUSE	PREF_MISC
#endif




/*
 * Standard way to get at feature list members...
 */
FEATURE_S *
feature_list(index)
    int index;
{
    /*
     * This list is alphabatized by feature string, but the 
     * macro values need not be ordered.
     */
    static FEATURE_S feat_list[] = {
/* Composer prefs */
	{"alternate-compose-menu",
	 F_ALT_COMPOSE_MENU, h_config_alt_compose_menu, PREF_COMP},
	{"alternate-role-menu",
	 F_ALT_ROLE_MENU, h_config_alt_role_menu, PREF_COMP},
	{"compose-cancel-confirm-uses-yes",
	 F_CANCEL_CONFIRM, h_config_cancel_confirm, PREF_COMP},
	{"compose-cut-from-cursor",
	 F_DEL_FROM_DOT, h_config_del_from_dot, PREF_COMP},
	{"compose-maps-delete-key-to-ctrl-d",
	 F_COMPOSE_MAPS_DEL, h_config_compose_maps_del, PREF_COMP},
	{"compose-rejects-unqualified-addrs",
	 F_COMPOSE_REJECTS_UNQUAL, h_config_compose_rejects_unqual, PREF_COMP},
	{"compose-send-offers-first-filter",
	 F_FIRST_SEND_FILTER_DFLT, h_config_send_filter_dflt, PREF_COMP},
	{"downgrade-multipart-to-text",
	 F_COMPOSE_ALWAYS_DOWNGRADE, h_downgrade_multipart_to_text, PREF_COMP},
	{"enable-alternate-editor-cmd",
	 F_ENABLE_ALT_ED, h_config_enable_alt_ed, PREF_COMP},
	{"enable-alternate-editor-implicitly",
	 F_ALT_ED_NOW, h_config_alt_ed_now, PREF_COMP},
	{"enable-search-and-replace",
	 F_ENABLE_SEARCH_AND_REPL, h_config_enable_search_and_repl, PREF_COMP},
	{"enable-sigdashes",
	 F_ENABLE_SIGDASHES, h_config_sigdashes, PREF_COMP},
	{"quell-dead-letter-on-cancel",
	 F_QUELL_DEAD_LETTER, h_config_quell_dead_letter, PREF_COMP},
	{"quell-flowed-text",
	 F_QUELL_FLOWED_TEXT, h_config_quell_flowed_text, PREF_COMP},
	{"quell-mailchecks-composing-except-inbox",
	 F_QUELL_PINGS_COMPOSING, h_config_quell_checks_comp, PREF_COMP},
	{"quell-mailchecks-composing-inbox",
	 F_QUELL_PINGS_COMPOSING_INBOX, h_config_quell_checks_comp_inbox, PREF_COMP},
	{"quell-user-lookup-in-passwd-file",
	 F_QUELL_LOCAL_LOOKUP, h_config_quell_local_lookup, PREF_OS_LCLK},
	{"spell-check-before-sending",
	 F_ALWAYS_SPELL_CHECK, h_config_always_spell_check, PREF_COMP},
	{"strip-whitespace-before-send",
	 F_STRIP_WS_BEFORE_SEND, h_config_strip_ws_before_send, PREF_COMP},

/* Reply Prefs */
	{"enable-reply-indent-string-editing",
	 F_ENABLE_EDIT_REPLY_INDENT, h_config_prefix_editing, PREF_RPLY},
	{"include-attachments-in-reply",
	 F_ATTACHMENTS_IN_REPLY, h_config_attach_in_reply, PREF_RPLY},
	{"include-header-in-reply",
	 F_INCLUDE_HEADER, h_config_include_header, PREF_RPLY},
	{"include-text-in-reply",
	 F_AUTO_INCLUDE_IN_REPLY, h_config_auto_include_reply, PREF_RPLY},
	{"reply-always-uses-reply-to",
	 F_AUTO_REPLY_TO, h_config_auto_reply_to, PREF_RPLY},
	{"signature-at-bottom",
	 F_SIG_AT_BOTTOM, h_config_sig_at_bottom, PREF_RPLY},
	{"strip-from-sigdashes-on-reply",
	 F_ENABLE_STRIP_SIGDASHES, h_config_strip_sigdashes, PREF_RPLY},

/* Sending Prefs */
	{"disable-sender",
	 F_DISABLE_SENDER, h_config_disable_sender, PREF_SEND},
	{"enable-8bit-esmtp-negotiation",
	 F_ENABLE_8BIT, h_config_8bit_smtp, PREF_SEND},
#ifdef	BACKGROUND_POST
	{"enable-background-sending",
	 F_BACKGROUND_POST, h_config_compose_bg_post, PREF_SEND},
#endif
	{"enable-delivery-status-notification",
	 F_DSN, h_config_compose_dsn, PREF_SEND},
	{"enable-verbose-smtp-posting",
	 F_VERBOSE_POST, h_config_verbose_post, PREF_SEND},
	{"fcc-on-bounce",
	 F_FCC_ON_BOUNCE, h_config_fcc_on_bounce, PREF_SEND},
	{"fcc-only-without-confirm",
	 F_AUTO_FCC_ONLY, h_config_auto_fcc_only, PREF_SEND},
	{"fcc-without-attachments",
	 F_NO_FCC_ATTACH, h_config_no_fcc_attach, PREF_SEND},
	{"mark-fcc-seen",
	 F_MARK_FCC_SEEN, h_config_mark_fcc_seen, PREF_SEND},
	{"send-without-confirm",
	 F_SEND_WO_CONFIRM, h_config_send_wo_confirm, PREF_SEND},
	{"use-sender-not-x-sender",
	 F_USE_SENDER_NOT_X, h_config_use_sender_not_x, PREF_SEND},
	{"warn-if-blank-to-and-cc-and-newsgroups",
	 F_WARN_ABOUT_NO_TO_OR_CC, h_config_warn_if_no_to_or_cc, PREF_SEND},
	{"warn-if-blank-subject",
	 F_WARN_ABOUT_NO_SUBJECT, h_config_warn_if_subj_blank, PREF_SEND},

/* Folder */
	{"combined-subdirectory-display",
	 F_CMBND_SUBDIR_DISP, h_config_combined_subdir_display, PREF_FLDR},
	{"combined-folder-display",
	 F_CMBND_FOLDER_DISP, h_config_combined_folder_display, PREF_FLDR},
	{"enable-dot-folders",
	 F_ENABLE_DOT_FOLDERS, h_config_enable_dot_folders, PREF_FLDR},
	{"enable-incoming-folders",
	 F_ENABLE_INCOMING, h_config_enable_incoming, PREF_FLDR},
	{"enable-lame-list-mode",
	 F_FIX_BROKEN_LIST, h_config_lame_list_mode, PREF_FLDR},
	{"expanded-view-of-folders",
	 F_EXPANDED_FOLDERS, h_config_expanded_folders, PREF_FLDR},
	{"quell-empty-directories",
	 F_QUELL_EMPTY_DIRS, h_config_quell_empty_dirs, PREF_FLDR},
	{"separate-folder-and-directory-entries",
	 F_SEPARATE_FLDR_AS_DIR, h_config_separate_fold_dir_view, PREF_FLDR},
	{"single-column-folder-list",
	 F_SINGLE_FOLDER_LIST, h_config_single_list, PREF_FLDR},
	{"sort-default-fcc-alpha",
	 F_SORT_DEFAULT_FCC_ALPHA, h_config_sort_fcc_alpha, PREF_FLDR},
	{"sort-default-save-alpha",
	 F_SORT_DEFAULT_SAVE_ALPHA, h_config_sort_save_alpha, PREF_FLDR},
	{"vertical-folder-list",
	 F_VERTICAL_FOLDER_LIST, h_config_vertical_list, PREF_FLDR},

/* Addr book */
	{"combined-addrbook-display",
	 F_CMBND_ABOOK_DISP, h_config_combined_abook_display, PREF_ADDR},
	{"expanded-view-of-addressbooks",
	 F_EXPANDED_ADDRBOOKS, h_config_expanded_addrbooks, PREF_ADDR},
	{"expanded-view-of-distribution-lists",
	 F_EXPANDED_DISTLISTS, h_config_expanded_distlists, PREF_ADDR},
#ifdef	ENABLE_LDAP
	{"ldap-result-to-addrbook-add",
	 F_ADD_LDAP_TO_ABOOK, h_config_add_ldap, PREF_ADDR},
#endif

/* Index prefs */
	{"auto-open-next-unread",
	 F_AUTO_OPEN_NEXT_UNREAD, h_config_auto_open_unread, PREF_INDX},
	{"continue-tab-without-confirm",
	 F_TAB_NO_CONFIRM, h_config_tab_no_prompt, PREF_INDX},
	{"delete-skips-deleted",
	 F_DEL_SKIPS_DEL, h_config_del_skips_del, PREF_INDX},
	{"enable-cruise-mode",
	 F_ENABLE_SPACE_AS_TAB, h_config_cruise_mode, PREF_INDX},
	{"enable-cruise-mode-delete",
	 F_ENABLE_TAB_DELETES, h_config_cruise_mode_delete, PREF_INDX},
	{"mark-for-cc",
	 F_MARK_FOR_CC, h_config_mark_for_cc, PREF_INDX},
	{"next-thread-without-confirm",
	 F_NEXT_THRD_WO_CONFIRM, h_config_next_thrd_wo_confirm, PREF_INDX},
	{"return-to-inbox-without-confirm",
	 F_RET_INBOX_NO_CONFIRM, h_config_inbox_no_confirm, PREF_INDX},
	{"show-sort",
	 F_SHOW_SORT, h_config_show_sort, PREF_INDX},
	{"tab-uses-unseen-for-next-folder",
	 F_TAB_USES_UNSEEN, h_config_tab_uses_unseen, PREF_INDX},
	{"tab-visits-next-new-message-only",
	 F_TAB_TO_NEW, h_config_tab_new_only, PREF_INDX},
	{"thread-index-shows-important-color",
	 F_COLOR_LINE_IMPORTANT, h_config_color_thrd_import, PREF_INDX},

/* Viewer prefs */
	{"enable-msg-view-addresses",
	 F_SCAN_ADDR, h_config_enable_view_addresses, PREF_VIEW},
	{"enable-msg-view-attachments",
	 F_VIEW_SEL_ATTACH, h_config_enable_view_attach, PREF_VIEW},
	{"enable-msg-view-forced-arrows",
	 F_FORCE_ARROWS, h_config_enable_view_arrows, PREF_VIEW},
	{"enable-msg-view-urls",
	 F_VIEW_SEL_URL, h_config_enable_view_url, PREF_VIEW},
	{"enable-msg-view-web-hostnames",
	 F_VIEW_SEL_URL_HOST, h_config_enable_view_web_host, PREF_VIEW},
	{"prefer-plain-text",
	 F_PREFER_PLAIN_TEXT, h_config_prefer_plain_text, PREF_VIEW},
#ifndef	_WINDOWS
	/* set to TRUE for windows */
	{"pass-c1-control-characters-as-is",
	 F_PASS_C1_CONTROL_CHARS, h_config_pass_c1_control, PREF_VIEW},
	{"pass-control-characters-as-is",
	 F_PASS_CONTROL_CHARS, h_config_pass_control, PREF_VIEW},
#endif
	{"quell-charset-warning",
	 F_QUELL_CHARSET_WARNING, h_config_quell_charset_warning, PREF_VIEW},

/* News */
	{"compose-sets-newsgroup-without-confirm",
	 F_COMPOSE_TO_NEWSGRP, h_config_compose_news_wo_conf, PREF_NEWS},
	{"enable-8bit-nntp-posting",
	 F_ENABLE_8BIT_NNTP, h_config_8bit_nntp, PREF_NEWS},
	{"enable-multiple-newsrcs",
	 F_ENABLE_MULNEWSRCS, h_config_enable_mulnewsrcs, PREF_NEWS},
	{"hide-nntp-path",
	 F_HIDE_NNTP_PATH, h_config_hide_nntp_path, PREF_NEWS},
	{"mult-newsrc-hostnames-as-typed",
	 F_MULNEWSRC_HOSTNAMES_AS_TYPED, h_config_mulnews_as_typed, PREF_NEWS},
	{"news-approximates-new-status",
	 F_FAKE_NEW_IN_NEWS, h_config_news_uses_recent, PREF_NEWS},
	{"news-deletes-across-groups",
	 F_NEWS_CROSS_DELETE, h_config_news_cross_deletes, PREF_NEWS},
	{"news-offers-catchup-on-close",
	 F_NEWS_CATCHUP, h_config_news_catchup, PREF_NEWS},
	{"news-post-without-validation",
	 F_NO_NEWS_VALIDATION, h_config_post_wo_validation, PREF_NEWS},
	{"news-read-in-newsrc-order",
	 F_READ_IN_NEWSRC_ORDER, h_config_read_in_newsrc_order, PREF_NEWS},
	{"predict-nntp-server",
	 F_PREDICT_NNTP_SERVER, h_config_predict_nntp_server, PREF_NEWS},
	{"quell-extra-post-prompt",
	 F_QUELL_EXTRA_POST_PROMPT, h_config_quell_post_prompt, PREF_NEWS},

/* Print */
	{"enable-print-via-y-command",
	 F_ENABLE_PRYNT, h_config_enable_y_print, PREF_PRNT},
	{"print-formfeed-between-messages",
	 F_AGG_PRINT_FF, h_config_ff_between_msgs, PREF_PRNT},
	{"print-includes-from-line",
	 F_FROM_DELIM_IN_PRINT, h_config_print_from, PREF_PRNT},
	{"print-index-enabled",
	 F_PRINT_INDEX, h_config_print_index, PREF_PRNT},
	{"print-offers-custom-cmd-prompt",
	 F_CUSTOM_PRINT, h_config_custom_print, PREF_PRNT},

/* adv cmd prefs */
	{"enable-aggregate-command-set",
	 F_ENABLE_AGG_OPS, h_config_enable_agg_ops, PREF_ACMD},
	{"enable-arrow-navigation",
	 F_ARROW_NAV, h_config_arrow_nav, PREF_ACMD},
	{"enable-arrow-navigation-relaxed",
	 F_RELAXED_ARROW_NAV, h_config_relaxed_arrow_nav, PREF_ACMD},
	{"enable-bounce-cmd",
	 F_ENABLE_BOUNCE, h_config_enable_bounce, PREF_ACMD},
	{"enable-exit-via-lessthan-command",
	 F_ENABLE_LESSTHAN_EXIT, h_config_enable_lessthan_exit, PREF_ACMD},
	{"enable-flag-cmd",
	 F_ENABLE_FLAG, h_config_enable_flag, PREF_ACMD},
	{"enable-flag-screen-implicitly",
	 F_FLAG_SCREEN_DFLT, h_config_flag_screen_default, PREF_ACMD},
	{"enable-flag-screen-keyword-shortcut",
	 F_FLAG_SCREEN_KW_SHORTCUT, h_config_flag_screen_kw_shortcut,PREF_ACMD},
	{"enable-full-header-and-text",
	 F_ENABLE_FULL_HDR_AND_TEXT, h_config_enable_full_hdr_and_text, PREF_ACMD},
	{"enable-full-header-cmd",
	 F_ENABLE_FULL_HDR, h_config_enable_full_hdr, PREF_ACMD},
	{"enable-goto-in-file-browser",
	 F_ALLOW_GOTO, h_config_allow_goto, PREF_ACMD},
	{"enable-jump-shortcut",
	 F_ENABLE_JUMP, h_config_enable_jump, PREF_ACMD},
	{"enable-partial-match-lists",
	 F_ENABLE_SUB_LISTS, h_config_sub_lists, PREF_ACMD},
	{"enable-tab-completion",
	 F_ENABLE_TAB_COMPLETE, h_config_enable_tab_complete, PREF_ACMD},
	{"enable-unix-pipe-cmd",
	 F_ENABLE_PIPE, h_config_enable_pipe, PREF_ACMD},
	{"quell-full-header-auto-reset",
	 F_QUELL_FULL_HDR_RESET, h_config_quell_full_hdr_reset, PREF_ACMD},

/* Adv user prefs */
#if !defined(DOS) && !defined(OS2)
	{"allow-talk",
	 F_ALLOW_TALK, h_config_allow_talk, PREF_MISC},
#endif
	{"assume-slow-link",
	 F_FORCE_LOW_SPEED, h_config_force_low_speed, PREF_OS_LWSD},
	{"auto-move-read-msgs",
	 F_AUTO_READ_MSGS, h_config_auto_read_msgs, PREF_MISC},
	{"auto-unzoom-after-apply",
	 F_AUTO_UNZOOM, h_config_auto_unzoom, PREF_MISC},
	{"auto-zoom-after-select",
	 F_AUTO_ZOOM, h_config_auto_zoom, PREF_MISC},
	{"check-newmail-when-quitting",
	 F_CHECK_MAIL_ONQUIT, h_config_check_mail_onquit, PREF_MISC},
	{"confirm-role-even-for-default",
	 F_ROLE_CONFIRM_DEFAULT, h_config_confirm_role, PREF_MISC},
	{"disable-2022-jp-conversions",
	 F_DISABLE_2022_JP_CONVERSIONS, h_config_disable_2022_jp_conv,
								 PREF_MISC},
	{"disable-charset-conversions",
	 F_DISABLE_CHARSET_CONVERSIONS, h_config_disable_cset_conv, PREF_MISC},
	{"disable-keymenu",
	 F_BLANK_KEYMENU, h_config_blank_keymenu, PREF_MISC},
	{"disable-take-fullname-in-addresses",
	 F_DISABLE_TAKE_FULLNAMES, h_config_take_fullname, PREF_MISC},
	{"disable-take-last-comma-first",
	 F_DISABLE_TAKE_LASTFIRST, h_config_take_lastfirst, PREF_MISC},
	{"disable-terminal-reset-for-display-filters",
	 F_DISABLE_TERM_RESET_DISP, h_config_disable_reset_disp, PREF_MISC},
	{"enable-dot-files",
	 F_ENABLE_DOT_FILES, h_config_enable_dot_files, PREF_MISC},
	{"enable-fast-recent-test",
	 F_ENABLE_FAST_RECENT, h_config_fast_recent, PREF_MISC},
	{"enable-mail-check-cue",
	 F_SHOW_DELAY_CUE, h_config_show_delay_cue, PREF_MISC},
	{"enable-mouse-in-xterm",
	 F_ENABLE_MOUSE, h_config_enable_mouse, PREF_OS_MOUSE},
	{"enable-newmail-in-xterm-icon",
	 F_ENABLE_XTERM_NEWMAIL, h_config_enable_xterm_newmail, PREF_OS_XNML},
	{"enable-newmail-short-text-in-icon",
	 F_ENABLE_NEWMAIL_SHORT_TEXT, h_config_enable_newmail_short_text, PREF_OS_XNML},
	{"enable-rules-under-take",
	 F_ENABLE_ROLE_TAKE, h_config_enable_role_take, PREF_MISC},
	{"enable-suspend",
	 F_CAN_SUSPEND, h_config_can_suspend, PREF_MISC},
	{"enable-take-export",
	 F_ENABLE_TAKE_EXPORT, h_config_enable_take_export, PREF_MISC},
#ifdef	_WINDOWS
	{"enable-tray-icon",
	 F_ENABLE_TRAYICON, h_config_tray_icon, PREF_MISC},
#endif
	{"expose-hidden-config",
	 F_EXPOSE_HIDDEN_CONFIG, h_config_expose_hidden_config, PREF_MISC},
	{"expunge-only-manually",
	 F_EXPUNGE_MANUALLY, h_config_expunge_manually, PREF_MISC},
	{"expunge-without-confirm",
	 F_AUTO_EXPUNGE, h_config_auto_expunge, PREF_MISC},
	{"expunge-without-confirm-everywhere",
	 F_FULL_AUTO_EXPUNGE, h_config_full_auto_expunge, PREF_MISC},
	{"force-arrow-cursor",
	 F_FORCE_ARROW, h_config_force_arrow, PREF_MISC},
	{"maildrops-preserve-state",
	 F_MAILDROPS_PRESERVE_STATE, h_config_maildrops_preserve_state,
	 PREF_MISC},
	{"offer-expunge-of-inbox",
	 F_EXPUNGE_INBOX, h_config_expunge_inbox, PREF_MISC},
	{"offer-expunge-of-stayopen-folders",
	 F_EXPUNGE_STAYOPENS, h_config_expunge_stayopens, PREF_MISC},
	{"preopen-stayopen-folders",
	 F_PREOPEN_STAYOPENS, h_config_preopen_stayopens, PREF_MISC},
	{"preserve-start-stop-characters",
	 F_PRESERVE_START_STOP, h_config_preserve_start_stop, PREF_OS_STSP},
	{"prune-uses-yyyy-mm",
	 F_PRUNE_USES_ISO, h_config_prune_uses_iso, PREF_MISC},
	{"quell-attachment-extension-warn",
	 F_QUELL_ATTACH_EXT_WARN, h_config_quell_attach_ext_warn,
	 PREF_MISC},
	{"quell-attachment-extra-prompt",
	 F_QUELL_ATTACH_EXTRA_PROMPT, h_config_quell_attach_extra_prompt,
	 PREF_MISC},
	{"quell-content-id",
	 F_QUELL_CONTENT_ID, h_config_quell_content_id, PREF_MISC},
	{"quell-filtering-done-message",
	 F_QUELL_FILTER_DONE_MSG, h_config_quell_filtering_done_message,
	 PREF_MISC},
	{"quell-filtering-messages",
	 F_QUELL_FILTER_MSGS, h_config_quell_filtering_messages,
	 PREF_MISC},
	{"quell-timezone-comment-when-sending",
	 F_QUELL_TIMEZONE, h_config_quell_tz_comment, PREF_MISC},
	{"quell-folder-internal-msg",
	 F_QUELL_INTERNAL_MSG, h_config_quell_folder_internal_msg, PREF_MISC},
	{"quell-lock-failure-warnings",
	 F_QUELL_LOCK_FAILURE_MSGS, h_config_quell_lock_failure_warnings,
	 PREF_MISC},
#ifdef	_WINDOWS
	{"quell-ssl-largeblocks",
	 F_QUELL_SSL_LARGEBLOCKS, h_config_quell_ssl_largeblocks, PREF_MISC},
#endif
	{"quell-status-message-beeping",
	 F_QUELL_BEEPS, h_config_quell_beeps, PREF_MISC},
	{"quit-without-confirm",
	 F_QUIT_WO_CONFIRM, h_config_quit_wo_confirm, PREF_MISC},
	{"quote-replace-nonflowed",
	 F_QUOTE_REPLACE_NOFLOW, h_config_quote_replace_noflow, PREF_MISC},
	{"save-partial-msg-without-confirm",
	 F_SAVE_PARTIAL_WO_CONFIRM, h_config_save_part_wo_confirm, PREF_MISC},
	{"save-will-advance",
	 F_SAVE_ADVANCES, h_config_save_advances, PREF_MISC},
	{"save-will-not-delete",
	 F_SAVE_WONT_DELETE, h_config_save_wont_delete, PREF_MISC},
	{"save-will-quote-leading-froms",
	 F_QUOTE_ALL_FROMS, h_config_quote_all_froms, PREF_MISC},
	{"scramble-message-id",
	 F_ROT13_MESSAGE_ID, h_config_scramble_message_id, PREF_MISC},
	{"select-without-confirm",
	 F_SELECT_WO_CONFIRM, h_config_select_wo_confirm, PREF_MISC},
	{"show-cursor",
	 F_SHOW_CURSOR, h_config_show_cursor, PREF_MISC},
	{"show-plain-text-internally",
	 F_SHOW_TEXTPLAIN_INT, h_config_textplain_int, PREF_MISC},
	{"show-selected-in-boldface",
	 F_SELECTED_SHOWN_BOLD, h_config_select_in_bold, PREF_MISC},
	{"slash-collapses-entire-thread",
	 F_SLASH_COLL_ENTIRE, h_config_slash_coll_entire, PREF_MISC},
#ifdef	_WINDOWS
	{"store-window-position-in-config",
	 F_STORE_WINPOS_IN_CONFIG, h_config_winpos_in_config, PREF_MISC},
#endif
	{"tab-checks-recent",
	 F_TAB_CHK_RECENT, h_config_tab_checks_recent, PREF_MISC},
	{"try-alternative-authentication-driver-first",
	 F_PREFER_ALT_AUTH, h_config_alt_auth, PREF_MISC},
	{"unselect-will-not-advance",
	 F_UNSELECT_WONT_ADVANCE, h_config_unsel_wont_advance, PREF_MISC},
	{"use-current-dir",
	 F_USE_CURRENT_DIR, h_config_use_current_dir, PREF_MISC},
	{"use-function-keys",
	 F_USE_FK, h_config_use_fk, PREF_OS_USFK},
	{"use-regular-startup-rule-for-stayopen-folders",
	 F_STARTUP_STAYOPEN, h_config_use_reg_start_for_stayopen, PREF_MISC},
	{"use-subshell-for-suspend",
	 F_SUSPEND_SPAWNS, h_config_suspend_spawns, PREF_OS_SPWN},

/* Hidden Features */
	{"old-growth",
	 F_OLD_GROWTH, NO_HELP, PREF_NONE},
	{"allow-changing-from",
	 F_ALLOW_CHANGING_FROM, h_config_allow_chg_from, PREF_HIDDEN},
	{"cache-remote-pinerc",
	 F_CACHE_REMOTE_PINERC, NO_HELP, PREF_NONE},
	{"disable-busy-alarm",
	 F_DISABLE_ALARM, h_config_disable_busy, PREF_HIDDEN},
	{"disable-config-cmd",
	 F_DISABLE_CONFIG_SCREEN, h_config_disable_config_cmd, PREF_HIDDEN},
	{"disable-keyboard-lock-cmd",
	 F_DISABLE_KBLOCK_CMD, h_config_disable_kb_lock, PREF_HIDDEN},
	{"disable-password-caching",
	 F_DISABLE_PASSWORD_CACHING, h_config_disable_password_caching,
	 PREF_HIDDEN},
	{"disable-password-cmd",
	 F_DISABLE_PASSWORD_CMD, h_config_disable_password_cmd, PREF_HIDDEN},
	{"disable-pipes-in-sigs",
	 F_DISABLE_PIPES_IN_SIGS, h_config_disable_pipes_in_sigs, PREF_HIDDEN},
	{"disable-pipes-in-templates",
	 F_DISABLE_PIPES_IN_TEMPLATES, h_config_disable_pipes_in_templates,
	 PREF_HIDDEN},
	{"disable-roles-setup-cmd",
	 F_DISABLE_ROLES_SETUP, h_config_disable_roles_setup, PREF_HIDDEN},
	{"disable-roles-sig-edit",
	 F_DISABLE_ROLES_SIGEDIT, h_config_disable_roles_sigedit, PREF_HIDDEN},
	{"disable-roles-template-edit",
	 F_DISABLE_ROLES_TEMPLEDIT, h_config_disable_roles_templateedit,
	 PREF_HIDDEN},
	{"disable-setlocale-collate",
	 F_DISABLE_SETLOCALE_COLLATE, h_config_disable_collate, PREF_HIDDEN},
	{"disable-shared-namespaces",
	 F_DISABLE_SHARED_NAMESPACES, h_config_disable_shared, PREF_HIDDEN},
	{"disable-signature-edit-cmd",
	 F_DISABLE_SIGEDIT_CMD, h_config_disable_signature_edit, PREF_HIDDEN},
	{"enable-mailcap-param-substitution",
	 F_DO_MAILCAP_PARAM_SUBST, h_config_mailcap_params, PREF_HIDDEN},
	{"enable-setlocale-ctype",
	 F_ENABLE_SETLOCALE_CTYPE, h_config_enable_ctype, PREF_HIDDEN},
	{"quell-berkeley-format-timezone",
	 F_QUELL_BEZERK_TIMEZONE, h_config_no_bezerk_zone, PREF_HIDDEN},
	{"quell-imap-envelope-update",
	 F_QUELL_IMAP_ENV_CB, h_config_quell_imap_env, PREF_HIDDEN},
	{"quell-maildomain-warning",
	 F_QUELL_MAILDOMAIN_WARNING, h_config_quell_domain_warn, PREF_HIDDEN},
	{"quell-news-envelope-update",
	 F_QUELL_NEWS_ENV_CB, h_config_quell_news_env, PREF_HIDDEN},
	{"quell-partial-fetching",
	 F_QUELL_PARTIAL_FETCH, h_config_quell_partial, PREF_HIDDEN},
	{"quell-personal-name-prompt",
	 F_QUELL_PERSONAL_NAME_PROMPT, h_config_quell_personal_name_prompt, PREF_HIDDEN},
	{"quell-user-id-prompt",
	 F_QUELL_USER_ID_PROMPT, h_config_quell_user_id_prompt, PREF_HIDDEN},
	{"save-aggregates-copy-sequence",
	 F_AGG_SEQ_COPY, h_config_save_aggregates, PREF_HIDDEN},
	{"selectable-item-nobold",
	 F_SLCTBL_ITEM_NOBOLD, NO_HELP, PREF_NONE},
	{"termdef-takes-precedence",
	 F_TCAP_WINS, h_config_termcap_wins, PREF_HIDDEN}
    };

    return((index >= 0 && index < (sizeof(feat_list)/sizeof(feat_list[0])))
	   ? &feat_list[index] : NULL);
}


/*
 * feature_list_index -- return index of given feature id in
 *			 feature list
 */
int
feature_list_index(id)
    int id;
{
    FEATURE_S *feature;
    int	       i;

    for(i = 0; feature = feature_list(i); i++)
      if(id == feature->id)
	return(i);

    return(-1);
}


/*
 * feature_list_name -- return the given feature id's corresponding name
 */
char *
feature_list_name(id)
    int id;
{
    FEATURE_S *f;

    return((f = feature_list(feature_list_index(id))) ? f->name : "");
}


/*
 * feature_list_help -- return the given feature id's corresponding help
 */
HelpType
feature_list_help(id)
    int id;
{
    FEATURE_S *f;

    return((f = feature_list(feature_list_index(id))) ? f->help : NO_HELP);
}


/*
 * All the arguments past "list" are the backwards compatibility hacks.
 */
void
process_feature_list(ps, list, old_growth, hir, osr)
    struct pine *ps;
    char **list;
    int old_growth, hir, osr;
{
    register char            *q;
    char                    **p,
                             *lvalue[BM_SIZE * 8];
    int                       i,
                              yorn;
    long                      l;
    FEATURE_S		     *feat;


    /* clear all previous settings and then reset them */
    for(i = 0; (feat = feature_list(i)) != NULL; i++)
      F_SET(feat->id, ps, 0);


    /* backwards compatibility */
    if(hir)
	F_TURN_ON(F_INCLUDE_HEADER, ps);

    /* ditto */
    if(osr)
	F_TURN_ON(F_SIG_AT_BOTTOM, ps);

    /* ditto */
    if(old_growth)
        set_old_growth_bits(ps, 0);

    /* now run through the list (global, user, and cmd_line lists are here) */
    if(list){
      for(p = list; (q = *p) != NULL; p++){
	if(struncmp(q, "no-", 3) == 0){
	  yorn = 0;
	  q += 3;
	}else{
	  yorn = 1;
	}

	for(i = 0; (feat = feature_list(i)) != NULL; i++){
	  if(strucmp(q, feat->name) == 0){
	    if(feat->id == F_OLD_GROWTH){
	      set_old_growth_bits(ps, yorn);
	    }else{
	      F_SET(feat->id, ps, yorn);
	    }
	    break;
	  }
	}
	/* if it wasn't in that list */
	if(feat == NULL)
          dprint(1, (debugfile,"Unrecognized feature in feature-list (%s%s)\n",
		     (yorn ? "" : "no-"), q ? q : "?"));
      }
    }

    /*
     * Turn on gratuitous '>From ' quoting, if requested...
     */
    mail_parameters(NULL, SET_FROMWIDGET,
		    (void  *)(F_ON(F_QUOTE_ALL_FROMS, ps) ? 1 : 0));

    /*
     * Turn off .lock creation complaints...
     */
    if(F_ON(F_QUELL_LOCK_FAILURE_MSGS, ps))
      mail_parameters(NULL, SET_LOCKEACCESERROR, (void *) 0);

    /*
     * Turn on quelling of pseudo message.
     */
    if(F_ON(F_QUELL_INTERNAL_MSG,ps_global))
      mail_parameters(NULL, SET_USERHASNOLIFE, (void *) 1);

    l = F_ON(F_MULNEWSRC_HOSTNAMES_AS_TYPED,ps_global) ? 0L : 1L;
    mail_parameters(NULL, SET_NEWSRCCANONHOST, (void *) l);

#ifdef	_WINDOWS
    ps->pass_ctrl_chars = 1;
#else
    ps->pass_ctrl_chars = F_ON(F_PASS_CONTROL_CHARS,ps_global) ? 1 : 0;
    ps->pass_c1_ctrl_chars = F_ON(F_PASS_C1_CONTROL_CHARS,ps_global) ? 1 : 0;

    if(F_ON(F_QUELL_BEZERK_TIMEZONE,ps_global))
      mail_parameters(NULL, SET_NOTIMEZONES, (void *) 1);
#endif

    if(F_ON(F_USE_FK, ps))
      ps->orig_use_fkeys = 1;

    /* Will we have to build a new list? */
    if(!(old_growth || hir || osr))
	return;

    /*
     * Build a new list for feature-list.  The only reason we ever need to
     * do this is if one of the obsolete options is being converted
     * into a feature-list item, and it isn't already included in the user's
     * feature-list.
     */
    i = 0;
    for(p = LVAL(&ps->vars[V_FEATURE_LIST], Main);
	p && (q = *p); p++){
      /* already have it or cancelled it, don't need to add later */
      if(hir && (strucmp(q, "include-header-in-reply") == 0 ||
                             strucmp(q, "no-include-header-in-reply") == 0)){
	hir = 0;
      }else if(osr && (strucmp(q, "signature-at-bottom") == 0 ||
                             strucmp(q, "no-signature-at-bottom") == 0)){
	osr = 0;
      }else if(old_growth && (strucmp(q, "old-growth") == 0 ||
                             strucmp(q, "no-old-growth") == 0)){
	old_growth = 0;
      }
      lvalue[i++] = cpystr(q);
    }

    /* check to see if we still need to build a new list */
    if(!(old_growth || hir || osr))
	return;

    if(hir)
      lvalue[i++] = "include-header-in-reply";
    if(osr)
      lvalue[i++] = "signature-at-bottom";
    if(old_growth)
      lvalue[i++] = "old-growth";
    lvalue[i] = NULL;
    set_variable_list(V_FEATURE_LIST, lvalue, TRUE, Main);
}


void
set_current_pattern_vals(ps)
    struct pine *ps;
{
    struct variable *vars = ps->vars;

    set_current_val(&vars[V_PATTERNS], TRUE, TRUE);
    set_current_val(&vars[V_PAT_ROLES], TRUE, TRUE);
    set_current_val(&vars[V_PAT_FILTS], TRUE, TRUE);
    set_current_val(&vars[V_PAT_FILTS_OLD], TRUE, TRUE);
    set_current_val(&vars[V_PAT_SCORES], TRUE, TRUE);
    set_current_val(&vars[V_PAT_SCORES_OLD], TRUE, TRUE);
    set_current_val(&vars[V_PAT_INCOLS], TRUE, TRUE);
    set_current_val(&vars[V_PAT_OTHER], TRUE, TRUE);

    /*
     * If old pattern variable (V_PATTERNS) is set and the new ones aren't
     * in the config file, then convert the old data into the new variables.
     * It isn't quite that simple, though, because we don't store unset
     * variables in remote pinercs. Check for the variables but if we
     * don't find any of them, also check the version number. This change was
     * made in version 4.30. We could just check that except that we're
     * worried somebody will make an incompatible version number change in
     * their local version, and will break this. So we check both the
     * version # and the var_in_pinerc things to be safer.
     */
    if(vars[V_PATTERNS].current_val.l
       && vars[V_PATTERNS].current_val.l[0]
       && !var_in_pinerc(vars[V_PAT_ROLES].name)
       && !var_in_pinerc(vars[V_PAT_FILTS].name)
       && !var_in_pinerc(vars[V_PAT_FILTS_OLD].name)
       && !var_in_pinerc(vars[V_PAT_SCORES].name)
       && !var_in_pinerc(vars[V_PAT_SCORES_OLD].name)
       && !var_in_pinerc(vars[V_PAT_INCOLS].name)
       && isdigit((unsigned char) ps->pine_pre_vers[0])
       && ps->pine_pre_vers[1] == '.'
       && isdigit((unsigned char) ps->pine_pre_vers[2])
       && isdigit((unsigned char) ps->pine_pre_vers[3])
       && strncmp(ps->pine_pre_vers, "4.30", 4) < 0){
	convert_pattern_data();
    }

    /*
     * Otherwise, if FILTS_OLD is set and FILTS isn't in the config file,
     * convert FILTS_OLD to FILTS. Same for SCORES.
     * The reason FILTS was changed was so we could change the
     * semantics of how rules work when there are pieces in the rule that
     * we don't understand. At the same time as the FILTS change we added
     * a rule to detect 8bitSubjects. So a user might have a filter that
     * deletes messages with 8bitSubjects. The problem is that that same
     * filter in a FILTS_OLD pine would match because it would ignore the
     * 8bitSubject part of the pattern and match on the rest. So we changed
     * the semantics so that rules with unknown bits would be ignored
     * instead of used. We had to change variable names at the same time
     * because we were adding the 8bit thing and the old pines are still
     * out there. Filters and Scores can both be dangerous. Roles, Colors,
     * and Other seem less dangerous so not worth adding a new variable.
     * This was changed in 4.50.
     */
    else{
	if(vars[V_PAT_FILTS_OLD].current_val.l
	   && vars[V_PAT_FILTS_OLD].current_val.l[0]
	   && !var_in_pinerc(vars[V_PAT_FILTS].name)
	   && !var_in_pinerc(vars[V_PAT_SCORES].name)
	   && isdigit((unsigned char) ps->pine_pre_vers[0])
	   && ps->pine_pre_vers[1] == '.'
	   && isdigit((unsigned char) ps->pine_pre_vers[2])
	   && isdigit((unsigned char) ps->pine_pre_vers[3])
	   && strncmp(ps->pine_pre_vers, "4.50", 4) < 0){
	    convert_filts_pattern_data();
	}

	if(vars[V_PAT_SCORES_OLD].current_val.l
	   && vars[V_PAT_SCORES_OLD].current_val.l[0]
	   && !var_in_pinerc(vars[V_PAT_FILTS].name)
	   && !var_in_pinerc(vars[V_PAT_SCORES].name)
	   && isdigit((unsigned char) ps->pine_pre_vers[0])
	   && ps->pine_pre_vers[1] == '.'
	   && isdigit((unsigned char) ps->pine_pre_vers[2])
	   && isdigit((unsigned char) ps->pine_pre_vers[3])
	   && strncmp(ps->pine_pre_vers, "4.50", 4) < 0){
	    convert_scores_pattern_data();
	}
    }

    if(vars[V_PAT_ROLES].post_user_val.l)
      ps_global->ew_for_role_take = Post;
    else
      ps_global->ew_for_role_take = Main;

    if(vars[V_PAT_FILTS].post_user_val.l)
      ps_global->ew_for_filter_take = Post;
    else
      ps_global->ew_for_filter_take = Main;

    if(vars[V_PAT_SCORES].post_user_val.l)
      ps_global->ew_for_score_take = Post;
    else
      ps_global->ew_for_score_take = Main;

    if(vars[V_PAT_INCOLS].post_user_val.l)
      ps_global->ew_for_incol_take = Post;
    else
      ps_global->ew_for_incol_take = Main;

    if(vars[V_PAT_OTHER].post_user_val.l)
      ps_global->ew_for_other_take = Post;
    else
      ps_global->ew_for_other_take = Main;
}


/*
 * Foreach of the config files;
 * transfer the data to the new variables.
 */
void
convert_pattern_data()
{
    convert_pinerc_patterns(PAT_USE_MAIN);
    convert_pinerc_patterns(PAT_USE_POST);
}


void
convert_filts_pattern_data()
{
    convert_pinerc_filts_patterns(PAT_USE_MAIN);
    convert_pinerc_filts_patterns(PAT_USE_POST);
}


void
convert_scores_pattern_data()
{
    convert_pinerc_scores_patterns(PAT_USE_MAIN);
    convert_pinerc_scores_patterns(PAT_USE_POST);
}


/*
 * Foreach of the four variables, transfer the data for this config file
 * from the old patterns variable. We don't have to convert OTHER patterns
 * because they didn't exist in pines without patterns-other.
 *
 * If the original variable had patlines with type File then we convert
 * all of the individual patterns to type Lit, because each pattern can
 * be of any category. Lit patterns are better tested, anyway.
 */
void
convert_pinerc_patterns(use_flags)
    long use_flags;
{
    long      old_rflags;
    long      rflags;
    PAT_S    *pat;
    PAT_STATE pstate;
    ACTION_S *act;

    old_rflags = (ROLE_OLD_PAT | use_flags);

    rflags = 0L;
    if(any_patterns(old_rflags, &pstate)){
	dprint(2, (debugfile, "converting old patterns to new (%s)\n", (use_flags == PAT_USE_MAIN) ? "Main" : "Post"));
	for(pat = first_pattern(&pstate); 
	    pat;
	    pat = next_pattern(&pstate)){
	    if((act = pat->action) != NULL){
		if(act->is_a_role &&
		   add_to_pattern(pat, ROLE_DO_ROLES | use_flags))
		  rflags |= ROLE_DO_ROLES;
		if(act->is_a_incol &&
		   add_to_pattern(pat, ROLE_DO_INCOLS | use_flags))
		  rflags |= ROLE_DO_INCOLS;
		if(act->is_a_score &&
		   add_to_pattern(pat, ROLE_DO_SCORES | use_flags))
		  rflags |= ROLE_DO_SCORES;
		if(act->is_a_filter &&
		   add_to_pattern(pat, ROLE_DO_FILTER | use_flags))
		  rflags |= ROLE_DO_FILTER;
	    }
	}
	
	if(rflags)
	  if(write_patterns(rflags | use_flags))
	    dprint(1, (debugfile,
		   "Trouble converting patterns to new variable\n"));
    }
}


/*
 * If the original variable had patlines with type File then we convert
 * all of the individual patterns to type Lit, because each pattern can
 * be of any category. Lit patterns are better tested, anyway.
 */
void
convert_pinerc_filts_patterns(use_flags)
    long use_flags;
{
    long      old_rflags;
    long      rflags;
    PAT_S    *pat;
    PAT_STATE pstate;
    ACTION_S *act;

    old_rflags = (ROLE_OLD_FILT | use_flags);

    rflags = 0L;
    if(any_patterns(old_rflags, &pstate)){
	dprint(2, (debugfile, "converting old filter patterns to new (%s)\n", (use_flags == PAT_USE_MAIN) ? "Main" : "Post"));
	for(pat = first_pattern(&pstate); 
	    pat;
	    pat = next_pattern(&pstate)){
	    if((act = pat->action) != NULL){
		if(act->is_a_filter &&
		   add_to_pattern(pat, ROLE_DO_FILTER | use_flags))
		  rflags |= ROLE_DO_FILTER;
	    }
	}
	
	if(rflags)
	  if(write_patterns(rflags | use_flags))
	    dprint(1, (debugfile,
		   "Trouble converting filter patterns to new variable\n"));
    }
}


/*
 * If the original variable had patlines with type File then we convert
 * all of the individual patterns to type Lit, because each pattern can
 * be of any category. Lit patterns are better tested, anyway.
 */
void
convert_pinerc_scores_patterns(use_flags)
    long use_flags;
{
    long      old_rflags;
    long      rflags;
    PAT_S    *pat;
    PAT_STATE pstate;
    ACTION_S *act;

    old_rflags = (ROLE_OLD_SCORE | use_flags);

    rflags = 0L;
    if(any_patterns(old_rflags, &pstate)){
	dprint(2, (debugfile, "converting old scores patterns to new (%s)\n", (use_flags == PAT_USE_MAIN) ? "Main" : "Post"));
	for(pat = first_pattern(&pstate); 
	    pat;
	    pat = next_pattern(&pstate)){
	    if((act = pat->action) != NULL){
		if(act->is_a_score &&
		   add_to_pattern(pat, ROLE_DO_SCORES | use_flags))
		  rflags |= ROLE_DO_SCORES;
	    }
	}
	
	if(rflags)
	  if(write_patterns(rflags | use_flags))
	    dprint(1, (debugfile,
		   "Trouble converting scores patterns to new variable\n"));
    }
}


int
add_to_pattern(pat, rflags)
    PAT_S *pat;
    long   rflags;
{
    PAT_LINE_S *new_patline, *patline;
    PAT_S      *new_pat;
    PAT_STATE   dummy;
    extern PAT_HANDLE **cur_pat_h;

    if(!any_patterns(rflags, &dummy))
      return(0);

    /* need a new patline */
    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
    memset((void *)new_patline, 0, sizeof(*new_patline));
    new_patline->type = Literal;
    (*cur_pat_h)->dirtypinerc = 1;

    /* and a copy of pat */
    new_pat = copy_pat(pat);

    /* tie together */
    new_patline->first = new_patline->last = new_pat;
    new_pat->patline = new_patline;


    /*
     * Manipulate bits directly in pattern list.
     * Cur_pat_h is set by any_patterns.
     */


    /* find last patline */
    for(patline = (*cur_pat_h)->patlinehead;
	patline && patline->next;
	patline = patline->next)
      ;
    
    /* add new patline to end of list */
    if(patline){
	patline->next = new_patline;
	new_patline->prev = patline;
    }
    else
      (*cur_pat_h)->patlinehead = new_patline;
    
    return(1);
}


/*
 * set_old_growth_bits - Command used to set or unset old growth set
 *			 of features
 */
void
set_old_growth_bits(ps, val)
    struct pine *ps;
    int          val;
{
    int i;

    for(i = 1; i <= F_FEATURE_LIST_COUNT; i++)
      if(test_old_growth_bits(ps, i))
	F_SET(i, ps, val);
}



/*
 * test_old_growth_bits - Test to see if all the old growth bits are on,
 *			  *or* if a particular feature index is in the old
 *			  growth set.
 *
 * WEIRD ALERT: if index == F_OLD_GROWTH bit values are tested
 *              otherwise a bits existence in the set is tested!!!
 *
 * BUG: this will break if an old growth feature number is ever >= 32.
 */
int
test_old_growth_bits(ps, index)
    struct pine *ps;
    int          index;
{
    /*
     * this list defines F_OLD_GROWTH set
     */
    static unsigned long old_growth_bits = ((1 << F_ENABLE_FULL_HDR)     |
					    (1 << F_ENABLE_PIPE)         |
					    (1 << F_ENABLE_TAB_COMPLETE) |
					    (1 << F_QUIT_WO_CONFIRM)     |
					    (1 << F_ENABLE_JUMP)         |
					    (1 << F_ENABLE_ALT_ED)       |
					    (1 << F_ENABLE_BOUNCE)       |
					    (1 << F_ENABLE_AGG_OPS)	 |
					    (1 << F_ENABLE_FLAG)         |
					    (1 << F_CAN_SUSPEND));
    if(index >= 32)
	return(0);

    if(index == F_OLD_GROWTH){
	for(index = 1; index <= F_FEATURE_LIST_COUNT; index++)
	  if(((1 << index) & old_growth_bits) && F_OFF(index, ps))
	    return(0);

	return(1);
    }
    else
      return((1 << index) & old_growth_bits);
}



/*
 * Standard way to get at save message rules...
 */
NAMEVAL_S *
save_msg_rules(index)
    int index;
{
    static NAMEVAL_S save_rules[] = {
      {"by-from",			      NULL, MSG_RULE_FROM},
      {"by-nick-of-from",		      NULL, MSG_RULE_NICK_FROM_DEF},
      {"by-nick-of-from-then-from",	      NULL, MSG_RULE_NICK_FROM},
      {"by-fcc-of-from",		      NULL, MSG_RULE_FCC_FROM_DEF},
      {"by-fcc-of-from-then-from",	      NULL, MSG_RULE_FCC_FROM},
      {"by-realname-of-from",	 	      NULL,MSG_RULE_RN_FROM_DEF},
      {"by-realname-of-from-then-from",	      NULL, MSG_RULE_RN_FROM},
      {"by-sender",			      NULL, MSG_RULE_SENDER},
      {"by-nick-of-sender",		      NULL, MSG_RULE_NICK_SENDER_DEF},
      {"by-nick-of-sender-then-sender",	      NULL, MSG_RULE_NICK_SENDER},
      {"by-fcc-of-sender",		      NULL, MSG_RULE_FCC_SENDER_DEF},
      {"by-fcc-of-sender-then-sender",	      NULL, MSG_RULE_FCC_SENDER},
      {"by-realname-of-sender",		      NULL, MSG_RULE_RN_SENDER_DEF},
      {"by-realname-of-sender-then-sender",   NULL, MSG_RULE_RN_SENDER},
      {"by-recipient",			      NULL, MSG_RULE_RECIP},
      {"by-nick-of-recip",		      NULL, MSG_RULE_NICK_RECIP_DEF},
      {"by-nick-of-recip-then-recip",	      NULL, MSG_RULE_NICK_RECIP},
      {"by-fcc-of-recip",		      NULL, MSG_RULE_FCC_RECIP_DEF},
      {"by-fcc-of-recip-then-recip",	      NULL, MSG_RULE_FCC_RECIP},
      {"by-realname-of-recip",		      NULL, MSG_RULE_RN_RECIP_DEF},
      {"by-realname-of-recip-then-recip",     NULL, MSG_RULE_RN_RECIP},
      {"by-replyto",			      NULL, MSG_RULE_REPLYTO},
      {"by-nick-of-replyto",		      NULL, MSG_RULE_NICK_REPLYTO_DEF},
      {"by-nick-of-replyto-then-replyto",     NULL, MSG_RULE_NICK_REPLYTO},
      {"by-fcc-of-replyto",		      NULL, MSG_RULE_FCC_REPLYTO_DEF},
      {"by-fcc-of-replyto-then-replyto",      NULL, MSG_RULE_FCC_REPLYTO},
      {"by-realname-of-replyto",	      NULL, MSG_RULE_RN_REPLYTO_DEF},
      {"by-realname-of-replyto-then-replyto", NULL, MSG_RULE_RN_REPLYTO},
      {"last-folder-used",		      NULL, MSG_RULE_LAST}, 
      {"default-folder",		      NULL, MSG_RULE_DEFLT}
    };

    return((index >= 0 && index < (sizeof(save_rules)/sizeof(save_rules[0])))
	   ? &save_rules[index] : NULL);
}


/*
 * Standard way to get at fcc rules...
 */
NAMEVAL_S *
fcc_rules(index)
    int index;
{
    static NAMEVAL_S f_rules[] = {
	{"default-fcc",        NULL, FCC_RULE_DEFLT}, 
	{"last-fcc-used",      NULL, FCC_RULE_LAST}, 
	{"by-recipient",       NULL, FCC_RULE_RECIP},
	{"by-nickname",        NULL, FCC_RULE_NICK},
	{"by-nick-then-recip", NULL, FCC_RULE_NICK_RECIP},
	{"current-folder",     NULL, FCC_RULE_CURRENT}
    };

    return((index >= 0 && index < (sizeof(f_rules)/sizeof(f_rules[0])))
	   ? &f_rules[index] : NULL);
}


/*
 * Standard way to get at addrbook sort rules...
 */
NAMEVAL_S *
ab_sort_rules(index)
    int index;
{
    static NAMEVAL_S ab_rules[] = {
	{"fullname-with-lists-last",  NULL, AB_SORT_RULE_FULL_LISTS},
	{"fullname",                  NULL, AB_SORT_RULE_FULL}, 
	{"nickname-with-lists-last",  NULL, AB_SORT_RULE_NICK_LISTS},
	{"nickname",                  NULL, AB_SORT_RULE_NICK},
	{"dont-sort",                 NULL, AB_SORT_RULE_NONE}
    };

    return((index >= 0 && index < (sizeof(ab_rules)/sizeof(ab_rules[0])))
	   ? &ab_rules[index] : NULL);
}


/*
 * Standard way to get at color styles.
 */
NAMEVAL_S *
col_style(index)
    int index;
{
    static NAMEVAL_S col_styles[] = {
	{"no-color",		NULL, COL_NONE}, 
	{"use-termdef",		NULL, COL_TERMDEF},
	{"force-ansi-8color",	NULL, COL_ANSI8},
	{"force-ansi-16color",	NULL, COL_ANSI16}
    };

    return((index >= 0 && index < (sizeof(col_styles)/sizeof(col_styles[0])))
	   ? &col_styles[index] : NULL);
}


/*
 * Standard way to get at index color styles.
 */
NAMEVAL_S *
index_col_style(index)
    int index;
{
    static NAMEVAL_S ind_col_styles[] = {
	{"flip-colors",			NULL, IND_COL_FLIP}, 
	{"reverse",			NULL, IND_COL_REV}, 
	{"reverse-fg",			NULL, IND_COL_FG},
	{"reverse-fg-no-ambiguity",	NULL, IND_COL_FG_NOAMBIG},
	{"reverse-bg",			NULL, IND_COL_BG},
	{"reverse-bg-no-ambiguity",	NULL, IND_COL_BG_NOAMBIG}
    };

    return((index >= 0 && index < (sizeof(ind_col_styles)/sizeof(ind_col_styles[0]))) ? &ind_col_styles[index] : NULL);
}


/*
 * Standard way to get at titlebar color styles.
 */
NAMEVAL_S *
titlebar_col_style(index)
    int index;
{
    static NAMEVAL_S tbar_col_styles[] = {
	{"default",			NULL, TBAR_COLOR_DEFAULT}, 
	{"indexline",			NULL, TBAR_COLOR_INDEXLINE}, 
	{"reverse-indexline",		NULL, TBAR_COLOR_REV_INDEXLINE}
    };

    return((index >= 0 && index < (sizeof(tbar_col_styles)/sizeof(tbar_col_styles[0]))) ? &tbar_col_styles[index] : NULL);
}


/*
 * Standard way to get at folder sort rules...
 */
NAMEVAL_S *
fld_sort_rules(index)
    int index;
{
    static NAMEVAL_S fdl_rules[] = {
	{"alphabetical",          NULL, FLD_SORT_ALPHA}, 
	{"alpha-with-dirs-last",  NULL, FLD_SORT_ALPHA_DIR_LAST},
	{"alpha-with-dirs-first", NULL, FLD_SORT_ALPHA_DIR_FIRST}
    };

    return((index >= 0 && index < (sizeof(fdl_rules)/sizeof(fdl_rules[0])))
	   ? &fdl_rules[index] : NULL);
}


/*
 * Standard way to get at incoming startup rules...
 */
NAMEVAL_S *
incoming_startup_rules(index)
    int index;
{
    static NAMEVAL_S is_rules[] = {
	{"first-unseen",		NULL, IS_FIRST_UNSEEN},
	{"first-recent",		NULL, IS_FIRST_RECENT}, 
	{"first-important",		NULL, IS_FIRST_IMPORTANT}, 
	{"first-important-or-unseen",	NULL, IS_FIRST_IMPORTANT_OR_UNSEEN}, 
	{"first-important-or-recent",	NULL, IS_FIRST_IMPORTANT_OR_RECENT}, 
	{"first",			NULL, IS_FIRST},
	{"last",			NULL, IS_LAST}
    };

    return((index >= 0 && index < (sizeof(is_rules)/sizeof(is_rules[0])))
	   ? &is_rules[index] : NULL);
}


NAMEVAL_S *
startup_rules(index)
    int index;
{
    static NAMEVAL_S is2_rules[] = {
	{"first-unseen",		NULL, IS_FIRST_UNSEEN},
	{"first-recent",		NULL, IS_FIRST_RECENT}, 
	{"first-important",		NULL, IS_FIRST_IMPORTANT}, 
	{"first-important-or-unseen",	NULL, IS_FIRST_IMPORTANT_OR_UNSEEN}, 
	{"first-important-or-recent",	NULL, IS_FIRST_IMPORTANT_OR_RECENT}, 
	{"first",			NULL, IS_FIRST},
	{"last",			NULL, IS_LAST},
	{"default",			NULL, IS_NOTSET}
    };

    return((index >= 0 && index < (sizeof(is2_rules)/sizeof(is2_rules[0])))
	   ? &is2_rules[index] : NULL);
}


/*
 * Standard way to get at pruning-rule values.
 */
NAMEVAL_S *
pruning_rules(index)
    int index;
{
    static NAMEVAL_S pr_rules[] = {
	{"ask about rename, ask about deleting","ask-ask", PRUNE_ASK_AND_ASK},
	{"ask about rename, don't delete",	"ask-no",  PRUNE_ASK_AND_NO},
	{"always rename, ask about deleting",	"yes-ask", PRUNE_YES_AND_ASK}, 
	{"always rename, don't delete",		"yes-no",  PRUNE_YES_AND_NO}, 
	{"don't rename, ask about deleting",	"no-ask",  PRUNE_NO_AND_ASK}, 
	{"don't rename, don't delete",		"no-no",   PRUNE_NO_AND_NO} 
    };

    return((index >= 0 && index < (sizeof(pr_rules)/sizeof(pr_rules[0])))
	   ? &pr_rules[index] : NULL);
}


/*
 * Standard way to get at reopen-rule values.
 */
NAMEVAL_S *
reopen_rules(index)
    int index;
{
    static NAMEVAL_S ro_rules[] = {
	{"Always reopen",					"yes-yes",
							    REOPEN_YES_YES},
	{"Yes for POP/NNTP, Ask about other remote [Yes]",	"yes-ask-y",
							    REOPEN_YES_ASK_Y},
	{"Yes for POP/NNTP, Ask about other remote [No]",	"yes-ask-n",
							    REOPEN_YES_ASK_N},
	{"Yes for POP/NNTP, No for other remote",		"yes-no",
							    REOPEN_YES_NO},
	{"Always ask [Yes]",					"ask-ask-y",
							    REOPEN_ASK_ASK_Y},
	{"Always ask [No]",					"ask-ask-n",
							    REOPEN_ASK_ASK_N},
	{"Ask about POP/NNTP [Yes], No for other remote",	"ask-no-y",
							    REOPEN_ASK_NO_Y},
	{"Ask about POP/NNTP [No], No for other remote",	"ask-no-n",
							    REOPEN_ASK_NO_N},
	{"Never reopen",					"no-no",
							    REOPEN_NO_NO},
    };

    return((index >= 0 && index < (sizeof(ro_rules)/sizeof(ro_rules[0])))
	   ? &ro_rules[index] : NULL);
}


/*
 * Standard way to get at thread_disp_style values.
 */
NAMEVAL_S *
thread_disp_styles(index)
    int index;
{
    static NAMEVAL_S td_styles[] = {
	{"none",			"none",		THREAD_NONE},
	{"show-thread-structure",	"struct",	THREAD_STRUCT},
	{"mutt-like",			"mutt",		THREAD_MUTTLIKE},
	{"indent-subject-1",		"subj1",	THREAD_INDENT_SUBJ1},
	{"indent-subject-2",		"subj2",	THREAD_INDENT_SUBJ2},
	{"indent-from-1",		"from1",	THREAD_INDENT_FROM1},
	{"indent-from-2",		"from2",	THREAD_INDENT_FROM2},
	{"show-structure-in-from",	"struct-from",	THREAD_STRUCT_FROM}
    };

    return((index >= 0 && index < (sizeof(td_styles)/sizeof(td_styles[0])))
	   ? &td_styles[index] : NULL);
}


/*
 * Standard way to get at thread_index_style values.
 */
NAMEVAL_S *
thread_index_styles(index)
    int index;
{
    static NAMEVAL_S ti_styles[] = {
	{"regular-index-with-expanded-threads",	"exp",	THRDINDX_EXP},
	{"regular-index-with-collapsed-threads","coll",	THRDINDX_COLL},
	{"separate-index-screen-always",	"sep",	THRDINDX_SEP},
	{"separate-index-screen-except-for-single-messages","sep-auto",
							THRDINDX_SEP_AUTO}
    };

    return((index >= 0 && index < (sizeof(ti_styles)/sizeof(ti_styles[0])))
	   ? &ti_styles[index] : NULL);
}


/*
 * Standard way to get at goto default rules...
 */
NAMEVAL_S *
goto_rules(index)
    int index;
{
    static NAMEVAL_S g_rules[] = {
	{"folder-in-first-collection",		 NULL, GOTO_FIRST_CLCTN},
	{"inbox-or-folder-in-first-collection",	 NULL, GOTO_INBOX_FIRST_CLCTN},
	{"inbox-or-folder-in-recent-collection", NULL, GOTO_INBOX_RECENT_CLCTN},
	{"first-collection-with-inbox-default",	 NULL, GOTO_FIRST_CLCTN_DEF_INBOX},
	{"most-recent-folder",			 NULL, GOTO_LAST_FLDR}
    };

    return((index >= 0 && index < (sizeof(g_rules)/sizeof(g_rules[0])))
	   ? &g_rules[index] : NULL);
}


/*
 * The argument is an argument from the command line.  We check to see
 * if it is specifying an alternate value for one of the options that is
 * normally set in pinerc.  If so, we return 1 and set the appropriate
 * values in the variables array.
 * The arg can be
 *          varname=value
 *          varname=value1,value2,value3
 *          feature-list=featurename                 these are just a special
 *          feature-list=featurename1,featurename2   case of above
 *          featurename                            This is equivalent to above
 *          no-featurename
 */
int
pinerc_cmdline_opt(arg)
    char *arg;
{
    struct variable *v;
    char            *p1 = NULL,
                    *value,
		   **oldlvalue = NULL,
                   **lvalue;
    int              i, count;

    if(!arg || !arg[0])
      return 0;

    for(v = variables; v->name != NULL; v++){
      if(v->is_used && struncmp(v->name, arg, strlen(v->name)) == 0){
	  p1 = arg + strlen(v->name);

	  /*----- Skip to '=' -----*/
	  while(*p1 && (*p1 == '\t' || *p1 == ' '))
	    p1++;

	  if(*p1 != '='){
	      fprintf(stderr, "Missing \"=\" after -%s\n", v->name);
	      exit(-1);
	  }

	  p1++;
          break;
      }
    }

    /* no match, check for a feature name used directly */
    if(v->name == NULL){
	FEATURE_S *feat;
	char      *featname;

	if(struncmp(arg, "no-", 3) == 0)
	  featname = arg+3;
	else
	  featname = arg;

	for(i = 0; (feat = feature_list(i)) != NULL; i++){
	    if(strucmp(featname, feat->name) == 0){
		v = &variables[V_FEATURE_LIST];
		p1 = arg;
		break;
	    }
	}
    }

    if(!p1)
      return 0;

    if(v->is_obsolete || !v->is_user){
      fprintf(stderr, "Option \"%s\" is %s\n", v->name, v->is_obsolete ?
				       "obsolete" :
				       "not user settable");
      exit(-1);
    }

    /* free mem */
    if(v->is_list){
	oldlvalue = v->cmdline_val.l;
	v->cmdline_val.l = NULL;
    }
    else if(v->cmdline_val.p)
      fs_give((void **) &(v->cmdline_val.p));

    /*----- Matched a variable, get its value ----*/
    while(*p1 == ' ' || *p1 == '\t')
      p1++;
    value = p1;

    if(*value == '\0'){
      if(v->is_list){
        v->cmdline_val.l = (char **)fs_get(2 * sizeof(char *));
	/*
	 * we let people leave off the quotes on command line so that
	 * they don't have to mess with shell quoting
	 */
        v->cmdline_val.l[0] = cpystr("");
        v->cmdline_val.l[1] = NULL;
	if(oldlvalue)
	  free_list_array(&oldlvalue);

      }else{
        v->cmdline_val.p = cpystr("");
      }
      return 1;
    }

    /*--value is non-empty--*/
    if(*value == '"' && !v->is_list){
      value++;
      for(p1 = value; *p1 && *p1 != '"'; p1++);
        if(*p1 == '"')
          *p1 = '\0';
        else
          removing_trailing_white_space(value);
    }else{
      removing_trailing_white_space(value);
    }

    if(v->is_list){
      int   was_quoted = 0;
      char *error      = NULL;

      count = 1;
      for(p1=value; *p1; p1++){		/* generous count of list elements */
        if(*p1 == '"')			/* ignore ',' if quoted   */
          was_quoted = (was_quoted) ? 0 : 1;

        if(*p1 == ',' && !was_quoted)
          count++;
      }

      lvalue = parse_list(value, count, 0, &error);
      if(error){
	fprintf(stderr, "%s in %s = \"%s\"\n", error, v->name, value);
	exit(-1);
      }
      /*
       * Special case: turn "" strings into empty strings.
       * This allows users to turn off default lists.  For example,
       * if smtp-server is set then a user could override smtp-server
       * with smtp-server="".
       */
      for(i = 0; lvalue[i]; i++)
	if(lvalue[i][0] == '"' &&
	   lvalue[i][1] == '"' &&
	   lvalue[i][2] == '\0')
	     lvalue[i][0] = '\0';
	else if(!strucmp("cache-remote-pinerc", lvalue[i]))
	  ps_global->cache_remote_pinerc = 1;
	  
    }

    if(v->is_list){
	if(oldlvalue){
	    char **combinedlvalue;
	    int    j;

	    /* combine old and new cmdline lists */
	    for(count = 0, i = 0; oldlvalue[i]; i++, count++)
	      ;

	    for(i = 0; lvalue && lvalue[i]; i++, count++)
	      ;

	    combinedlvalue = (char **) fs_get((count+1) * sizeof(char *));
	    memset(combinedlvalue, 0, (count+1) * sizeof(*combinedlvalue));

	    for(i = 0, j = 0; oldlvalue[i]; i++, j++)
	      combinedlvalue[j] = cpystr(oldlvalue[i]);

	    for(i = 0; lvalue && lvalue[i]; i++, j++)
	      combinedlvalue[j] = cpystr(lvalue[i]);

	    v->cmdline_val.l = combinedlvalue;

	    fs_give((void **) &oldlvalue);
	    if(lvalue)
	      fs_give((void **) &lvalue);
	}
	else
	  v->cmdline_val.l = lvalue;
    }
    else
      v->cmdline_val.p = cpystr(value);

    return 1;
}


/*
 * Process the command list, changing function key notation into
 * lexical equivalents.
 */
void
process_init_cmds(ps, list)
    struct pine *ps;
    char **list;
{
    char **p;
    int i = 0;
    int j;
    int lpm1;
#define MAX_INIT_CMDS 500
    /* this is just a temporary stack array, the real one is allocated below */
    int i_cmds[MAX_INIT_CMDS];
    int fkeys = 0;
    int not_fkeys = 0;
  
    if(list){
      for(p = list; *p; p++){
	  if(i >= MAX_INIT_CMDS){
	      sprintf(tmp_20k_buf, 
		      "Initial keystroke list too long at \"%s\"", *p);
	      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	      break;
	  }
	  

	/* regular character commands */
	if(strlen(*p) == 1){
	  i_cmds[i++] = **p;
	  not_fkeys++;
	}

	/* special commands */
	else if(strucmp(*p, "SPACE") == 0)
	  i_cmds[i++] = ' ';
	else if(strucmp(*p, "CR") == 0)
	  i_cmds[i++] = '\n';
	else if(strucmp(*p, "TAB") == 0)
	  i_cmds[i++] = '\t';
	else if(strucmp(*p, "UP") == 0)
	  i_cmds[i++] = KEY_UP;
	else if(strucmp(*p, "DOWN") == 0)
	  i_cmds[i++] = KEY_DOWN;
	else if(strucmp(*p, "LEFT") == 0)
	  i_cmds[i++] = KEY_LEFT;
	else if(strucmp(*p, "RIGHT") == 0)
	  i_cmds[i++] = KEY_RIGHT;

	/* control chars */
	else if(strlen(*p) == 2 && **p == '^')
	  i_cmds[i++] = ctrl(*((*p)+1));

	/* function keys */
	else if(**p == 'F' || **p == 'f'){
	    int v;

	    fkeys++;
	    v = atoi((*p)+1);
	    if(v >= 1 && v <= 12)
	      i_cmds[i++] = PF1 + v - 1;
	    else
	      i_cmds[i++] = KEY_JUNK;
	}

	/* literal string */
	else if(**p == '"' && (*p)[lpm1 = strlen(*p) - 1] == '"'){
	    if(lpm1 + i - 1 > MAX_INIT_CMDS){
		sprintf(tmp_20k_buf, 
			"Initial keystroke list too long, truncated at %s\n", *p);
		init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
		break;                   /* Bail out of this loop! */
	    } else
	      for(j = 1; j < lpm1; j++)
		i_cmds[i++] = (*p)[j];
	}
	else {
	    sprintf(tmp_20k_buf,
		    "Bad initial keystroke \"%.500s\" (missing comma?)", *p);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	    break;
	}
      }
    }

    /*
     * We don't handle the case where function keys are used to specify the
     * commands but some non-function key input is also required.  For example,
     * you might want to jump to a specific message number and view it
     * on start up.  To do that, you need to use character commands instead
     * of function key commands in the initial-keystroke-list.
     */
    if(fkeys && not_fkeys){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
"Mixed characters and function keys in \"initial-keystroke-list\", skipping.");
	i = 0;
    }

    if(fkeys && !not_fkeys)
      F_TURN_ON(F_USE_FK,ps);
    if(!fkeys && not_fkeys)
      F_TURN_OFF(F_USE_FK,ps);

    if(i > 0){
	ps->initial_cmds = (int *)fs_get((i+1) * sizeof(int));
	ps->free_initial_cmds = ps->initial_cmds;
	for(j = 0; j < i; j++)
	  ps->initial_cmds[j] = i_cmds[j];

	ps->initial_cmds[i] = 0;
	ps->in_init_seq = ps->save_in_init_seq = 1;
    }
}


/*
 * Choose from the global default, command line args, pinerc values to set
 * the actual value of the variable that we will use.  Start at the top
 * and work down from higher to lower precedence.
 * For lists, we may inherit values from lower precedence
 * versions if that's the way the user specifies it.
 * The user can put INHERIT_DEFAULT as the first entry in a list and that
 * means it will inherit the current values, for example the values
 * from the global_val, or the value from the main_user_val could be
 * inherited in the post_user_val.
 */
void    
set_current_val(var, expand, cmdline)
    struct variable *var;
    int              expand, cmdline;
{
    int    is_set[5], is_inherit[5];
    int    i, j, k, cnt, start;
    char **tmp, **t, **list[5];
    char  *p;

    dprint(9, (debugfile,
	       "set_current_val(var=%s%s, expand=%d, cmdline=%d)\n",
	       (var && var->name) ? var->name : "?",
	       (var && var->is_list) ? " (list)" : "",
	       expand, cmdline));

    if(!var)
      return;

    if(var->is_list){			  /* variable is a list */

	for(j = 0; j < 5; j++){
	    t = j==0 ? var->global_val.l :
		j==1 ? var->main_user_val.l :
		j==2 ? var->post_user_val.l :
		j==3 ? ((cmdline) ? var->cmdline_val.l : NULL) :
		       var->fixed_val.l;

	    is_set[j] = is_inherit[j] = 0;
	    list[j] = NULL;

	    if(t){
		if(!expand){
		    is_set[j]++;
		    list[j] = t;
		}
		else{
		    for(i = 0; t[i]; i++){
			if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, t[i],
					    0)){
			    /* successful expand */
			    is_set[j]++;
			    list[j] = t;
			    break;
			}
		    }
		}

		if(list[j] && list[j][0] && !strcmp(list[j][0],INHERIT))
		  is_inherit[j]++;
	    }
	}

	cnt = 0;
	start = 0;
	/* count how many items in current_val list */
	/* Admin wants default, which is global_val. */
	if(var->is_fixed && var->fixed_val.l == NULL){
	    cnt = 0;
	    if(is_set[0]){
		for(; list[0][cnt]; cnt++)
		  ;
	    }
	}
	else{
	    for(j = 0; j < 5; j++){
		if(is_set[j]){
		    if(!is_inherit[j]){
			cnt = 0;	/* reset */
			start = j;
		    }

		    for(i = is_inherit[j] ? 1 : 0; list[j][i]; i++)
		      cnt++;
		}
	    }
	}

	free_list_array(&var->current_val.l);	 /* clean up any old values */

	/* check to see if anything is set */
	if(is_set[0] + is_set[1] + is_set[2] + is_set[3] + is_set[4] > 0){
	    var->current_val.l = (char **)fs_get((cnt+1)*sizeof(char *));
	    tmp = var->current_val.l;
	    if(var->is_fixed && var->fixed_val.l == NULL){
		if(is_set[0]){
		    for(i = 0; list[0][i]; i++){
			if(!expand)
			  *tmp++ = cpystr(list[0][i]);
			else if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
						 list[0][i], 0))
			  *tmp++ = cpystr(tmp_20k_buf);
		    }
		}
	    }
	    else{
		for(j = start; j < 5; j++){
		    if(is_set[j]){
			for(i = is_inherit[j] ? 1 : 0; list[j][i]; i++){
			    if(!expand)
			      *tmp++ = cpystr(list[j][i]);
			    else if(expand_variables(tmp_20k_buf,SIZEOF_20KBUF,
						     list[j][i], 0))
			      *tmp++ = cpystr(tmp_20k_buf);
			}
		    }
		}
	    }

	    *tmp = NULL;
	}
	else
	  var->current_val.l = NULL;
    }
    else{  /* variable is not a list */
	char *strvar = NULL;

	for(j = 0; j < 5; j++){

	    p = j==0 ? var->fixed_val.p :
		j==1 ? ((cmdline) ? var->cmdline_val.p : NULL) :
		j==2 ? var->post_user_val.p :
		j==3 ? var->main_user_val.p :
		       var->global_val.p;

	    is_set[j] = 0;

	    if(p){
		if(!expand){
		    is_set[j]++;
		    if(!strvar)
			strvar = p;
		}
		else if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, p,
				(var == &ps_global->vars[V_MAILCAP_PATH] ||
				 var == &ps_global->vars[V_MIMETYPE_PATH]))){
		    is_set[j]++;
		    if(!strvar)
			strvar = p;
		}
	    }
	}

	/* Admin wants default, which is global_val. */
	if(var->is_fixed && var->fixed_val.p == NULL)
	  strvar = var->global_val.p;

	if(var->current_val.p)		/* free previous value */
	  fs_give((void **)&var->current_val.p);

	if(strvar){
	    if(!expand)
	      var->current_val.p = cpystr(strvar);
	    else{
		expand_variables(tmp_20k_buf, SIZEOF_20KBUF, strvar,
				 (var == &ps_global->vars[V_MAILCAP_PATH] ||
				  var == &ps_global->vars[V_MIMETYPE_PATH]));
		var->current_val.p = cpystr(tmp_20k_buf);
	    }
	}
	else
	  var->current_val.p = NULL;
    }

    if(var->is_fixed && !is_inherit[4]){
	char **flist;
	int fixed_len, user_len;

	/*
	 * sys mgr fixed this variable and user is trying to change it
	 */
	for(k = 1; !(ps_global->give_fixed_warning &&
		     ps_global->fix_fixed_warning) && k <= 3; k++){
	    if(is_set[k]){
		if(var->is_list){
		    t = k==1 ? ((cmdline) ? var->cmdline_val.l : NULL) :
			k==2 ? var->post_user_val.l :
			       var->main_user_val.l;

		    /* If same length and same contents, don't warn. */
		    for(flist=var->fixed_val.l; flist && *flist; flist++)
		      ;/* just counting */

		    fixed_len = var->fixed_val.l ? (flist - var->fixed_val.l)
						 : 0;
		    for(flist=t; flist && *flist; flist++)
		      ;/* just counting */

		    user_len = t ? (flist - t) : 0;
		    if(user_len == fixed_len){
		      for(i=0; i < user_len; i++){
			for(j=0; j < user_len; j++)
			  if(!strucmp(t[i], var->fixed_val.l[j]))
			    break;
			  
			if(j == user_len){
			  ps_global->give_fixed_warning = 1;
			  if(k != 1)
			    ps_global->fix_fixed_warning = 1;

			  break;
			}
		      }
		    }
		    else{
			ps_global->give_fixed_warning = 1;
			if(k != 1)
			  ps_global->fix_fixed_warning = 1;
		    }
		}
		else{
		    p = k==1 ? ((cmdline) ? var->cmdline_val.p : NULL) :
			k==2 ? var->post_user_val.p :
			       var->main_user_val.p;
		    
		    if((var->fixed_val.p && !p) ||
		       (!var->fixed_val.p && p) ||
		       (var->fixed_val.p && p && strucmp(var->fixed_val.p, p))){
			ps_global->give_fixed_warning = 1;
			if(k != 1)
			  ps_global->fix_fixed_warning = 1;
		    }
		}
	    }
	}
    }
}


void
set_news_spec_current_val(expand, cmdline)
    int expand, cmdline;
{
    struct variable *newsvar = &ps_global->vars[V_NEWS_SPEC];
    struct variable *fvar    = &ps_global->vars[V_FOLDER_SPEC];

    /* check to see if it has a value */
    set_current_val(newsvar, expand, cmdline);

    /*
     * If no value, we might want to fake a value. We'll do that if
     * there is no news collection already defined in FOLDER_SPEC and if
     * there is also an NNTP_SERVER defined.
     */
    if(!newsvar->current_val.l && ps_global->VAR_NNTP_SERVER &&
       ps_global->VAR_NNTP_SERVER[0] && ps_global->VAR_NNTP_SERVER[0][0] &&
       !news_in_folders(fvar)){
	char buf[MAXPATH];

	newsvar->global_val.l = (char **)fs_get(2 * sizeof(char *));
	sprintf(buf, "{%.*s/nntp}#news.[]", sizeof(buf)-20,
		ps_global->VAR_NNTP_SERVER[0]);
	newsvar->global_val.l[0] = cpystr(buf);
	newsvar->global_val.l[1] = NULL;
	set_current_val(newsvar, expand, cmdline);
	/*
	 * But we're going to get rid of the fake global_val in case
	 * things change.
	 */
	free_list_array(&newsvar->global_val.l);
    }
}


/*
 * Feature-list has to be handled separately from the other variables
 * because it is additive.  The other variables choose one of command line,
 * or pine.conf, or pinerc.  Feature list adds them.  This could easily be
 * converted to a general purpose routine if we add more additive variables.
 *
 * This works by replacing earlier values with later ones.  That is, command
 * line settings have higher precedence than global settings and that is
 * accomplished by putting the command line features after the global
 * features in the list.  When they are processed, the last one wins.
 *
 * Feature-list also has a backwards compatibility hack.
 */
void    
set_feature_list_current_val(var)
  struct variable *var;
{
    char **list;
    char **list_fixed;
    char   no_allow[110], *allow;
    int    i, j, k, m,
	   elems = 0;

    elems++;	/* for default F_ALLOW_CHANGING_FROM */

    /* count the lists so we can allocate */
    for(m = 0; m < 6; m++){
	list = m==0 ? var->global_val.l :
		m==1 ? var->main_user_val.l :
		 m==2 ? var->post_user_val.l :
		  m==3 ? ps_global->feat_list_back_compat :
		   m==4 ? var->cmdline_val.l :
		           var->fixed_val.l;
	if(list)
	  for(i = 0; list[i]; i++)
	    elems++;
    }

    list_fixed = var->fixed_val.l;

    if(var->current_val.l)
      free_list_array(&var->current_val.l);

    var->current_val.l = (char **)fs_get((elems+1) * sizeof(char *));

    /*
     * We need to warn the user if the sys mgr has restricted him or her
     * from changing a feature that he or she is trying to change.
     *
     * We're not catching the old-growth macro since we're just comparing
     * strings. That is, it works correctly, but the user won't be warned
     * if the user old-growth and the mgr says no-quit-without-confirm.
     */

    j = 0;
    /* everything defaults to off except for allow-changing-from */
    allow = no_allow+3;
    strncpy(no_allow, "no-", 3);
    strncpy(allow, feature_list_name(F_ALLOW_CHANGING_FROM), 100);
    var->current_val.l[j++] = cpystr(allow);

    for(m = 0; m < 6; m++){
	list = m==0 ? var->global_val.l :
		m==1 ? var->main_user_val.l :
		 m==2 ? var->post_user_val.l :
		  m==3 ? ps_global->feat_list_back_compat :
		   m==4 ? var->cmdline_val.l :
		           var->fixed_val.l;
	if(list)
	  for(i = 0; list[i]; i++){
	      var->current_val.l[j++] = cpystr(list[i]);
	      if(m >= 1 && m <= 4){
		  for(k = 0; list_fixed && list_fixed[k]; k++){
		      char *p, *q;
		      p = list[i];
		      q = list_fixed[k];
		      if(!struncmp(p, "no-", 3))
			p += 3;
		      if(!struncmp(q, "no-", 3))
			q += 3;
		      if(!strucmp(q, p) && strucmp(list[i], list_fixed[k])){
			  ps_global->give_fixed_warning = 1;
			  if(m <= 2)
			    ps_global->fix_fixed_warning = 1;
		      }
		  }
	      }
	      else if(m == 5 && !strucmp(list[i], no_allow))
	        ps_global->never_allow_changing_from = 1;
	  }
    }

#ifdef	NEVER_ALLOW_CHANGING_FROM
    ps_global->never_allow_changing_from = 1;
#endif

    var->current_val.l[j] = NULL;
}
                                                     


/*----------------------------------------------------------------------

	Expand Metacharacters/variables in file-names

   Read input line and expand shell-variables/meta-characters

	<input>		<replaced by>
	$variable	getenv("variable")
	${variable}	getenv("variable")
	${variable:-defvalue} is getenv("variable") if variable is defined and
	                      is defvalue otherwise
	~		getenv("HOME")
	\c		c
	<others>	<just copied>

NOTE handling of braces in ${name} doesn't check much or do error recovery

   If colon_path is set, then we expand ~ not only at the start of linein,
   but also after each : in the path.
	
  ----*/

char *
expand_variables(lineout, lineoutlen, linein, colon_path)
    char  *lineout;
    size_t lineoutlen;
    char  *linein;
    int    colon_path;
{
    char *src = linein, *dest = lineout, *p;
    char *limit = lineout + lineoutlen;
    int   envexpand = 0;

    if(!linein)
      return(NULL);

    while(*src ){			/* something in input string */
        if(*src == '$' && *(src+1) == '$'){
	    /*
	     * $$ to escape chars we're interested in, else
	     * it's up to the user of the variable to handle the 
	     * backslash...
	     */
	    if(dest < limit)
              *dest++ = *++src;		/* copy next as is */
        }else
#if !(defined(DOS) || defined(OS2))
        if(*src == '\\' && *(src+1) == '$'){
	    /*
	     * backslash to escape chars we're interested in, else
	     * it's up to the user of the variable to handle the 
	     * backslash...
	     */
	    if(dest < limit)
              *dest++ = *++src;		/* copy next as is */
        }else if(*src == '~' &&
		 (src == linein || colon_path && *(src-1) == ':')){
	    char buf[MAXPATH];
	    int  i;

	    for(i = 0; i < sizeof(buf)-1 && src[i] && src[i] != '/'; i++)
	      buf[i] = src[i];

	    src    += i;		/* advance src pointer */
	    buf[i]  = '\0';		/* tie off buf string */
	    fnexpand(buf, sizeof(buf));	/* expand the path */

	    for(p = buf; dest < limit && (*dest = *p); p++, dest++)
	      ;

	    continue;
        }else
#endif
	if(*src == '$'){		/* shell variable */
	    char word[128+1], *colon = NULL, *rbrace = NULL;

	    envexpand++;		/* signal that we've expanded a var */
	    src++;			/* skip dollar */
	    if(*src == '{'){		/* starts with brace? */
		src++;        
		rbrace = strindex(src, '}');
		if(rbrace){
		    /* look for default value */
		    colon = strstr(src, ":-");
		    if(colon && (rbrace < colon))
		      colon = NULL;
		}
	    }

	    p = word;

	    /* put the env variable to be looked up in word */
	    if(rbrace){
		while(*src
		      && (p-word < sizeof(word)-1)
		      && ((colon && src < colon) || (!colon && src < rbrace))){
		    if(isspace((unsigned char) *src)){
			/*
			 * Illegal input. This should be an error of some
			 * sort but instead of that we'll just backup to the
			 * $ and treat it like it wasn't there.
			 */
			while(*src != '$')
			  src--;
			
			envexpand--;
			goto just_copy;
		    }
		    else
		      *p++ = *src++;
		}

		/* adjust src for next char */
		src = rbrace + 1;
	    }
	    else{
		while(*src && !isspace((unsigned char) *src)
		      && (p-word < sizeof(word)-1))
		  *p++ = *src++;
	    }

	    *p = '\0';

	    if((p = getenv(word)) != NULL){ /* check for word in environment */
		while(*p && dest < limit)
		  *dest++ = *p++;
	    }
	    else if(colon){		    /* else possible default value */
		p = colon + 2;
		while(*p && p < rbrace && dest < limit)
		  *dest++ = *p++;
	    }

	    continue;
	}else{				/* other cases: just copy */
just_copy:
	    if(dest < limit)
	      *dest++ = *src;
	}

        if(*src)			/* next character (if any) */
	  src++;
    }

    if(dest < limit)
      *dest = '\0';
    else
      lineout[lineoutlen-1] = '\0';

    return((envexpand && lineout[0] == '\0') ? NULL : lineout);
}


/*----------------------------------------------------------------------
    Sets  login, full_username and home_dir

   Args: ps -- The Pine structure to put the user name, etc in

  Result: sets the fullname, login and home_dir field of the pine structure
          returns 0 on success, -1 if not.
  ----*/
#define	MAX_INIT_ERRS	10
void
init_error(ps, flags, min_time, max_time, message)
    struct pine *ps;
    int          flags;
    int          min_time, max_time;
    char	*message;
{
    int    i;

    if(!ps->init_errs){
	ps->init_errs = (INIT_ERR_S *)fs_get((MAX_INIT_ERRS + 1) *
					     sizeof(*ps->init_errs));
	memset(ps->init_errs, 0, (MAX_INIT_ERRS + 1) * sizeof(*ps->init_errs));
    }

    for(i = 0; i < MAX_INIT_ERRS; i++)
      if(!(ps->init_errs)[i].message){
	  (ps->init_errs)[i].message  = cpystr(message);
	  (ps->init_errs)[i].min_time = min_time;
	  (ps->init_errs)[i].max_time = max_time;
	  (ps->init_errs)[i].flags    = flags;
	  dprint(2, (debugfile, "%s\n", message ? message : "?"));
	  break;
      }
}


/*----------------------------------------------------------------------
    Sets  login, full_username and home_dir

   Args: ps -- The Pine structure to put the user name, etc in

  Result: sets the fullname, login and home_dir field of the pine structure
          returns 0 on success, -1 if not.
  ----*/

init_username(ps)
     struct pine *ps;
{
    char fld_dir[MAXPATH+1], *expanded;
    int  rv;

    rv       = 0;
    expanded = NULL;
#if defined(DOS) || defined(OS2)
    if(ps->COM_USER_ID)
      expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				  ps->COM_USER_ID, 0);
    
    if(!expanded && ps->vars[V_USER_ID].post_user_val.p)
      expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				  ps->vars[V_USER_ID].post_user_val.p, 0);

    if(!expanded && ps->vars[V_USER_ID].main_user_val.p)
      expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				  ps->vars[V_USER_ID].main_user_val.p, 0);

    if(!expanded)
      ps->blank_user_id = 1;

    ps->VAR_USER_ID = cpystr(expanded ? expanded : "");
#else
    ps->VAR_USER_ID = cpystr(ps->ui.login);
    if(!ps->VAR_USER_ID[0]){
        fprintf(stderr, "Who are you? (Unable to look up login name)\n");
        rv = -1;
    }
#endif

    expanded = NULL;
    if(ps->vars[V_PERSONAL_NAME].is_fixed){
	if(ps->FIX_PERSONAL_NAME){
            expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
					ps->FIX_PERSONAL_NAME, 0);
	}
	if(ps->vars[V_PERSONAL_NAME].main_user_val.p ||
	   ps->vars[V_PERSONAL_NAME].post_user_val.p){
	    ps_global->give_fixed_warning = 1;
	    ps_global->fix_fixed_warning = 1;
	}
	else if(ps->COM_PERSONAL_NAME)
	  ps_global->give_fixed_warning = 1;
    }
    else{
	if(ps->COM_PERSONAL_NAME)
	  expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				      ps->COM_PERSONAL_NAME, 0);

	if(!expanded && ps->vars[V_PERSONAL_NAME].post_user_val.p)
	  expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
			      ps->vars[V_PERSONAL_NAME].post_user_val.p, 0);

	if(!expanded && ps->vars[V_PERSONAL_NAME].main_user_val.p)
	  expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
			      ps->vars[V_PERSONAL_NAME].main_user_val.p, 0);
    }

    if(!expanded){
	expanded = ps->ui.fullname;
#if defined(DOS) || defined(OS2)
	ps->blank_personal_name = 1;
#endif
    }

    ps->VAR_PERSONAL_NAME = cpystr(expanded ? expanded : "");

    if(strlen(ps->home_dir) + strlen(ps->VAR_MAIL_DIRECTORY)+2 > MAXPATH){
        printf("Folders directory name is longer than %d\n", MAXPATH);
        printf("Directory name: \"%s/%s\"\n",ps->home_dir,
               ps->VAR_MAIL_DIRECTORY);
        return(-1);
    }
#if defined(DOS) || defined(OS2)
    if(ps->VAR_MAIL_DIRECTORY[1] == ':'){
	strncpy(fld_dir, ps->VAR_MAIL_DIRECTORY, sizeof(fld_dir)-1);
	fld_dir[sizeof(fld_dir)-1] = '\0';
    }
    else
#endif
    build_path(fld_dir, ps->home_dir, ps->VAR_MAIL_DIRECTORY, sizeof(fld_dir));
    ps->folders_dir = cpystr(fld_dir);

    dprint(1, (debugfile, "Userid: %s\nFullname: \"%s\"\n",
               ps->VAR_USER_ID ? ps->VAR_USER_ID : "?",
	       ps->VAR_PERSONAL_NAME ? ps->VAR_PERSONAL_NAME : "?"));
    return(rv);
}


/*----------------------------------------------------------------------
        Fetch the hostname of the current system and put it in pine struct

   Args: ps -- The pine structure to put the hostname, etc in

  Result: hostname, localdomain, userdomain and maildomain are set


** Pine uses the following set of names:
  hostname -    The fully-qualified hostname.  Obtained with
		gethostbyname() which reads /etc/hosts or does a DNS
		lookup.  This may be blank.
  localdomain - The domain name without the host.  Obtained from the
		above hostname if it has a "." in it.  Removes first
		segment.  If hostname has no "." in it then the hostname
		is used.  This may be blank.
  userdomain -  Explicitly configured domainname.  This is read out of the
		global pine.conf or user's .pinerc.  The user's entry in the
		.pinerc overrides.

** Pine has the following uses for such names:

  1. On outgoing messages in the From: line
	Uses userdomain if there is one.  If not uses, uses
	hostname unless Pine has been configured to use localdomain.

  2. When expanding/fully-qualifying unqualified addresses during
     composition
	(same as 1)

  3. When expanding/fully-qualifying unqualified addresses during
     composition when a local entry in the password file exists for
     name.
        If no userdomain is given, then this lookup is always done
	and the hostname is used unless Pine's been configured to 
	use the localdomain.  If userdomain is defined, it is used,
	but no local lookup is done.  We can't assume users on the
	local host are valid in the given domain (and, for simplicity,
	have chosen to ignore the cases userdomain matches localdomain
	or localhost).  Setting user-lookup-even-if-domain-mismatch
	feature will tell pine to override this behavior and perform
	the local lookup anyway.  The problem of a global "even-if"
	set and a .pinerc-defined user-domain of something odd causing
	the local lookup, but this will only effect the personal name, 
	and is not judged to be a significant problem.

  4. In determining if an address is that of the current pine user for
     formatting index and filtering addresses when replying
	If a userdomain is specified the address must match the
	userdomain exactly.  If a userdomain is not specified or the
	userdomain is the same as the hostname or domainname, then
	an address will be considered the users if it matches either
	the domainname or the hostname.  Of course, the userid must
	match too. 

  5. In Message ID's
	The fully-qualified hostname is always users here.


** Setting the domain names
  To set the domain name for all Pine users on the system to be
different from what Pine figures out from DNS, set the domain name in
the "user-domain" variable in pine.conf.  To set the domain name for an
individual user, set the "user-domain" variable in his .pinerc.
The .pinerc setting overrides any other setting.
 ----*/
init_hostname(ps)
     struct pine *ps;
{
    char hostname[MAX_ADDRESS+1], domainname[MAX_ADDRESS+1];

    getdomainnames(hostname, sizeof(hostname)-1,
		   domainname, sizeof(domainname)-1);

    if(ps->hostname)
      fs_give((void **)&ps->hostname);

    ps->hostname = cpystr(hostname);

    if(ps->localdomain)
      fs_give((void **)&ps->localdomain);

    ps->localdomain = cpystr(domainname);
    ps->userdomain  = NULL;

    if(ps->VAR_USER_DOMAIN && ps->VAR_USER_DOMAIN[0]){
        ps->maildomain = ps->userdomain = ps->VAR_USER_DOMAIN;
    }else{
#if defined(DOS) || defined(OS2)
	if(ps->VAR_USER_DOMAIN)
	  ps->blank_user_domain = 1;	/* user domain set to null string! */

        ps->maildomain = ps->localdomain[0] ? ps->localdomain : ps->hostname;
#else
        ps->maildomain = strucmp(ps->VAR_USE_ONLY_DOMAIN_NAME, "yes")
			  ? ps->hostname : ps->localdomain;
#endif
    }

    /*
     * Tell c-client what domain to use when completing unqualified
     * addresses it finds in local mailboxes.  Remember, it won't 
     * affect what's to the right of '@' for unqualified addresses in
     * remote folders...
     */
    mail_parameters(NULL, SET_LOCALHOST, (void *) ps->maildomain);
    if(F_OFF(F_QUELL_MAILDOMAIN_WARNING, ps) && !strchr(ps->maildomain, '.')){
	sprintf(tmp_20k_buf, "Incomplete maildomain \"%s\".",
		ps->maildomain);
	init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	strcpy(tmp_20k_buf,
	       "Return address in mail you send may be incorrect.");
	init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    }

    dprint(1, (debugfile,"User domain name being used \"%s\"\n",
               ps->userdomain == NULL ? "" : ps->userdomain));
    dprint(1, (debugfile,"Local Domain name being used \"%s\"\n",
               ps->localdomain ? ps->localdomain : "?"));
    dprint(1, (debugfile,"Host name being used \"%s\"\n",
               ps->hostname ? ps->hostname : "?"));
    dprint(1, (debugfile,
	       "Mail Domain name being used (by c-client too) \"%s\"\n",
               ps->maildomain ? ps->maildomain : "?"));

    if(!ps->maildomain || !ps->maildomain[0]){
#if defined(DOS) || defined(OS2)
	if(ps->blank_user_domain)
	  return(0);		/* prompt for this in send.c:dos_valid_from */
#endif
        fprintf(stderr, "No host name or domain name set\n");
        return(-1);
    }
    else
      return(0);
}


/*----------------------------------------------------------------------
         Read and parse a pinerc file
  
   Args:  Filename   -- name of the .pinerc file to open and read
          vars       -- The vars structure to store values in
          which_vars -- Whether the local or global values are being read

   Result: 

 This may be the local file or the global file.  The values found are
merged with the values currently in vars.  All values are strings and
are malloced; and existing values will be freed before the assignment.
Those that are <unset> will be left unset; their values will be NULL.
  ----*/
void
read_pinerc(prc, vars, which_vars)
     PINERC_S *prc;
     ParsePinerc which_vars;
     struct variable *vars;
{
    char               *filename, *file, *value, **lvalue, *line, *error;
    register char      *p, *p1;
    struct variable    *v;
    PINERC_LINE        *pline = NULL;
    int                 line_count, was_quoted;
    int			i;

    if(!prc)
      return;

    dprint(2, (debugfile, "reading_pinerc \"%s\"\n",
	   prc->name ? prc->name : "?"));

    if(prc->type == Loc){
	filename = prc->name ? prc->name : "";
	file = read_file(filename);
    }
    else
      file = read_remote_pinerc(prc, which_vars);

    if(file == NULL || *file == '\0'){
	if(file == NULL){
          dprint(2, (debugfile, "Open failed: %s\n", error_description(errno)));
	}
	else{
	    if(prc->type == Loc){
	      dprint(1, (debugfile, "Read_pinerc: empty pinerc (new?)\n"));
	    }
	    else{
	      dprint(1, (debugfile, "Read_pinerc: new remote pinerc\n"));
	    }
	}

	if(which_vars == ParsePers){
	    /* problems getting remote config */
	    if(file == NULL && prc->type == RemImap){
#ifdef _WINDOWS
		if(ps_global->install_flag)  /* just exit silently */
		  exit(0);
#endif /* _WINDOWS */
		if(ps_global->exit_if_no_pinerc){
		    fprintf(stderr,
	"Exiting because -bail option is set and config file not readable.\n");
		    exit(-1);
		}

		if(want_to("Trouble reading remote configuration! Continue anyway ",
			   'n', 'n', NO_HELP, WT_FLUSH_IN) != 'y'){
		    exit(-1);
		}
	    }

	    ps_global->first_time_user = 1;
	    prc->outstanding_pinerc_changes = 1;
	}

        return;
    }
    else{
	if(prc->type == Loc &&
	   (which_vars == ParseFixed || which_vars == ParseGlobal ||
	    (can_access(filename, ACCESS_EXISTS) == 0 &&
	     can_access(filename, EDIT_ACCESS) != 0))){
	    prc->readonly = 1;
	    if(prc == ps_global->prc)
	      ps_global->readonly_pinerc = 1;
	}

	/*
	 * accept CRLF or LF newlines
	 */
	for(p = file; *p && *p != '\012'; p++)
	  ;

	if(p > file && *p && *(p-1) == '\015')	/* cvt crlf to lf */
	  for(p1 = p - 1; *p1 = *p; p++)
	    if(!(*p == '\015' && *(p+1) == '\012'))
	      p1++;
    }

    dprint(2, (debugfile, "Read %d characters:\n", strlen(file)));

    if(which_vars == ParsePers || which_vars == ParsePersPost){
	/*--- Count up lines and allocate structures */
	for(line_count = 0, p = file; *p != '\0'; p++)
          if(*p == '\n')
	    line_count++;

	prc->pinerc_lines = (PINERC_LINE *)
			       fs_get((3 + line_count) * sizeof(PINERC_LINE));
	memset((void *)prc->pinerc_lines, 0,
	       (3 + line_count) * sizeof(PINERC_LINE));
	pline = prc->pinerc_lines;
    }

    for(p = file, line = file; *p != '\0';){
        /*----- Grab the line ----*/
        line = p;
        while(*p && *p != '\n')
          p++;
        if(*p == '\n'){
            *p++ = '\0';
        }

        /*----- Comment Line -----*/
        if(*line == '#'){
	    /* no comments in remote pinercs */
            if(pline && prc->type == Loc){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

	if(*line == '\0' || *line == '\t' || *line == ' '){
            p1 = line;
            while(*p1 == '\t' || *p1 == ' ')
               p1++;
            if(pline){
		/*
		 * This could be a continuation line from some future
		 * version of pine, or it could be a continuation line
		 * from a PC-Pine variable we don't know about in unix.
		 */
	        if(*p1 != '\0')
                    pline->line = cpystr(line);
	        else
                    pline->line = cpystr("");
               pline->is_var = 0;
               pline++;
            }
            continue;
	}

        /*----- look up matching 'v' and leave "value" after '=' ----*/
        for(v = vars; *line && v->name; v++)
	  if((i = strlen(v->name)) < strlen(line) && !struncmp(v->name,line,i)){
	      int j;

	      for(j = i; line[j] == ' ' || line[j] == '\t'; j++)
		;

	      if(line[j] == '='){	/* bingo! */
		  for(value = &line[j+1];
		      *value == ' ' || *value == '\t';
		      value++)
		    ;

		  break;
	      }
	      /* else either unrecognized var or bogus line */
	  }

        /*----- Didn't match any variable or bogus format -----*/
	/*
	 * This could be a variable from some future
	 * version of pine, or it could be a PC-Pine variable
	 * we don't know about in unix. Either way, we want to preserve
	 * it in the file.
	 */
        if(!v->name){
            if(pline){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

	/*
	 * Previous versions have caused duplicate pinerc data to be
	 * written to pinerc files. This clause erases the duplicate
	 * information when we read it, and it will be removed from the file
	 * if we call write_pinerc. We test to see if the same variable
	 * appears later in the file, if so, we skip over it here.
	 * We don't care about duplicates if this isn't a pinerc we might
	 * write out, so include pline in the conditional.
	 * Note that we will leave all of the duplicate comments and blank
	 * lines in the file unless it is a remote pinerc. Luckily, the
	 * but that caused the duplicates only applied to remote pinercs,
	 * so we should have that case covered.
	 *
	 * If we find a duplicate, we point p to the start
	 * of the next line that should be considered, and then skip back
	 * to the top of the loop.
	 */
	if(pline && var_is_in_rest_of_file(v->name, p)){
	    if(v->is_list)
	      p = skip_over_this_var(line, p);

	    continue;
	}

	
        /*----- Obsolete variable, read it anyway below, might use it -----*/
        if(v->is_obsolete){
            if(pline){
                pline->obsolete_var = 1;
                pline->line = cpystr(line);
                pline->var = v;
            }
        }

        /*----- Variable is in the list but unused for some reason -----*/
        if(!v->is_used){
            if(pline){
                pline->is_var = 0;
                pline->line = cpystr(line);
                pline++;
            }
            continue;
        }

        /*--- Var is not user controlled, leave it alone for back compat ---*/
        if(!v->is_user && pline){
	    pline->is_var = 0;
	    pline->line = cpystr(line);
	    pline++;
	    continue;
        }

	if(which_vars == ParseFixed)
	  v->is_fixed = 1;

        /*---- variable is unset, or it's global but expands to nothing ----*/
        if(!*value
	   || (which_vars == ParseGlobal
	       && !expand_variables(tmp_20k_buf, SIZEOF_20KBUF, value,
				    (v == &ps_global->vars[V_MAILCAP_PATH] ||
				     v == &ps_global->vars[V_MIMETYPE_PATH])))){
            if(v->is_user && pline){
                pline->is_var   = 1;
                pline->var = v;
                pline++;
            }
            continue;
        }

        /*--value is non-empty, store it handling quotes and trailing space--*/
        if(*value == '"' && !v->is_list && v->del_quotes){
            was_quoted = 1;
            value++;
            for(p1 = value; *p1 && *p1 != '"'; p1++);
            if(*p1 == '"')
              *p1 = '\0';
            else
              removing_trailing_white_space(value);
        }else
          was_quoted = 0;

	/*
	 * List Entry Parsing
	 *
	 * The idea is to parse a comma separated list of 
	 * elements, preserving quotes, and understanding
	 * continuation lines (that is ',' == "\n ").
	 * Quotes must be balanced within elements.  Space 
	 * within elements is preserved, but leading and trailing 
	 * space is trimmed.  This is a generic function, and it's 
	 * left to the the functions that use the lists to make sure
	 * they contain valid data...
	 */
	if(v->is_list){

	    was_quoted = 0;
	    line_count = 0;
	    p1         = value;
	    while(1){			/* generous count of list elements */
		if(*p1 == '"')		/* ignore ',' if quoted   */
		  was_quoted = (was_quoted) ? 0 : 1 ;

		if((*p1 == ',' && !was_quoted) || *p1 == '\n' || *p1 == '\0')
		  line_count++;		/* count this element */

		if(*p1 == '\0' || *p1 == '\n'){	/* deal with EOL */
		    if(p1 < p || *p1 == '\n'){
			*p1++ = ','; 	/* fix null or newline */

			if(*p1 != '\t' && *p1 != ' '){
			    *(p1-1) = '\0'; /* tie off list */
			    p       = p1;   /* reset p */
			    break;
			}
		    }else{
			p = p1;		/* end of pinerc */
			break;
		    }
		}else
		  p1++;
	    }

	    error  = NULL;
	    lvalue = parse_list(value, line_count,
				v->del_quotes ? PL_REMSURRQUOT : PL_NONE,
			        &error);
	    if(error){
		dprint(1, (debugfile,
		       "read_pinerc: ERROR: %s in %s = \"%s\"\n", 
			   error ? error : "?",
			   v->name ? v->name : "?",
			   value ? value : "?"));
	    }
	    /*
	     * Special case: turn "" strings into empty strings.
	     * This allows users to turn off default lists.  For example,
	     * if smtp-server is set then a user could override smtp-server
	     * with smtp-server="".
	     */
	    for(i = 0; lvalue[i]; i++)
		if(lvalue[i][0] == '"' &&
		   lvalue[i][1] == '"' &&
		   lvalue[i][2] == '\0')
		     lvalue[i][0] = '\0';
	}

        if(pline){
            if(v->is_user && (which_vars == ParsePers || !v->is_onlymain)){
		if(v->is_list){
		    char ***l;

		    l = (which_vars == ParsePers) ? &v->main_user_val.l
						  : &v->post_user_val.l;
		    free_list_array(l);
		    *l = lvalue;
		}
		else{
		    char **p;

		    p = (which_vars == ParsePers) ? &v->main_user_val.p
						  : &v->post_user_val.p;
		    if(p && *p != NULL)
		      fs_give((void **)p);

		    *p = cpystr(value);
		}

		if(pline){
		    pline->is_var    = 1;
		    pline->var       = v;
		    pline->is_quoted = was_quoted;
		    pline++;
		}
            }
        }
	else if(which_vars == ParseGlobal){
            if(v->is_global){
		if(v->is_list){
		    free_list_array(&v->global_val.l);
		    v->global_val.l = lvalue;
		}
		else{
		    if(v->global_val.p != NULL)
		      fs_give((void **) &(v->global_val.p));

		    v->global_val.p = cpystr(value);
		}
            }
        }
	else{  /* which_vars == ParseFixed */
            if(v->is_user || v->is_global){
		if(v->is_list){
		    free_list_array(&v->fixed_val.l);
		    v->fixed_val.l = lvalue;
		}
		else{
		    if(v->fixed_val.p != NULL)
		      fs_give((void **) &(v->fixed_val.p));

		    v->fixed_val.p = cpystr(value);
		}
	    }
	}

#ifdef DEBUG
	if(v->is_list){
	    char **l;
	    l =   (which_vars == ParsePers)     ? v->main_user_val.l :
	           (which_vars == ParsePersPost) ? v->post_user_val.l :
	            (which_vars == ParseGlobal)   ? v->global_val.l :
						     v->fixed_val.l;
	    if(l && *l && **l){
                dprint(5,(debugfile, " %20.20s : %s\n",
		       v->name ? v->name : "?",
		       *l ? *l : "?"));
	        while(++l && *l && **l)
                    dprint(5,(debugfile, " %20.20s : %s\n", "",
			   *l ? *l : "?"));
	    }
	}else{
	    char *p;
	    p =   (which_vars == ParsePers)     ? v->main_user_val.p :
	           (which_vars == ParsePersPost) ? v->post_user_val.p :
	            (which_vars == ParseGlobal)   ? v->global_val.p :
						     v->fixed_val.p;
	    if(p && *p)
                dprint(5,(debugfile, " %20.20s : %s\n",
		       v->name ? v->name : "?",
		       p ? p : "?"));
	}
#endif /* DEBUG */
    }

    if(pline){
        pline->line = NULL;
        pline->is_var = 0;
	if(!prc->pinerc_written && prc->type == Loc){
	    prc->pinerc_written = name_file_mtime(filename);
	    dprint(5, (debugfile, "read_pinerc: time_pinerc_written = %ld\n",
		       (long) prc->pinerc_written));
	}
    }

    fs_give((void **)&file);
}


/*
 * Args   varname   The variable name we're looking for
 *        begin     Begin looking here
 *
 * Returns 1   if variable varname appears in the rest of the file
 *         0   if not
 */
int
var_is_in_rest_of_file(varname, begin)
    char *varname;
    char *begin;
{
    char *p;

    if(!(varname && *varname && begin && *begin))
      return 0;

    p = begin;

    while(p = srchstr(p, varname)){
	/* beginning of a line? */
	if(p > begin && (*(p-1) != '\n' && *(p-1) != '\r')){
	    p++;
	    continue;
	}

	/* followed by [ SPACE ] < = > ? */
	p += strlen(varname);
	while(*p == ' ' || *p == '\t')
	  p++;
	
	if(*p == '=')
	  return 1;
    }
    
    return 0;
}


/*
 * Args   begin    Variable to skip starts here.
 *        nextline This is where the next line starts. We need to know this
 *                 because the input has been mangled a little. A \0 has
 *                 replaced the \n at the end of the first line, but we can
 *                 use nextline to help us out of that quandry.
 *
 * Return a pointer to the start of the first line after this variable
 *        and all of its continuation lines.
 */
char *
skip_over_this_var(begin, nextline)
    char *begin;
    char *nextline;
{
    char *p;

    p = begin;

    while(1){
	if(*p == '\0' || *p == '\n'){		/* EOL */
	    if(p < nextline || *p == '\n'){	/* there may be another line */
		p++;
		if(*p != ' ' && *p != '\t')	/* no continuation line */
		  return(p);
	    }
	    else				/* end of file */
	      return(p);
	}
	else
	  p++;
    }
}


char *
read_remote_pinerc(prc, which_vars)
    PINERC_S *prc;
    ParsePinerc which_vars;
{
    int        try_cache, no_perm_create_pass = 0;
    char      *file = NULL;
    unsigned   flags;


    dprint(7, (debugfile, "read_remote_pinerc \"%s\"\n",
	   prc->name ? prc->name : "?"));

    if(ps_global->cache_remote_pinerc){
	/*
	 * We derive the name of the cache file from the remote name. We do
	 * that because we don't have the metadata filename yet, and that's
	 * the place that tells us the name of the cache file. So we make up
	 * a name algorithmically instead. This could be dangerous because
	 * two different users using the same PC could both have the same
	 * name for a remote pinerc (because the name is relative to their
	 * user context on the remote system). If we're going to do this,
	 * we'll probably need to put some magic in a first-line comment at
	 * the start of the remote pinerc, and then verify that it is
	 * our pinerc.
	 *
	 * Unfortunately, caching doesn't help much because we don't have a
	 * stream already open for a fast check. So we always have to go open
	 * a stream in order to tell if our cached copy is up-to-date.
	 */
	flags = DERIVE_CACHE;
    }
    else{
	/*
	 * We don't cache the pinerc, we always copy it.
	 *
	 * Don't store the config in a temporary file, just leave it
	 * in memory while using it.
	 * It is currently required that NO_PERM_CACHE be set if NO_FILE is set.
	 */
	flags = (NO_PERM_CACHE | NO_FILE);
    }

create_the_remote_folder:

    if(no_perm_create_pass){
	if(prc->rd){
	    prc->rd->flags &= ~DO_REMTRIM;
	    rd_close_remdata(&prc->rd);
	}

	/* this will cause the remote folder to be created */
	flags = 0;
    }

    /*
     * We could parse the name here to find what type it is. So far we
     * only have type RemImap.
     */
    prc->rd = rd_create_remote(RemImap, prc->name,
			       (void *)REMOTE_PINERC_SUBTYPE,
			       &flags, "Error: ",
			       " Can't fetch remote configuration.");
    if(!prc->rd)
      goto bail_out;
    
    /*
     * On first use we just use a temp file instead of memory (NO_FILE).
     * In other words, for our convenience, we don't turn NO_FILE back on
     * here. Why is that convenient? Because of the stuff that happened in
     * rd_create_remote when flags was set to zero.
     */
    if(no_perm_create_pass)
      prc->rd->flags |= NO_PERM_CACHE;

    try_cache = rd_read_metadata(prc->rd);

    if(prc->rd->access == MaybeRorW){
	if(prc->rd->read_status == 'R' ||
           !(which_vars == ParsePers || which_vars == ParsePersPost)){
	    prc->rd->access = ReadOnly;
	    prc->rd->read_status = 'R';
	}
	else
	  prc->rd->access = ReadWrite;
    }

    if(prc->rd->access != NoExists){

	rd_check_remvalid(prc->rd, 1L);

	/*
	 * If the cached info says it is readonly but
	 * it looks like it's been fixed now, change it to readwrite.
	 */
        if((which_vars == ParsePers || which_vars == ParsePersPost) &&
	   prc->rd->read_status == 'R'){
	    /*
	     * We go to this trouble since readonly pinercs
	     * are likely a mistake. They are usually supposed to be
	     * readwrite so we open it and check if it's been fixed.
	     */
	    rd_check_readonly_access(prc->rd);
	    if(prc->rd->read_status == 'W'){
		prc->rd->access = ReadWrite;
		prc->rd->flags |= REM_OUTOFDATE;
	    }
	    else
	      prc->rd->access = ReadOnly;
	}

	if(prc->rd->flags & REM_OUTOFDATE){
	    if(rd_update_local(prc->rd) != 0){
		if(!no_perm_create_pass && prc->rd->flags & NO_PERM_CACHE
		   && !(prc->rd->flags & USER_SAID_NO)){
		    /*
		     * We don't check for the existence of the remote
		     * folder when this flag is turned on, so we could
		     * fail here because the remote folder doesn't exist.
		     * We try to create it.
		     */
		    no_perm_create_pass++;
		    goto create_the_remote_folder;
		}

		dprint(1, (debugfile,
		       "read_pinerc_remote: rd_update_local failed\n"));
		/*
		 * Don't give up altogether. We still may be
		 * able to use a cached copy.
		 */
	    }
	    else{
		dprint(7, (debugfile,
		       "%s: copied remote to local (%ld)\n",
		       prc->rd->rn ? prc->rd->rn : "?",
		       (long)prc->rd->last_use));
	    }
	}

	if(prc->rd->access == ReadWrite)
	  prc->rd->flags |= DO_REMTRIM;
    }

    /* If we couldn't get to remote folder, try using the cached copy */
    if(prc->rd->access == NoExists || prc->rd->flags & REM_OUTOFDATE){
	if(try_cache){
	    prc->rd->access = ReadOnly;
	    prc->rd->flags |= USE_OLD_CACHE;
	    q_status_message(SM_ORDER, 3, 4,
	     "Can't contact remote config server, using cached copy");
	    dprint(2, (debugfile,
    "Can't open remote pinerc %s, using local cached copy %s readonly\n",
		   prc->rd->rn ? prc->rd->rn : "?",
		   prc->rd->lf ? prc->rd->lf : "?"));
	}
	else{
	    prc->rd->flags &= ~DO_REMTRIM;
	    goto bail_out;
	}
    }

    if(prc->rd->flags & NO_FILE)
      /* copy text, leave sonofile for later use */
      file = cpystr((char *)so_text(prc->rd->sonofile));
    else
      file = read_file(prc->rd->lf);

bail_out:
    if((which_vars == ParsePers || which_vars == ParsePersPost) &&
       (!file || !prc->rd || prc->rd->access != ReadWrite)){
	prc->readonly = 1;
	if(prc == ps_global->prc)
	  ps_global->readonly_pinerc = 1;
    }
    
    return(file);
}


/*
 * parse_list - takes a comma delimited list of "count" elements and 
 *              returns an array of pointers to each element neatly
 *              malloc'd in its own array.  Any errors are returned
 *              in the string pointed to by "error"
 *
 *	If remove_surrounding_double_quotes is set, then double quotes around
 *	each element of the list are removed. We can't do this for all list
 *	variables. For example, incoming folders look like
 *		nickname foldername
 *	in the config file. Each of those may be quoted separately.
 *
 *  NOTE: only recognizes escaped quotes
 */
char **
parse_list(list, count, flags, error)
    char *list, **error;
    int   count, flags;
{
    char **lvalue, *p2, *p3, *p4;
    int    was_quoted = 0;
    int    remove_surrounding_double_quotes;
    int    commas_may_be_escaped;

    remove_surrounding_double_quotes = (flags & PL_REMSURRQUOT);
    commas_may_be_escaped = (flags & PL_COMMAQUOTE);

    lvalue = (char **) fs_get((count+1) * sizeof(char *));
    count  = 0;
    while(*list){			/* pick elements from list */
	p2 = list;		/* find end of element */
	while(1){
	    if(*p2 == '"')	/* ignore ',' if quoted   */
	      was_quoted = (was_quoted) ? 0 : 1 ;

	    if(*p2 == '\\' && *(p2+1) == '"')
	      p2++;		/* preserve escaped quotes, too */

	    if((*p2 == ',' && !was_quoted) || *p2 == '\0')
	      break;

	    if(commas_may_be_escaped && *p2 == '\\' && *(p2+1) == ',')
	      p2++;

	    p2++;
	}

	if(was_quoted){		/* unbalanced quotes! */
	    if(error)
	      *error = "Unbalanced quotes";

	    break;
	}

	/*
	 * if element found, eliminate trailing 
	 * white space and tie into variable list
	 */
	if(p2 != list){
	    for(p3 = p2 - 1; isspace((unsigned char) *p3) && list < p3; p3--)
	      ;

	    p4 = fs_get(((p3 - list) + 2) * sizeof(char));
	    lvalue[count] = p4;
	    while(list <= p3)
	      *p4++ = *list++;

	    *p4 = '\0';

	    if(remove_surrounding_double_quotes)
	      removing_double_quotes(lvalue[count]);
	    
	    count++;
	}

	if(*(list = p2) != '\0'){	/* move to beginning of next val */
	    while(*list == ',' || isspace((unsigned char)*list))
	      list++;
	}
    }

    lvalue[count] = NULL;		/* tie off pointer list */
    return(lvalue);
}



/*
 * Free array of string pointers and associated strings
 *
 * Args: list -- array of char *'s
 */
void
free_list_array(list)
    char ***list;
{
    char **p;

    if(list && *list){
	for(p = *list; *p; p++)
	  fs_give((void **) p);

	fs_give((void **) list);
    }
}


/*
 * Copy array of string pointers and associated strings
 *
 * Args: list -- array of char *'s
 *
 * Returns: Allocated array of string pointers and allocated copies of strings.
 *          Caller should free the list array when done.
 */
char **
copy_list_array(list)
    char **list;
{
    int    i, cnt = 0;
    char **p, **ret_list = NULL;

    if(list){
	while(list[cnt++])
	  ;

	p = ret_list = (char **)fs_get((cnt+1) * sizeof(char *));
	memset((void *) ret_list, 0, (cnt+1) * sizeof(char *));

	for(i=0; list[i]; i++, p++)
	  *p = cpystr(list[i]);
	
	*p = NULL;

    }

    return(ret_list);
}


int
equal_list_arrays(list1, list2)
    char **list1, **list2;
{
    int ret = 0;

    if(list1 && list2){
	while(*list1){
	    if(!*list2 || strcmp(*list1, *list2) != 0)
	      break;
	    
	    list1++;
	    list2++;
	}

	if(*list1 == NULL && *list2 == NULL)
	  ret++;
    }

    return(ret);
}


static char quotes[3] = {'"', '"', '\0'};
/*----------------------------------------------------------------------
    Write out the .pinerc state information

   Args: ps -- The pine structure to take state to be written from
      which -- Which pinerc to write
      flags -- If bit WRP_NOUSER is set, then assume that there is
                not a user present to answer questions.

  This writes to a temporary file first, and then renames that to 
 be the new .pinerc file to protect against disk error.  This has the 
 problem of possibly messing up file protections, ownership and links.
  ----*/
write_pinerc(ps, which, flags)
    struct pine *ps;
    EditWhich    which;
    int          flags;
{
    char               *p, *dir, *tmp = NULL, *pinrc;
    char               *pval, **lval;
    FILE               *f;
    PINERC_LINE        *pline;
    struct variable    *var;
    time_t		mtime;
    char               *filename;
    REMDATA_S          *rd = NULL;
    PINERC_S           *prc = NULL;
    STORE_S            *so = NULL;

    dprint(2,(debugfile,"---- write_pinerc(%s) ----\n",
	    (which == Main) ? "Main" : "Post"));

    switch(which){
      case Main:
	prc = ps ? ps->prc : NULL;
	break;
      case Post:
	prc = ps ? ps->post_prc : NULL;
	break;
    }

    if(!prc)
      return(-1);
    
    if(prc->quit_to_edit){
	if(!(flags & WRP_NOUSER))
	  quit_to_edit_msg(prc);

	return(-1);
    }

    if(prc->type != Loc && !prc->readonly){

	rd = prc->rd;
	if(!rd)
	  return(-1);
	
	rd_check_remvalid(rd, -10L);

	if(rd->flags & REM_OUTOFDATE){
	    Writechar(BELL, 0);
	    if((flags & WRP_NOUSER) ||
	      want_to("Unexpected pinerc change! Overwrite with current config",
		       'n', 0, NO_HELP, WT_FLUSH_IN) == 'n'){
		prc->outstanding_pinerc_changes = 1;
		if(!(flags & WRP_NOUSER))
		  q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    "Pinerc \"%.200s\" NOT saved",
				    prc->name ? prc->name : "");
		dprint(2, (debugfile,
			   "write_pinerc: remote pinerc changed\n"));
		return(-1);
	    }
	    else
	      rd->flags &= ~REM_OUTOFDATE;
	}

	rd_open_remote(rd);

	if(rd->access == ReadWrite){
	    int ro;

	    if((ro=rd_remote_is_readonly(rd)) || rd->flags & REM_OUTOFDATE){
		if(ro == 1){
		    if(!(flags & WRP_NOUSER))
		      q_status_message(SM_ORDER | SM_DING, 5, 15,
			     "Can't access remote config, changes NOT saved!");
		    dprint(1, (debugfile,
	    "write_pinerc: Can't write to remote pinerc %s, aborting write\n",
			   rd->rn ? rd->rn : "?"));
		}
		else if(ro == 2){
		    if(!(rd->flags & NO_META_UPDATE)){
			unsigned long save_chk_nmsgs;

			switch(rd->type){
			  case RemImap:
			    save_chk_nmsgs = rd->t.i.chk_nmsgs;
			    rd->t.i.chk_nmsgs = 0;
			    rd_write_metadata(rd, 0);
			    rd->t.i.chk_nmsgs = save_chk_nmsgs;
			    break;

			  default:
			    q_status_message(SM_ORDER | SM_DING, 3, 5,
					 "Write_pinerc: Type not supported");
			    break;
			}
		    }

		    if(!(flags & WRP_NOUSER))
		      q_status_message1(SM_ORDER | SM_DING, 5, 15,
	    "No write permission for remote config %.200s, changes NOT saved!",
				    rd->rn);
		}
		else{
		    if(!(flags & WRP_NOUSER))
		      q_status_message(SM_ORDER | SM_DING, 5, 15,
		 "Remote config changed, aborting our change to avoid damage...");
		    dprint(1, (debugfile,
			    "write_pinerc: remote config %s changed since we started pine, aborting write\n",
			    prc->name ? prc->name : "?"));
		}

		rd->flags &= ~DO_REMTRIM;
		return(-1);
	    }

	    filename = rd->lf;
	}
	else{
	    prc->readonly = 1;
	    if(prc == ps->prc)
	      ps->readonly_pinerc = 1;
	}
    }
    else
      filename = prc->name ? prc->name : "";

    pinrc = prc->name ? prc->name : "";

    if(prc->type == Loc){
	mtime = name_file_mtime(filename);
	if(prc->pinerc_written && prc->pinerc_written != mtime){
	    Writechar(BELL, 0);
	    if((flags & WRP_NOUSER) ||
	     want_to("Unexpected pinerc change!  Overwrite with current config",
		       'n', 0, NO_HELP, WT_FLUSH_IN) == 'n'){
		prc->outstanding_pinerc_changes = 1;
		if(!(flags & WRP_NOUSER))
		  q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    "Pinerc \"%.200s\" NOT saved", pinrc);
		dprint(2, (debugfile,
			   "write_pinerc: mtime mismatch: \"%s\": %ld != %ld\n",
			   filename ? filename : "?",
			   (long) prc->pinerc_written, (long) mtime));
		return(-1);
	    }
	}
    }

    /* don't write if pinerc is read-only */
    if(prc->readonly ||
         (filename &&
	  can_access(filename, ACCESS_EXISTS) == 0 &&
          can_access(filename, EDIT_ACCESS) != 0)){
	prc->readonly = 1;
	if(prc == ps->prc)
	  ps->readonly_pinerc = 1;

	if(!(flags & WRP_NOUSER))
	  q_status_message1(SM_ORDER | SM_DING, 0, 5,
		      "Can't modify configuration file \"%.200s\": ReadOnly",
			    pinrc);
	dprint(2,
	    (debugfile,"write_pinerc: fail because can't access pinerc\n"));

	if(rd)
	  rd->flags &= ~DO_REMTRIM;

	return(-1);
    }

    if(rd && rd->flags & NO_FILE){
	so = rd->sonofile;
	so_truncate(rd->sonofile, 0L);		/* reset storage object */
    }
    else{
	dir = ".";
	if(p = last_cmpnt(filename)){
	    *--p = '\0';
	    dir = filename;
	}

#if	defined(DOS) || defined(OS2)
	if(!(isalpha((unsigned char)dir[0]) && dir[1] == ':' && dir[2] == '\0')
	   && (can_access(dir, EDIT_ACCESS) < 0 &&
#ifdef	DOS
	       mkdir(dir) < 0))
#else
	       mkdir(dir,0700) < 0))
#endif
	{
	    if(!(flags & WRP_NOUSER))
	      q_status_message2(SM_ORDER | SM_DING, 3, 5,
			      "Error creating \"%.200s\" : %.200s", dir,
			      error_description(errno));
	    if(rd)
	      rd->flags &= ~DO_REMTRIM;

	    return(-1);
	}

	tmp = temp_nam(dir, "rc");

	if(*dir && tmp && !in_dir(dir, tmp)){
	    (void)unlink(tmp);
	    fs_give((void **)&tmp);
	}

	if(p)
	  *p = '\\';

	if(tmp == NULL)
	  goto io_err;

#else  /* !DOS */
	tmp = temp_nam((*dir) ? dir : "/", "pinerc");

	/*
	 * If temp_nam can't write in dir it puts the temp file in a
	 * temp directory, which won't help us when we go to rename.
	 */
	if(*dir && tmp && !in_dir(dir, tmp)){
	    (void)unlink(tmp);
	    fs_give((void **)&tmp);
	}

	if(p)
	  *p = '/';

	if(tmp == NULL)
	  goto io_err;

#endif  /* !DOS */

	if((so = so_get(FileStar, tmp, WRITE_ACCESS)) == NULL)
	  goto io_err;
    }

    for(var = ps->vars; var->name != NULL; var++) 
      var->been_written = 0;

    if(prc->type == Loc && ps->first_time_user &&
       !so_puts(so, native_nl(cf_text_comment)))
      goto io_err;

    /* Write out what was in the .pinerc */
    for(pline = prc->pinerc_lines;
	pline && (pline->is_var || pline->line); pline++){
	if(pline->is_var){
	    var = pline->var;

	    if(var->is_list)
	      lval = LVAL(var, which);
	    else
	      pval = PVAL(var, which);

	    /* variable is not set */
	    if((var->is_list && (!lval || !lval[0])) ||
	       (!var->is_list && !pval)){
		/* leave null variables out of remote pinerc */
		if(prc->type == Loc &&
		   (!so_puts(so, var->name) || !so_puts(so, "=") ||
		    !so_puts(so, NEWLINE)))
		  goto io_err;
	    }
	    /* var is set to empty string */
	    else if((var->is_list && lval[0][0] == '\0') ||
		    (!var->is_list && pval[0] == '\0')){
		if(!so_puts(so, var->name) || !so_puts(so, "=") ||
		   !so_puts(so, quotes) || !so_puts(so, NEWLINE))
		  goto io_err;
	    }
	    else{
		if(var->is_list){
		    int i = 0;

		    for(i = 0; lval[i]; i++){
			sprintf(tmp_20k_buf, "%.100s%.100s%.10000s%.100s%s",
				(i) ? "\t" : var->name,
				(i) ? "" : "=",
				lval[i][0] ? lval[i] : quotes,
				lval[i+1] ? "," : "", NEWLINE);
			if(!so_puts(so, (char *)tmp_20k_buf))
			  goto io_err;
		    }
		}
		else{
		    sprintf(tmp_20k_buf, "%.100s=%.100s%.10000s%.100s%s",
			    var->name,
			    (pline->is_quoted && pval[0] != '\"')
			      ? "\"" : "",
			    pval,
			    (pline->is_quoted && pval[0] != '\"')
			      ? "\"" : "", NEWLINE);
		    if(!so_puts(so, (char *)tmp_20k_buf))
		      goto io_err;
		}
	    }

	    var->been_written = 1;

	}else{
	    /*
	     * The description text should be changed into a message
	     * about the variable being obsolete when a variable is
	     * moved to obsolete status.  We add that message before
	     * the variable unless it is already there.  However, we
	     * leave the variable itself in case the user runs an old
	     * version of pine again.  Note that we have read in the
	     * value of the variable in read_pinerc and translated it
	     * into a new variable if appropriate.
	     */
	    if(pline->obsolete_var && prc->type == Loc){
		if(pline <= prc->pinerc_lines || (pline-1)->line == NULL ||
		   strlen((pline-1)->line) < 3 ||
		   strucmp((pline-1)->line+2, pline->var->descrip) != 0)
		  if(!so_puts(so, "# ") ||
		     !so_puts(so, native_nl(pline->var->descrip)) ||
		     !so_puts(so, NEWLINE))
		    goto io_err;
	    }

	    /* remove comments from remote pinercs */
	    if((prc->type == Loc ||
		(pline->line[0] != '#' && pline->line[0] != '\0')) &&
	        (!so_puts(so, pline->line) || !so_puts(so, NEWLINE)))
	      goto io_err;
	}
    }

    /* Now write out all the variables not in the .pinerc */
    for(var = ps->vars; var->name != NULL; var++){
	if(!var->is_user || var->been_written || !var->is_used ||
	   var->is_obsolete || (var->is_onlymain && which != Main))
	  continue;

	if(var->is_list)
	  lval = LVAL(var, which);
	else
	  pval = PVAL(var, which);

	/*
	 * set description to NULL to eliminate preceding
	 * blank and comment line.
	 */
	if(prc->type == Loc && var->descrip && *var->descrip &&
	   (!so_puts(so, NEWLINE) || !so_puts(so, "# ") ||
	    !so_puts(so, native_nl(var->descrip)) || !so_puts(so, NEWLINE)))
	  goto io_err;

	/* variable is not set */
	/** Don't know what the global_val thing is for. SH, Mar 00 **/
	if((var->is_list && (!lval || (!lval[0] && !var->global_val.l))) ||
	   (!var->is_list && !pval)){
	    /* leave null variables out of remote pinerc */
	    if(prc->type == Loc &&
	       (!so_puts(so, var->name) || !so_puts(so, "=") ||
	        !so_puts(so, NEWLINE)))
	      goto io_err;
	}
	/* var is set to empty string */
	else if((var->is_list && (!lval[0] || !lval[0][0]))
		|| (!var->is_list && pval[0] == '\0')){
	    if(!so_puts(so, var->name) || !so_puts(so, "=") ||
	       !so_puts(so, quotes) || !so_puts(so, NEWLINE))
	      goto io_err;
	}
	else if(var->is_list){
	    int i = 0;

	    for(i = 0; lval[i] ; i++){
		sprintf(tmp_20k_buf, "%.100s%.100s%.10000s%.100s%s",
			(i) ? "\t" : var->name,
			(i) ? "" : "=",
			lval[i],
			lval[i+1] ? "," : "", NEWLINE);
		if(!so_puts(so, (char *)tmp_20k_buf))
		  goto io_err;
	    }
	}
	else{
	    if(!so_puts(so, var->name) || !so_puts(so, "=") ||
	       !so_puts(so, pval) || !so_puts(so, NEWLINE))
	      goto io_err;
	}
    }

    if(!(rd && rd->flags & NO_FILE)){
	if(so_give(&so))
	  goto io_err;

	file_attrib_copy(tmp, filename);
	if(rename_file(tmp, filename) < 0)
	  goto io_err;
    }
    
    if(prc->type != Loc){
	int   e, we_cancel;
	char datebuf[200];

	datebuf[0] = '\0';

	if(!(flags & WRP_NOUSER))
	  we_cancel = busy_alarm(1, "Copying to remote config", NULL, 0);

	if((e = rd_update_remote(rd, datebuf)) != 0){
	    dprint(1, (debugfile,
		   "write_pinerc: error copying from %s to %s\n",
		   rd->lf ? rd->lf : "<memory>", rd->rn ? rd->rn : "?"));
	    if(!(flags & WRP_NOUSER)){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				"Error copying to %.200s: %.200s",
				rd->rn, error_description(errno));
		
		q_status_message(SM_ORDER | SM_DING, 5, 5,
       "Copy of config to remote folder failed, changes NOT saved remotely");
	    }
	}
	else{
	    rd_update_metadata(rd, datebuf);
	    rd->read_status = 'W';
	    rd_trim_remdata(&rd);
	    rd_close_remote(rd);
	}

	if(we_cancel)
	  cancel_busy_alarm(-1);
    }

    prc->outstanding_pinerc_changes = 0;

    if(prc->type == Loc){
	prc->pinerc_written = name_file_mtime(filename);
	dprint(2, (debugfile, "wrote pinerc: %s: time_pinerc_written = %ld\n",
		   pinrc ? pinrc : "?", (long) prc->pinerc_written));
    }
    else{
	dprint(2, (debugfile, "wrote pinerc: %s\n", pinrc ? pinrc : "?"));
    }

    if(tmp){
	(void)unlink(tmp);
	fs_give((void **)&tmp);
    }

    return(0);

  io_err:
    if(!(flags & WRP_NOUSER))
      q_status_message2(SM_ORDER | SM_DING, 3, 5,
		        "Error saving configuration in \"%.200s\": %.200s",
		        pinrc, error_description(errno));

    dprint(1, (debugfile, "Error writing %s : %s\n", pinrc ? pinrc : "?",
	       error_description(errno)));
    if(rd)
      rd->flags &= ~DO_REMTRIM;
    if(tmp){
	(void)unlink(tmp);
	fs_give((void **)&tmp);
    }

    return(-1);
}


/*
 * Given a unix-style source string which may contain LFs,
 * convert those to CRLFs if appropriate.
 *
 * Returns a pointer to the converted string. This will be a string
 * stored in tmp_20k_buf.
 *
 * This is just used for the variable descriptions in the pinerc file. It
 * could certainly be fancier. It simply converts all \n to NEWLINE.
 */
char *
native_nl(src)
    char *src;
{ 
    char *q, *p;

    tmp_20k_buf[0] = '\0';

    if(src){
	for(q = (char *)tmp_20k_buf; *src; src++){
	    if(*src == '\n'){
		for(p = NEWLINE; *p; p++)
		  *q++ = *p;
	    }
	    else
	      *q++ = *src;
	}

	*q = '\0';
    }

    return((char *)tmp_20k_buf);
}


void
quit_to_edit_msg(prc)
    PINERC_S *prc;
{
    q_status_message1(SM_ORDER, 3, 4, "Must quit Pine to change %sconfig file.",
		      (prc == ps_global->post_prc) ? "Postload " : "");
}


/*------------------------------------------------------------
  Return TRUE if the given string was a feature name present in the
  pinerc as it was when pine was started...
  ----*/
var_in_pinerc(s)
char *s;
{
    PINERC_LINE *pline;

    for(pline = ps_global->prc ? ps_global->prc->pinerc_lines : NULL;
	pline && (pline->var || pline->line); pline++)
      if(pline->var && pline->var->name && !strucmp(s, pline->var->name))
	return(1);

    for(pline = ps_global->post_prc ? ps_global->post_prc->pinerc_lines : NULL;
	pline && (pline->var || pline->line); pline++)
      if(pline->var && pline->var->name && !strucmp(s, pline->var->name))
	return(1);

    return(0);
}



/*------------------------------------------------------------
  Free resources associated with pinerc_lines data
 ----*/
void
free_pinerc_lines(pinerc_lines)
    PINERC_LINE **pinerc_lines;
{
    PINERC_LINE *pline;

    if(pinerc_lines && *pinerc_lines){
	for(pline = *pinerc_lines; pline->var || pline->line; pline++)
	  if(pline->line)
	    fs_give((void **)&pline->line);

	fs_give((void **)pinerc_lines);
    }
}



/*
 * This is only used at startup time.  It sets ps->def_sort and
 * ps->def_sort_rev.  The syntax of the sort_spec is type[/reverse].
 * A reverse without a type is the same as arrival/reverse.  A blank
 * argument also means arrival/reverse.
 */
int
decode_sort(sort_spec, def_sort, def_sort_rev)
     char        *sort_spec;
     SortOrder   *def_sort;
     int         *def_sort_rev;
{
    char *sep;
    char *fix_this = NULL;
    int   x, reverse;

    if(!sort_spec || !*sort_spec){
	*def_sort = SortArrival;
	*def_sort_rev = 0;
        return(0);
    }

    if(struncmp(sort_spec, "reverse", strlen(sort_spec)) == 0){
	*def_sort = SortArrival;
	*def_sort_rev = 1;
        return(0);
    }
     
    reverse = 0;
    if((sep = strindex(sort_spec, '/')) != NULL){
        *sep = '\0';
	fix_this = sep;
        sep++;
        if(struncmp(sep, "reverse", strlen(sep)) == 0)
          reverse = 1;
        else{
	    *fix_this = '/';
	    return(-1);
	}
    }

    for(x = 0; ps_global->sort_types[x] != EndofList; x++)
      if(struncmp(sort_name(ps_global->sort_types[x]),
                  sort_spec, strlen(sort_spec)) == 0)
        break;

    if(fix_this)
      *fix_this = '/';

    if(ps_global->sort_types[x] == EndofList)
      return(-1);

    *def_sort     = ps_global->sort_types[x];
    *def_sort_rev = reverse;
    return(0);
}


/*------------------------------------------------------------
    Dump out a global pine.conf on the standard output with fresh
    comments.  Preserves variables currently set in SYSTEM_PINERC, if any.
  ----*/
void
dump_global_conf()
{
     FILE            *f;
     struct variable *var;
     PINERC_S        *prc;

     prc = new_pinerc_s(SYSTEM_PINERC);
     read_pinerc(prc, variables, ParseGlobal);
     if(prc)
       free_pinerc_s(&prc);

     f = stdout;
     if(f == NULL) 
       goto io_err;

     fprintf(f, "#      %s -- system wide pine configuration\n#\n",
	     SYSTEM_PINERC);
     fprintf(f, "# Values here affect all pine users unless they've overridden the values\n");
     fprintf(f, "# in their .pinerc files.  A copy of this file with current comments may\n");
     fprintf(f, "# be obtained by running \"pine -conf\". It will be printed to standard output.\n#\n");
     fprintf(f,"# For a variable to be unset its value must be null/blank.  This is not the\n");
     fprintf(f,"# same as the value of \"empty string\", which can be used to effectively\n");
     fprintf(f,"# \"unset\" a variable that has a default or previously assigned value.\n");
     fprintf(f,"# To set a variable to the empty string its value should be \"\".\n");
     fprintf(f,"# Switch variables are set to either \"yes\" or \"no\", and default to \"no\".\n");
     fprintf(f,"# Except for feature-list items, which are additive, values set in the\n");
     fprintf(f,"# .pinerc file replace those in pine.conf, and those in pine.conf.fixed\n");
     fprintf(f,"# over-ride all others.  Features can be over-ridden in .pinerc or\n");
     fprintf(f,"# pine.conf.fixed by pre-pending the feature name with \"no-\".\n#\n");
     fprintf(f,"# (These comments are automatically inserted.)\n");

     for(var = variables; var->name != NULL; var++){
         if(!var->is_global || !var->is_used || var->is_obsolete)
           continue;

         if(var->descrip && *var->descrip){
           if(fprintf(f, "\n# %s\n", var->descrip) == EOF)
             goto io_err;
	 }

	 if(var->is_list){
	     if(var->global_val.l == NULL){
		 if(fprintf(f, "%s=\n", var->name) == EOF)
		   goto io_err;
	     }else{
		 int i;

		 for(i=0; var->global_val.l[i]; i++)
		   if(fprintf(f, "%s%s%s%s\n", (i) ? "\t" : var->name,
			      (i) ? "" : "=", var->global_val.l[i],
			      var->global_val.l[i+1] ? ",":"") == EOF)
		     goto io_err;
	     }
	 }else{
	     if(var->global_val.p == NULL){
		 if(fprintf(f, "%s=\n", var->name) == EOF)
		   goto io_err;
	     }else if(strlen(var->global_val.p) == 0){
		 if(fprintf(f, "%s=\"\"\n", var->name) == EOF)
               goto io_err;
	     }else{
		 if(fprintf(f,"%s=%s\n",var->name,var->global_val.p) == EOF)
		   goto io_err;
	     }
	 }
     }
     exit(0);


   io_err:
     fprintf(stderr, "Error writing config to stdout: %s\n",
             error_description(errno));
     exit(-1);
}


/*------------------------------------------------------------
    Dump out a pinerc to filename with fresh
    comments.  Preserves variables currently set in pinerc, if any.
  ----*/
void
dump_new_pinerc(filename)
char *filename;
{
    FILE            *f;
    struct variable *var;
    char             buf[MAXPATH], *p;
    PINERC_S        *prc;


    p = ps_global->pinerc;

#if defined(DOS) || defined(OS2)
    if(!ps_global->pinerc){
	char *p;
	int   l;

	if(p = getenv("PINERC")){
	    ps_global->pinerc = cpystr(p);
	}else{
	    char buf2[MAXPATH];
	    build_path(buf2, ps_global->home_dir, DF_PINEDIR, sizeof(buf2));
	    build_path(buf, buf2, SYSTEM_PINERC, sizeof(buf));
	}

	p = buf;
    }
#else	/* !DOS */
    if(!ps_global->pinerc){
	build_path(buf, ps_global->home_dir, ".pinerc", sizeof(buf));
	p = buf;
    }
#endif	/* !DOS */

    prc = new_pinerc_s(p);
    read_pinerc(prc, variables, ParsePers);
    if(prc)
      free_pinerc_s(&prc);

    f = NULL;;
    if(filename[0] == '\0'){
	fprintf(stderr, "Missing argument to \"-pinerc\".\n");
    }else if(!strcmp(filename, "-")){
	f = stdout;
    }else{
	f = fopen(filename, "w");
    }

    if(f == NULL) 
	goto io_err;

    if(fprintf(f, "%s", cf_text_comment) == EOF)
	goto io_err;

    for(var = variables; var->name != NULL; var++){
	dprint(7,(debugfile,"write_pinerc: %s = %s\n",
	       var->name ? var->name : "?",
	       var->main_user_val.p ? var->main_user_val.p : "<not set>"));
        if(!var->is_user || !var->is_used || var->is_obsolete)
	    continue;

	/*
	 * set description to NULL to eliminate preceding
	 * blank and comment line.
	 */
         if(var->descrip && *var->descrip){
           if(fprintf(f, "\n# %s\n", var->descrip) == EOF)
             goto io_err;
	 }

	if(var->is_list){
	    if(var->main_user_val.l == NULL){
		if(fprintf(f, "%s=\n", var->name) == EOF)
		    goto io_err;
	    }else{
		int i;

		for(i=0; var->main_user_val.l[i]; i++)
		    if(fprintf(f, "%s%s%s%s\n", (i) ? "\t" : var->name,
			      (i) ? "" : "=", var->main_user_val.l[i],
			      var->main_user_val.l[i+1] ? ",":"") == EOF)
		    goto io_err;
	    }
	}else{
	    if(var->main_user_val.p == NULL){
		if(fprintf(f, "%s=\n", var->name) == EOF)
		    goto io_err;
	    }else if(strlen(var->main_user_val.p) == 0){
		if(fprintf(f, "%s=\"\"\n", var->name) == EOF)
		    goto io_err;
	    }else{
		if(fprintf(f,"%s=%s\n",var->name,var->main_user_val.p) == EOF)
		    goto io_err;
	    }
	}
    }
    exit(0);


io_err:
    fprintf(stderr, "Error writing config to %s: %s\n",
             filename, error_description(errno));
    exit(-1);
}


/*----------------------------------------------------------------------
  Dump the whole config enchilada using the given function
   
  Args: ps -- pine struct containing vars to dump
	pc -- function to use to write the config data
	brief -- brief dump, only dump user_vals 
 Result: command line, global, user var's written with given function

 ----*/ 
void
dump_config(ps, pc, brief)
    struct pine *ps;
    gf_io_t      pc;
    int          brief;
{
    int	       i;
    char       quotes[3], tmp[MAILTMPLEN];
    register struct variable *vars;
    FEATURE_S *feat;

    quotes[0] = '"'; quotes[1] = '"'; quotes[2] = '\0';

    for(i = (brief ? 2 : 0); i < (brief ? 4 : 6); i++){
	sprintf(tmp, "======= %.20s_val options set",
		(i == 0) ? "Current" :
		 (i == 1) ? "Command_line" :
		  (i == 2) ? "User" :
		   (i == 3) ? "PostloadUser" :
		    (i == 4) ? "Global"
			     : "Fixed");
	gf_puts(tmp, pc);
	if(i > 1){
	    sprintf(tmp, " (%.100s)",
		    (i == 2) ? ((ps->prc) ? ps->prc->name : ".pinerc") :
		    (i == 3) ? ((ps->post_prc) ? ps->post_prc->name : "postload") :
		    (i == 4) ? ((ps->pconf) ? ps->pconf->name
						: SYSTEM_PINERC) :
#if defined(DOS) || defined(OS2)
		    "NO FIXED"
#else
		    ((can_access(SYSTEM_PINERC_FIXED, ACCESS_EXISTS) == 0)
			     ? SYSTEM_PINERC_FIXED : "NO pine.conf.fixed")
#endif
		    );
	    gf_puts(tmp, pc);
	}

	gf_puts(" =======\n", pc);
	for(vars = ps->vars; vars->name; vars++){
	    if(vars->is_list){
		char **t;
		t = (i == 0) ? vars->current_val.l :
		     (i == 1) ? vars->cmdline_val.l :
		      (i == 2) ? vars->main_user_val.l :
		       (i == 3) ? vars->post_user_val.l :
			(i == 4) ? vars->global_val.l
				 : vars->fixed_val.l;
		if(t && *t){
		    sprintf(tmp, " %20.20s : %.*s\n", vars->name,
			    sizeof(tmp)-26, **t ? *t : quotes);
		    gf_puts(tmp, pc);
		    while(++t && *t){
			sprintf(tmp," %20.20s : %.*s\n","",
				sizeof(tmp)-26, **t ? *t : quotes);
			gf_puts(tmp, pc);
		    }
		}
	    }
	    else{
		char *t;
		t = (i == 0) ? vars->current_val.p :
		     (i == 1) ? vars->cmdline_val.p :
		      (i == 2) ? vars->main_user_val.p :
		       (i == 3) ? vars->post_user_val.p :
		        (i == 4) ? vars->global_val.p
				 : vars->fixed_val.p;
		if(t){
		    sprintf(tmp, " %20.20s : %.*s\n", vars->name,
			    sizeof(tmp)-26, *t ? t : quotes);
		    gf_puts(tmp, pc);
		}
	    }
	}
    }

    if(!brief){
	gf_puts("========== Feature settings ==========\n", pc);
	for(i = 0; feat = feature_list(i) ; i++)
	  if(feat->id != F_OLD_GROWTH){
	      sprintf(tmp,
		      "  %.50s%.100s\n", F_ON(feat->id, ps) ? "   " : "no-",
		      feat->name);
	      gf_puts(tmp, pc);
	  }
    }
}


/*----------------------------------------------------------------------
  Dump interesting variables from the given pine struct
   
  Args: ps -- pine struct to dump 
	pc -- function to use to write the config data

 Result: various important pine struct data written

 ----*/ 
void
dump_pine_struct(ps, pc)
    struct pine *ps;
    gf_io_t pc;
{
    char *p;
    extern char term_name[];
    int i;
    MAILSTREAM *m;
    MSGNO_S *msgmap;

    gf_puts("========== struct pine * ==========\n", pc);
    if(!ps){
	gf_puts("!No struct!\n", pc);
	return;
    }

    gf_puts("ui:\tlogin = ", pc);
    gf_puts((ps->ui.login) ? ps->ui.login : "NULL", pc);
    gf_puts(", full = ", pc);
    gf_puts((ps->ui.fullname) ? ps->ui.fullname : "NULL", pc);
    gf_puts("\n\thome = ", pc);
    gf_puts((ps->ui.homedir) ? ps->ui.homedir : "NULL", pc);

    gf_puts("\nhome_dir=\t", pc);
    gf_puts(ps->home_dir ? ps->home_dir : "NULL", pc);
    gf_puts("\nhostname=\t", pc);
    gf_puts(ps->hostname ? ps->hostname : "NULL", pc);
    gf_puts("\nlocaldom=\t", pc);
    gf_puts(ps->localdomain ? ps->localdomain : "NULL", pc);
    gf_puts("\nuserdom=\t", pc);
    gf_puts(ps->userdomain ? ps->userdomain : "NULL", pc);
    gf_puts("\nmaildom=\t", pc);
    gf_puts(ps->maildomain ? ps->maildomain : "NULL", pc);

    gf_puts("\ncur_cntxt=\t", pc);
    gf_puts((ps->context_current && ps->context_current->context)
	    ? ps->context_current->context : "None", pc);
    gf_puts("\ncur_fldr=\t", pc);
    gf_puts(ps->cur_folder, pc);

    gf_puts("\nnstream=\t", pc);
    gf_puts(long2string((long) ps->s_pool.nstream), pc);

    for(i = 0; i < ps->s_pool.nstream; i++){
	m = ps->s_pool.streams[i];
	gf_puts("\nstream slot ", pc);
	gf_puts(long2string((long) i), pc);
	if(!m){
	    gf_puts(" empty", pc);
	    continue;
	}

	if(ps->mail_stream == m)
	  gf_puts("\nThis is the current mailstream", pc);
	if(sp_flagged(m, SP_INBOX))
	  gf_puts("\nThis is the inbox stream", pc);

	gf_puts("\nactual mbox=\t", pc);
	gf_puts(m->mailbox ? m->mailbox : "no mailbox!", pc);

	gf_puts("\nflags=", pc);
	gf_puts(long2string((long) sp_flags(m)), pc);
	gf_puts("\nnew_mail_count=", pc);
	gf_puts(long2string((long) sp_new_mail_count(m)), pc);
	gf_puts("\nmail_since_cmd=", pc);
	gf_puts(long2string((long) sp_mail_since_cmd(m)), pc);
	gf_puts("\nmail_box_changed=", pc);
	gf_puts(long2string((long) sp_mail_box_changed(m)), pc);
	gf_puts("\nunsorted_newmail=", pc);
	gf_puts(long2string((long) sp_unsorted_newmail(m)), pc);
	gf_puts("\nneed_to_rethread=", pc);
	gf_puts(long2string((long) sp_need_to_rethread(m)), pc);
	gf_puts("\nviewing_a_thread=", pc);
	gf_puts(long2string((long) sp_viewing_a_thread(m)), pc);
	gf_puts("\ndead_stream=", pc);
	gf_puts(long2string((long) sp_dead_stream(m)), pc);
	gf_puts("\nnoticed_dead_stream=", pc);
	gf_puts(long2string((long) sp_noticed_dead_stream(m)), pc);
	gf_puts("\nio_error_on_stream=", pc);
	gf_puts(long2string((long) sp_io_error_on_stream(m)), pc);

	msgmap = sp_msgmap(m);
	if(msgmap){
	    gf_puts("\nmsgmap: tot=", pc);
	    gf_puts(long2string(mn_get_total(msgmap)), pc);
	    gf_puts(", cur=", pc);
	    gf_puts(long2string(mn_get_cur(msgmap)), pc);
	    gf_puts(", del=", pc);
	    gf_puts(long2string(count_flagged(m, F_DEL)),pc);
	    gf_puts(", hid=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_HIDE)), pc);
	    gf_puts(", exld=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_EXLD)), pc);
	    gf_puts(", slct=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_SLCT)), pc);
	    gf_puts(", chid=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_CHID)), pc);
	    gf_puts(", coll=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_COLL)), pc);
	    gf_puts(", usor=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_USOR)), pc);
	    gf_puts(", stmp=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_STMP)), pc);
	    gf_puts(", sort=", pc);
	    if(mn_get_revsort(msgmap))
	      gf_puts("rev-", pc);

	    gf_puts(sort_name(mn_get_sort(msgmap)), pc);
	}
	else
	  gf_puts("\nNo msgmap", pc);
    }


    gf_puts("\nterm ", pc);
#if !defined(DOS) && !defined(OS2)
    gf_puts("type=", pc);
    gf_puts(term_name, pc);
    gf_puts(", ttyname=", pc);
    gf_puts((p = (char *)ttyname(0)) ? p : "NONE", pc);
#endif
    gf_puts(", size=", pc);
    gf_puts(int2string(ps->ttyo->screen_rows), pc);
    gf_puts("x", pc);
    gf_puts(int2string(ps->ttyo->screen_cols), pc);
    gf_puts(", speed=", pc);
    gf_puts((ps->low_speed) ? "slow" : "normal", pc);
    gf_puts("\n", pc);
}


/*----------------------------------------------------------------------
      Set a user variable and save the .pinerc
   
  Args:  var -- The index of the variable to set from pine.h (V_....)
         value -- The string to set the value to

 Result: -1 is returned on failure and 0 is returned on success

 The vars data structure is updated and the pinerc saved.
 ----*/ 
set_variable(var, value, expand, commit, which)
     int   var, expand, commit;
     char *value;
     EditWhich which;
{
    struct variable *v;
    char           **apval;
    PINERC_S        *prc;

    v = &ps_global->vars[var];

    if(!v->is_user) 
      panic1("Trying to set non-user variable %s", v->name);
    
    /* Override value of which, at most one of these should be set */
    if(v->is_onlymain)
      which = Main;
    else if(v->is_outermost)
      which = ps_global->ew_for_except_vars;

    apval = APVAL(v, which);

    if(!apval)
      return(-1);

    if(*apval)
      fs_give((void **)apval);

    *apval = value ? cpystr(value) : NULL;
    set_current_val(v, expand, FALSE);

    switch(which){
      case Main:
	prc = ps_global->prc;
	break;
      case Post:
	prc = ps_global->post_prc;
	break;
    }

    if(prc)
      prc->outstanding_pinerc_changes = 1;

    return(commit ? write_pinerc(ps_global, which, WRP_NONE) : 1);
}


/*----------------------------------------------------------------------
      Set a user variable list and save the .pinerc
   
  Args:  var -- The index of the variable to set from pine.h (V_....)
         lvalue -- The list to set the value to

 Result: -1 is returned on failure and 0 is returned on success

 The vars data structure is updated and if write_it, the pinerc is saved.
 ----*/ 
set_variable_list(var, lvalue, write_it, which)
    int    var;
    char **lvalue;
    int    write_it;
    EditWhich which;
{
    char          ***alval;
    int              i;
    struct variable *v = &ps_global->vars[var];
    PINERC_S        *prc;

    if(!v->is_user || !v->is_list)
      panic1("BOTCH: Trying to set non-user or non-list variable %s", v->name);

    /* Override value of which, at most one of these should be set */
    if(v->is_onlymain)
      which = Main;
    else if(v->is_outermost)
      which = ps_global->ew_for_except_vars;

    alval = ALVAL(v, which);
    if(!alval)
      return(-1);

    if(*alval)
      free_list_array(alval);

    if(lvalue){
	for(i = 0; lvalue[i] ; i++)	/* count elements */
	  ;

	*alval = (char **) fs_get((i+1) * sizeof(char *));

	for(i = 0; lvalue[i] ; i++)
	  (*alval)[i] = cpystr(lvalue[i]);

	(*alval)[i]         = NULL;
    }

    set_current_val(v, TRUE, FALSE);

    switch(which){
      case Main:
	prc = ps_global->prc;
	break;
      case Post:
	prc = ps_global->post_prc;
	break;
    }

    if(prc)
      prc->outstanding_pinerc_changes = 1;

    return(write_it ? write_pinerc(ps_global, which, WRP_NONE) : 0);
}
           

/*
 * Return malloc'd string containing only the context specifier given
 * a string containing the raw pinerc entry
 */
char *
context_string(s)
    char *s;
{
    CONTEXT_S *t = new_context(s, NULL);
    char      *rv = NULL;

    if(t){
	/*
	 * Take what we want from context, then free the rest...
	 */
	rv = t->context;
	t->context = NULL;			/* don't free it! */
	free_context(&t);
    }

    return(rv ? rv : cpystr(""));
}


/*----------------------------------------------------------------------
    Make sure the pine folders directory exists, with proper folders

   Args: ps -- pine structure to get mail directory and contexts from

  Result: returns 0 if it exists or it is created and all is well
                  1 if it is missing and can't be created.
  ----*/
init_mail_dir(ps)
    struct pine *ps;
{

/* MAIL_LIST: can we use imap4 CREATE? */
    /*
     * We don't really care if mail_dir exists if it isn't 
     * part of the first folder collection specified.  If this
     * is the case, it must have been created external to us, so
     * just move one...
     */
    if(ps->VAR_FOLDER_SPEC && ps->VAR_FOLDER_SPEC[0]){
	char *p  = context_string(ps->VAR_FOLDER_SPEC[0]);
	int   rv = strncmp(p, ps->VAR_MAIL_DIRECTORY,
			   strlen(ps->VAR_MAIL_DIRECTORY));
	fs_give((void **)&p);
	if(rv)
	  return(0);
    }

    switch(is_writable_dir(ps->folders_dir)){
      case 0:
        /* --- all is well --- */
	return(0);

      case 1:
	sprintf(tmp_20k_buf, init_md_exists, ps->folders_dir);
	display_init_err(tmp_20k_buf, 1);
	return(-1);

      case 2:
	sprintf(tmp_20k_buf, init_md_file, ps->folders_dir);
	display_init_err(tmp_20k_buf, 1);
	return(-1);

      case 3:
	sprintf(tmp_20k_buf, init_md_create, ps->folders_dir);
	display_init_err(tmp_20k_buf, 0);
#ifndef	_WINDOWS
    	sleep(4);
#endif
        if(create_mail_dir(ps->folders_dir) < 0){
            sprintf(tmp_20k_buf, "Error creating subdirectory \"%s\" : %s",
		    ps->folders_dir, error_description(errno));
	    display_init_err(tmp_20k_buf, 1);
            return(-1);
        }
    }

    return(0);
}


/*----------------------------------------------------------------------
  Make sure the default save folders exist in the default
  save context.
  ----*/
void
display_init_err(s, err)
    char *s;
    int   err;
{
#ifdef	_WINDOWS
    mswin_messagebox(s, err);
#else
    int n = 0;

    if(err)
      fputc(BELL, stdout);

    for(; *s; s++)
      if(++n > 60 && isspace((unsigned char)*s)){
	  n = 0;
	  fputc('\n', stdout);
	  while(*(s+1) && isspace((unsigned char)*(s+1)))
	    s++;
      }
      else
	fputc(*s, stdout);

    fputc('\n', stdout);
#endif
}


/*----------------------------------------------------------------------
  Make sure the default save folders exist in the default
  save context.
  ----*/
void
init_save_defaults()
{
    CONTEXT_S  *save_cntxt;

    if(!ps_global->VAR_DEFAULT_FCC ||
       !*ps_global->VAR_DEFAULT_FCC ||
       !ps_global->VAR_DEFAULT_SAVE_FOLDER ||
       !*ps_global->VAR_DEFAULT_SAVE_FOLDER)
      return;

    if(!(save_cntxt = default_save_context(ps_global->context_list)))
      save_cntxt = ps_global->context_list;

    if(!(folder_exists(save_cntxt, ps_global->VAR_DEFAULT_FCC) & FEX_ISFILE))
      context_create(save_cntxt, NULL, ps_global->VAR_DEFAULT_FCC);

    if(!(folder_exists(save_cntxt, ps_global->VAR_DEFAULT_SAVE_FOLDER) &
								 FEX_ISFILE))
      context_create(save_cntxt, NULL, ps_global->VAR_DEFAULT_SAVE_FOLDER);

    free_folder_list(save_cntxt);
}



/*----------------------------------------------------------------------
   Routines for pruning old Fcc, usually "sent-mail" folders.     
  ----*/
struct sm_folder {
    char *name;
    int   month_num;
};


/*
 * Pruning prototypes
 */
void	 delete_old_mail PROTO((struct sm_folder *, CONTEXT_S *, char *));
struct	 sm_folder *get_mail_list PROTO((CONTEXT_S *, char *));
int	 prune_folders PROTO((CONTEXT_S *, char *, int, char *, unsigned));



/*----------------------------------------------------------------------
      Put sent-mail files in date order 

   Args: a, b  -- The names of two files.  Expects names to be sent-mail-mmm-yy
                  Other names will sort in order and come before those
                  in above format.
 ----*/
int   
compare_sm_files(aa, bb)
    const QSType *aa, *bb;
{
    struct sm_folder *a = (struct sm_folder *)aa,
                     *b = (struct sm_folder *)bb;

    if(a->month_num == -1 && b->month_num == -1)
      return(strucmp(a->name, b->name));
    if(a->month_num == -1)      return(-1);
    if(b->month_num == -1)      return(1);

    return(a->month_num - b->month_num);
}



/*----------------------------------------------------------------------
      Create an ordered list of sent-mail folders and their month numbers

   Args: dir -- The directory to find the list of files in

 Result: Pointer to list of files is returned. 

This list includes all files that start with "sent-mail", but not "sent-mail" 
itself.
  ----*/
struct sm_folder *
get_mail_list(list_cntxt, folder_base)
    CONTEXT_S *list_cntxt;
    char      *folder_base;
{
#define MAX_FILES  (150)
    register struct sm_folder *sm  = NULL;
    struct sm_folder          *sml = NULL;
    char                      *filename;
    int                        i, folder_base_len;
    char		       searchname[MAXPATH+1];

    sml = sm = (struct sm_folder *)fs_get(sizeof(struct sm_folder)*MAX_FILES);
    memset((void *)sml, 0, sizeof(struct sm_folder) * MAX_FILES);
    if((folder_base_len = strlen(folder_base)) == 0 || !list_cntxt){
        sml->name = cpystr("");
        return(sml);
    }

#ifdef	DOS
    if(*list_cntxt->context != '{'){	/* NOT an IMAP collection! */
	sprintf(searchname, "%4.4s*", folder_base);
	folder_base_len = strlen(searchname) - 1;
    }
    else
#endif
    sprintf(searchname, "%.*s*", sizeof(searchname)-2, folder_base);

    build_folder_list(NULL, list_cntxt, searchname, NULL, BFL_FLDRONLY);
    for(i = 0; i < folder_total(FOLDERS(list_cntxt)); i++){
	filename = folder_entry(i, FOLDERS(list_cntxt))->name;
#ifdef	DOS
        if(struncmp(filename, folder_base, folder_base_len) == 0
           && strucmp(filename, folder_base)){

	if(*list_cntxt->context != '{'){
	    int j;
	    for(j = 0; j < 4; j++)
	      if(!isdigit((unsigned char)filename[folder_base_len + j]))
		break;

	   if(j < 4)		/* not proper date format! */
	     continue;		/* keep trying */
	}
#else
#ifdef OS2
        if(strnicmp(filename, folder_base, folder_base_len) == 0
           && stricmp(filename, folder_base)){
#else
        if(strncmp(filename, folder_base, folder_base_len) == 0
           && strcmp(filename, folder_base)){
#endif
#endif
	    sm->name = cpystr(filename);
#ifdef	DOS
	    if(*list_cntxt->context != '{'){ /* NOT an IMAP collection! */
		sm->month_num  = (sm->name[folder_base_len] - '0') * 10;
		sm->month_num += sm->name[folder_base_len + 1] - '0';
	    }
	    else
#endif
            sm->month_num = month_num(sm->name + (size_t)folder_base_len + 1);
            sm++;
            if(sm >= &sml[MAX_FILES - 1])
               break; /* Too many files, ignore the rest ; shouldn't occur */
        }
    }

    sm->name = cpystr("");

    /* anything to sort?? */
    if(sml->name && *(sml->name) && (sml+1)->name && *((sml+1)->name)){
	qsort(sml,
	      sm - sml,
	      sizeof(struct sm_folder),
	      compare_sm_files);
    }

    return(sml);
}



/*----------------------------------------------------------------------
      Rename the current sent-mail folder to sent-mail for last month

   open up sent-mail and get date of very first message
   if date is last month rename and...
       if files from 3 months ago exist ask if they should be deleted and...
           if files from previous months and yes ask about them, too.   
  ----------------------------------------------------------------------*/
int
expire_sent_mail()
{
    int		 cur_month, ok = 1;
    time_t	 now;
    char	 tmp[50], **p;
    struct tm	*tm_now;
    CONTEXT_S	*prune_cntxt;

    dprint(5, (debugfile, "==== expire_mail called ====\n"));

    now = time((time_t *)0);
    tm_now = localtime(&now);

    /*
     * If the last time we did this is blank (as if pine's run for
     * first time), don't go thru list asking, but just note it for 
     * the next time...
     */
    if(ps_global->VAR_LAST_TIME_PRUNE_QUESTION == NULL){
	ps_global->last_expire_year = tm_now->tm_year;
	ps_global->last_expire_month = tm_now->tm_mon;
	sprintf(tmp, "%d.%d", ps_global->last_expire_year,
		ps_global->last_expire_month + 1);
	set_variable(V_LAST_TIME_PRUNE_QUESTION, tmp, 1, 1, Main);
	return(0);
    }

    if(ps_global->last_expire_year != -1 &&
      (tm_now->tm_year <  ps_global->last_expire_year ||
       (tm_now->tm_year == ps_global->last_expire_year &&
        tm_now->tm_mon <= ps_global->last_expire_month)))
      return(0); 
    
    cur_month = (1900 + tm_now->tm_year) * 12 + tm_now->tm_mon;
    dprint(5, (debugfile, "Current month %d\n", cur_month));

    /*
     * locate the default save context...
     */
    if(!(prune_cntxt = default_save_context(ps_global->context_list)))
      prune_cntxt = ps_global->context_list;

    /*
     * Since fcc's and read-mail can be an IMAP mailbox, be sure to only
     * try expiring a list if it's an ambiguous name associated with some
     * collection...
     *
     * If sentmail set outside a context, then pruning is up to the
     * user...
     */
    if(prune_cntxt){
	if(ps_global->VAR_DEFAULT_FCC && *ps_global->VAR_DEFAULT_FCC
	   && context_isambig(ps_global->VAR_DEFAULT_FCC))
	  ok = prune_folders(prune_cntxt, ps_global->VAR_DEFAULT_FCC,
			     cur_month, " SENT",
			     ps_global->pruning_rule);

	if(ok && ps_global->VAR_READ_MESSAGE_FOLDER 
	   && *ps_global->VAR_READ_MESSAGE_FOLDER
	   && context_isambig(ps_global->VAR_READ_MESSAGE_FOLDER))
	  ok = prune_folders(prune_cntxt, ps_global->VAR_READ_MESSAGE_FOLDER,
			     cur_month, " READ",
			     ps_global->pruning_rule);
    }

    /*
     * Within the default prune context,
     * prune back the folders with the given name
     */
    if(ok && prune_cntxt && (p = ps_global->VAR_PRUNED_FOLDERS))
      for(; ok && *p; p++)
	if(**p && context_isambig(*p))
	  ok = prune_folders(prune_cntxt, *p, cur_month, "",
			     ps_global->pruning_rule);

    /*
     * Mark that we're done for this month...
     */
    if(ok){
	ps_global->last_expire_year = tm_now->tm_year;
	ps_global->last_expire_month = tm_now->tm_mon;
	sprintf(tmp, "%d.%d", ps_global->last_expire_year,
		ps_global->last_expire_month + 1);
	set_variable(V_LAST_TIME_PRUNE_QUESTION, tmp, 1, 1, Main);
    }

    return(1);
}


int
first_run_of_month()
{
    time_t     now;
    struct tm *tm_now;

    now = time((time_t *) 0);
    tm_now = localtime(&now);

    if(ps_global->last_expire_year == -1 ||
      (tm_now->tm_year <  ps_global->last_expire_year ||
       (tm_now->tm_year == ps_global->last_expire_year &&
        tm_now->tm_mon <= ps_global->last_expire_month)))
      return(0); 

    return(1);
}


int
first_run_of_year()
{
    time_t     now;
    struct tm *tm_now;

    now = time((time_t *) 0);
    tm_now = localtime(&now);

    if(ps_global->last_expire_year == -1 ||
      (tm_now->tm_year <=  ps_global->last_expire_year))
      return(0); 

    return(1);
}


/*----------------------------------------------------------------------
     Check pruned-folders for validity, making sure they are in the 
     same context as sent-mail.

  ----*/
int
prune_folders_ok()
    
{
    char **p;

    for(p = ps_global->VAR_PRUNED_FOLDERS; p && *p && **p; p++)
      if(!context_isambig(*p))
	return(0);

    return(1);
}


/*----------------------------------------------------------------------
     Offer to delete old sent-mail folders

  Args: sml -- The list of sent-mail folders
 
  ----*/
int
prune_folders(prune_cntxt, folder_base, cur_month, type, pr)
    CONTEXT_S *prune_cntxt;
    char      *folder_base;
    int        cur_month;
    char      *type;
    unsigned   pr;
{
    char         path[MAXPATH+1], path2[MAXPATH+1],  prompt[128], tmp[21];
    char         ctmp[MAILTMPLEN];
    int          month_to_use, exists;
    struct sm_folder *mail_list, *sm;

    mail_list = get_mail_list(prune_cntxt, folder_base);
    free_folder_list(prune_cntxt);

#ifdef	DEBUG
    for(sm = mail_list; sm != NULL && sm->name[0] != '\0'; sm++)
      dprint(5, (debugfile,"Old sent-mail: %5d  %s\n", sm->month_num,
	     sm->name ? sm->name : "?"));
#endif

    for(sm = mail_list; sm != NULL && sm->name[0] != '\0'; sm++)
      if(sm->month_num == cur_month - 1)
        break;  /* matched a month */
 
    month_to_use = (sm == NULL || sm->name[0] == '\0') ? cur_month - 1 : 0;

    dprint(5, (debugfile, "Month_to_use : %d\n", month_to_use));

    if(month_to_use == 0 || pr == PRUNE_NO_AND_ASK || pr == PRUNE_NO_AND_NO)
      goto delete_old;

    strncpy(path, folder_base, sizeof(path)-1);
    path[sizeof(path)-1] = '\0';
    strncpy(path2, folder_base, sizeof(path2)-1);
    path2[sizeof(path2)-1] = '\0';

    if(F_ON(F_PRUNE_USES_ISO,ps_global)){             /* sent-mail-yyyy-mm */
	sprintf(path2 + strlen(path2), "-%04.4d-%02.2d", month_to_use/12,
		month_to_use % 12 + 1);
    }
    else{
	strncpy(tmp, month_abbrev((month_to_use % 12)+1), 20);
	tmp[20] = '\0';
	lcase((unsigned char *) tmp);
#ifdef	DOS
	if(*prune_cntxt->context != '{'){
	  int i;

	  sprintf(path2 + (size_t)(((i = strlen(path2)) > 4) ? 4 : i),
		  "%2.2d%2.2d", (month_to_use % 12) + 1,
		  ((month_to_use / 12) - 1900) % 100);
	}
	else
#endif
	sprintf(path2 + strlen(path2), "-%.20s-%d", tmp, month_to_use/12);
    }

    Writechar(BELL, 0);
    sprintf(prompt, "Move current \"%.50s\" to \"%.50s\"", path, path2);
    if((exists = folder_exists(prune_cntxt, folder_base)) == FEX_ERROR){
        dprint(5, (debugfile, "prune_folders: Error testing existence\n"));
        return(0);
    }
    else if(exists == FEX_NOENT){	/* doesn't exist */
        dprint(5, (debugfile, "prune_folders: nothing to prune <%s %s>\n",
		   prune_cntxt->context ? prune_cntxt->context : "?",
		   folder_base ? folder_base : "?"));
        goto delete_old;
    }
    else if(!(exists & FEX_ISFILE)
	    || pr == PRUNE_NO_AND_ASK || pr == PRUNE_NO_AND_NO
	    || ((pr == PRUNE_ASK_AND_ASK || pr == PRUNE_ASK_AND_NO) &&
	        want_to(prompt, 'n', 0, h_wt_expire, WT_FLUSH_IN) == 'n')){
	dprint(5, (debugfile, "User declines renaming %s\n",
	       ps_global->VAR_DEFAULT_FCC ? ps_global->VAR_DEFAULT_FCC : "?"));
	goto delete_old;
    }

    /*--- User says OK to rename ---*/
    dprint(5, (debugfile, "rename \"%.100s\" to \"%.100s\"\n",
	   path ? path : "?", path2 ? path2 : "?"));
    q_status_message1(SM_ORDER, 1, 3,
		      "Renaming \"%.200s\" at start of month",
		      pretty_fn(folder_base));

    if(!context_rename(prune_cntxt, NULL, path, path2)){
        q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Error renaming \"%.200s\": %.200s",
                          pretty_fn(folder_base),
			  error_description(errno));
        dprint(1, (debugfile, "Error renaming %.100s to %.100s: %.100s\n",
                   path ? path : "?", path2 ? path2 : "?",
		   error_description(errno)));
        display_message('x');
        goto delete_old;
    }

    context_create(prune_cntxt, NULL, folder_base);

  delete_old:
    if(pr == PRUNE_ASK_AND_ASK || pr == PRUNE_YES_AND_ASK
       || pr == PRUNE_NO_AND_ASK)
      delete_old_mail(mail_list, prune_cntxt, type);

    if(sm = mail_list){
	while(sm->name){
	    fs_give((void **)&(sm->name));
	    sm++;
	}

        fs_give((void **)&mail_list);
    }

    return(1);
}


/*----------------------------------------------------------------------
     Offer to delete old sent-mail folders

  Args: sml       -- The list of sent-mail folders
        fcc_cntxt -- context to delete list of folders in
        type      -- label indicating type of folders being deleted
 
  ----*/
void
delete_old_mail(sml, fcc_cntxt, type)
    struct sm_folder *sml;
    CONTEXT_S        *fcc_cntxt;
    char             *type;
{
    char  prompt[150];
    struct sm_folder *sm;

    for(sm = sml; sm != NULL && sm->name[0] != '\0'; sm++){
        sprintf(prompt,
	       "To save disk space, delete old%.10s mail folder \"%.30s\" ",
	       type, sm->name);
        if(want_to(prompt, 'n', 0, h_wt_delete_old, WT_FLUSH_IN) == 'y'){

	    if(!context_delete(fcc_cntxt, NULL, sm->name)){
		q_status_message1(SM_ORDER,
				  3, 3, "Error deleting \"%.200s\".", sm->name);
		dprint(1, (debugfile, "Error context_deleting %s in %s\n",
			   sm->name ? sm->name : "?",
			   (fcc_cntxt && fcc_cntxt->context)
				     ? fcc_cntxt->context : "<null>"));
            }
	    else{
		int index;

		if((index = folder_index(sm->name, fcc_cntxt, FI_FOLDER)) >= 0)
		  folder_delete(index, FOLDERS(fcc_cntxt));
	    }
	}else{
		/* break;*/ /* skip out of the whole thing when he says no */
		/* Decided to keep asking anyway */
        }
    }
}



void
rd_grope_for_metaname()
{
    char            *val, *q, *filename, *contents, *free_this = NULL;
    struct variable *vars = ps_global->vars;

    if(!ps_global->prc)
      return;
    
    if(ps_global->prc->type == Loc)
      filename = ps_global->prc->name;
    else{
        free_this = filename = rd_derived_cachename(ps_global->prc->name);
    }

    /*
     * Look in the file for metadata filename. This only works
     * if the file is a filled-in personal pinerc or filled-in cache of
     * a personal pinerc.
     */
    if((contents = read_file(filename)) != NULL){
	if((val = srchstr(contents, METASTR)) != NULL){

	    /* mimic processing that read_pinerc does */

	    val += strlen(METASTR);
	    if((q = strpbrk(val, "\n\r")) != NULL)
		*q = '\0';

	    for(; *val == ' ' || *val == '\t'; val++)
	      ;
	    
	    if(*val != '\0'){
		struct variable *v;
		char **apval;

		if(*val == '"'){
		    val++;
		    for(q = val; *q && *q != '"'; q++);
		    if(*q == '"')
		      *q = '\0';
		    else
		      removing_trailing_white_space(val);
		}

		v = &ps_global->vars[V_REMOTE_ABOOK_METADATA];
		apval = APVAL(v, Main);
		if(apval && *apval)
		  fs_give((void **)apval);
		
		if(apval)
		  *apval = cpystr(val);

		set_current_val(&vars[V_REMOTE_ABOOK_METADATA],
				TRUE, TRUE);
	    }
	}
	
	fs_give((void **)&contents);
    }

    if(free_this)
      fs_give((void **)&free_this);
}


/*
 * Check if the remote data folder exists and create an empty folder
 * if it doesn't exist.
 *
 * Args -        type -- The type of remote storage.
 *        remote_name -- 
 *          type_spec -- Type-specific data.
 *              flags -- 
 *         err_prefix -- Should usually end with a SPACE
 *         err_suffix -- Should usually begin with a SPACE
 *
 * Returns a pointer to a REMDATA_S with access set to either
 * NoExists or MaybeRorW. On success, "so" will point to a storage object.
 */
REMDATA_S *
rd_create_remote(type, remote_name, type_spec, flags, err_prefix, err_suffix)
    RemType   type;
    char     *remote_name;
    void     *type_spec;
    unsigned *flags;
    char     *err_prefix;
    char     *err_suffix;
{
    REMDATA_S *rd = NULL;
    CONTEXT_S *dummy_cntxt;

    dprint(7, (debugfile, "rd_create_remote \"%s\"\n",
	   remote_name ? remote_name : "?"));

    rd = rd_new_remdata(type, remote_name, type_spec);
    if(flags)
      rd->flags = *flags;
    
    switch(rd->type){
      case RemImap:
	if(rd->flags & NO_PERM_CACHE){
	    if(rd->rn && (rd->so = so_get(CharStar, NULL, WRITE_ACCESS))){
		if(rd->flags & NO_FILE){
		    rd->sonofile = so_get(CharStar, NULL, WRITE_ACCESS);
		    if(!rd->sonofile)
		      rd->flags &= ~NO_FILE;
		}

		/*
		 * We're not going to check if it is there in this case,
		 * in order to save ourselves some round trips and
		 * connections. We'll just try to select it and then
		 * recover at that point if it isn't already there.
		 */
		rd->flags |= REM_OUTOFDATE;
		rd->access = MaybeRorW;
	    }

	}
	else{
	    /*
	     * Open_fcc creates the folder if it didn't already exist.
	     */
	    if(rd->rn && (rd->so = open_fcc(rd->rn, &dummy_cntxt, 1,
					    err_prefix, err_suffix)) != NULL){
		/*
		 * We know the folder is there but don't know what access
		 * rights we have until we try to select it, which we don't
		 * want to do unless we have to. So delay evaluating.
		 */
		rd->access = MaybeRorW;
	    }
	}

	break;

      default:
	q_status_message(SM_ORDER, 3,5, "rd_create_remote: type not supported");
	break;
    }

    return(rd);
}


REMDATA_S *
rd_new_remdata(type, remote_name, type_spec)
    RemType type;
    char   *remote_name;
    void   *type_spec;
{
    REMDATA_S *rd = NULL;

    rd = (REMDATA_S *)fs_get(sizeof(*rd));
    memset((void *)rd, 0, sizeof(*rd));

    rd->type   = type;
    rd->access = NoExists;

    if(remote_name)
      rd->rn = cpystr(remote_name);

    switch(rd->type){
      case RemImap:
	if(type_spec)
	  rd->t.i.special_hdr = cpystr((char *)type_spec);

	break;

      default:
	q_status_message(SM_ORDER, 3,5, "rd_new_remdata: type not supported");
	break;
    }

    return(rd);
}


/*
 * Closes the remote stream and frees.
 */
void
rd_free_remdata(rd)
    REMDATA_S **rd;
{
    if(rd && *rd){
	rd_close_remote(*rd);

	if((*rd)->rn)
	  fs_give((void **)&(*rd)->rn);

	if((*rd)->lf)
	  fs_give((void **)&(*rd)->lf);

	if((*rd)->so){
	    so_give(&(*rd)->so);
	    (*rd)->so = NULL;
	}

	if((*rd)->sonofile){
	    so_give(&(*rd)->sonofile);
	    (*rd)->sonofile = NULL;
	}

	switch((*rd)->type){
	  case RemImap:
	    if((*rd)->t.i.special_hdr)
	      fs_give((void **)&(*rd)->t.i.special_hdr);

	    if((*rd)->t.i.chk_date)
	      fs_give((void **)&(*rd)->t.i.chk_date);
	    
	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 5,
			     "rd_free_remdata: type not supported");
	    break;
	}

	fs_give((void **)rd);
    }
}


/*
 * Call this when finished with the remdata. This does the REMTRIM if the
 * flag is set.
 */
void
rd_trim_remdata(rd)
    REMDATA_S **rd;
{
    if(!(rd && *rd))
      return;

    switch((*rd)->type){
      case RemImap:
	/*
	 * Trim the number of saved copies of remote data history.
	 * The first message is a fake message that always
	 * stays there, then come ps_global->remote_abook_history messages
	 * which are each a revision of the data, then comes the active
	 * data.
	 */
	if((*rd)->flags & DO_REMTRIM &&
	   (*rd)->flags & REMUPDATE_DONE &&
	   !((*rd)->flags & REM_OUTOFDATE) &&
	   (*rd)->t.i.chk_nmsgs > ps_global->remote_abook_history + 2){

	    /* make sure stream is open */
	    rd_ping_stream(*rd);
	    rd_open_remote(*rd);

	    if(!rd_remote_is_readonly(*rd)){
		if((*rd)->t.i.stream &&
		   (*rd)->t.i.stream->nmsgs >
					ps_global->remote_abook_history + 2){
		    char sequence[20];
		    int  user_deleted = 0;

		    /*
		     * If user manually deleted some, we'd better not delete
		     * any more.
		     */
		    if(count_flagged((*rd)->t.i.stream, F_DEL) == 0L){

			dprint(4, (debugfile, "  rd_trim: trimming remote: mark msgs 2-%ld deleted (%s)\n", (*rd)->t.i.stream->nmsgs - 1 - ps_global->remote_abook_history, (*rd)->rn ? (*rd)->rn : "?"));
			sprintf(sequence, "2:%ld",
		(*rd)->t.i.stream->nmsgs - 1 - ps_global->remote_abook_history);
			mail_flag((*rd)->t.i.stream, sequence,
				  "\\DELETED", ST_SET);
		    }
		    else
		      user_deleted++;

		    mail_expunge((*rd)->t.i.stream);

		    if(!user_deleted)
		      rd_update_metadata(*rd, NULL);
		    /* else
		     *  don't update metafile because user is messing with
		     *  the remote folder manually. We'd better re-read it next
		     *  time.  */
		}
	    }

	    ps_global->noshow_error = 0;
	}

	break;

      default:
	q_status_message(SM_ORDER, 3,5, "rd_trim_remdata: type not supported");
	break;
    }
}


/*
 * All done with this remote data. Trim the folder, close the
 * stream, and free.
 */
void
rd_close_remdata(rd)
    REMDATA_S **rd;
{
    if(!(rd && *rd))
      return;

    rd_trim_remdata(rd);

    if((*rd)->lf && (*rd)->flags & DEL_FILE)
      (void)unlink((*rd)->lf);

    /* this closes the stream and frees memory */
    rd_free_remdata(rd);
}


/*
 * Looks in the metadata file for the cache line corresponding to rd and
 * fills in data in rd.
 *
 * Return value -- 1 if it is likely that the filename we're returning
 *   is the permanent name of the local cache file and it may already have
 *   a cached copy of the data. This is to tell us if it makes sense to use
 *   the cached copy when we are unable to contact the remote server.
 *   0 otherwise.
 */
int
rd_read_metadata(rd)
    REMDATA_S *rd;
{
    REMDATA_META_S *rab = NULL;
    int             try_cache = 0;
    struct variable *vars = ps_global->vars;

    dprint(7, (debugfile, "rd_read_metadata \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd)
      return try_cache;

    if(rd->flags & NO_PERM_CACHE)
      rab = NULL;
    else if(rd->flags & DERIVE_CACHE){
	if(VAR_REMOTE_ABOOK_METADATA && VAR_REMOTE_ABOOK_METADATA[0]){
	    rab = rd_find_our_metadata(rd->rn, &rd->flags);
	    if(rab){
		/* now we're normal */
		rd->flags &= ~DERIVE_CACHE;
	    }
	}

	if(rd->flags & DERIVE_CACHE){
	    /* didn't find the metadata */
	    rd->flags |= REM_OUTOFDATE;

	    /* derive the name of the cache file from remote name */
	    rd->lf = rd_derived_cachename(rd->rn);
	}
    }
    else
      rab = rd_find_our_metadata(rd->rn, &rd->flags);

    if(!rab){
	if(!rd->lf){
	    rd->flags |= (NO_META_UPDATE | REM_OUTOFDATE);
	    if(!(rd->flags & NO_FILE)){
		rd->lf = temp_nam(NULL, "a6");
		rd->flags |= DEL_FILE;
	    }

	    /* display error */
	    if(!(rd->flags & NO_PERM_CACHE))
	      display_message('x');

	    dprint(2, (debugfile, "using temp cache file %s\n",
		   rd->lf ? rd->lf : "<none>"));
	}
    }
    else if(rab->local_cache_file){	/* A-OK, it was in the file already */
	if(!is_absolute_path(rab->local_cache_file)){
	    char  dir[MAXPATH+1], path[MAXPATH+1];
	    char *lc;

	    /*
	     * This should be the normal case. The file is stored as a
	     * filename in the pinerc dir, so that it can be
	     * accessed from the PC or from unix where the pathnames to
	     * get there will be different.
	     */

	    if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
		int to_copy;

		to_copy = (lc - ps_global->pinerc > 1)
			    ? (lc - ps_global->pinerc - 1) : 1;
		strncpy(dir, ps_global->pinerc, min(to_copy, sizeof(dir)-1));
		dir[min(to_copy, sizeof(dir)-1)] = '\0';
	    }
	    else{
		dir[0] = '.';
		dir[1] = '\0';
	    }

	    build_path(path, dir, rab->local_cache_file, sizeof(path));
	    rd->lf = cpystr(path);
	}
	else{
	    rd->lf = rab->local_cache_file;
	    /* don't free this below, we're using it */
	    rab->local_cache_file = NULL;
	}

	rd->read_status = rab->read_status;

	switch(rd->type){
	  case RemImap:
	    rd->t.i.chk_date    = rab->date;
	    rab->date = NULL;	/* don't free this below, we're using it */

	    dprint(7, (debugfile,
	       "in read_metadata, setting chk_date from metadata to ->%s<-\n",
	       rd->t.i.chk_date ? rd->t.i.chk_date : "?"));
	    rd->t.i.chk_nmsgs   = rab->nmsgs;
	    rd->t.i.uidvalidity = rab->uidvalidity;
	    rd->t.i.uidnext     = rab->uidnext;
	    rd->t.i.uid         = rab->uid;
	    rd->t.i.chk_nmsgs   = rab->nmsgs;
	    dprint(7, (debugfile,
	      "setting uid=%lu uidnext=%lu uidval=%lu read_stat=%c nmsgs=%lu\n",
		 rd->t.i.uid, rd->t.i.uidnext, rd->t.i.uidvalidity,
		 rd->read_status ? rd->read_status : '0',
		 rd->t.i.chk_nmsgs));
	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 5,
			     "rd_read_metadata: type not supported");
	    break;
	}

	if(rd->t.i.chk_nmsgs > 0)
	  try_cache++;	/* cache should be valid if we can't contact server */
    }
    /*
     * The line for this data wasn't in the metadata file yet.
     * Figure out what should go there and put it in.
     */
    else{
	/*
	 * The local_cache_file is where we will store the cached local
	 * copy of the remote data.
	 */
	rab->local_cache_file = tempfile_in_same_dir(ps_global->pinerc,
						     meta_prefix, NULL);
	if(rab->local_cache_file){
	    rd->lf = rab->local_cache_file;
	    rd_write_metadata(rd, 0);
	    rab->local_cache_file = NULL;
	}
	else
	  rd->lf = temp_nam(NULL, "a7");
    }

    if(rab){
	if(rab->local_cache_file)
	  fs_give((void **)&rab->local_cache_file);
	if(rab->date)
	  fs_give((void **)&rab->date);
	fs_give((void **)&rab);
    }

    return(try_cache);
}


char *
rd_derived_cachename(remote_name)
    char *remote_name;
{
    char  tmp[32];
    char *ans = NULL;
    char  dir[MAXPATH+1], path[MAXPATH+1];
    char *lc, *p, *contents = NULL;
    extern unsigned int phone_home_hash();

    dprint(7, (debugfile, "rd_derived_cachename \"%s\"\n",
	   remote_name ? remote_name : "?"));

    if(!remote_name)
      return(ans);

    sprintf(tmp, ".ab%06u", phone_home_hash(remote_name) % 1000000);

    if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
	int to_copy;

	to_copy = (lc - ps_global->pinerc > 1)
		   ? (lc - ps_global->pinerc - 1) : 1;
	strncpy(dir, ps_global->pinerc, min(to_copy, sizeof(dir)-1));
	dir[min(to_copy, sizeof(dir)-1)] = '\0';
    }
    else{
	dir[0] = '.';
	dir[1] = '\0';
    }

    build_path(path, dir, tmp, sizeof(path));

    if(can_access(dir, EDIT_ACCESS) != 0)
      return(ans);

    do {
	/*
	 * Check that we can read and write the file and that if it
	 * has something in it already, it is a pinerc. If it doesn't
	 * exist yet, that's ok. Don't overwrite non-pinerc files.
	 */
	if(can_access(path, ACCESS_EXISTS) == 0 &&
	   (can_access(path, EDIT_ACCESS) != 0 ||
	    ((contents = read_file(path)) && !srchstr(contents, METASTR)))){

	    /* change last char of path and try again */
	    p = tmp + (strlen(tmp) - 1);
	    do {
		*p = (*p + 1) == 128 ? '0' : (*p + 1);
	    }while(!(isascii(*p) && isalnum(*p) && *p != '0'));
	}
	else
	  ans = cpystr(path);

	if(contents)
	  fs_give((void **)&contents);

    }while(ans == NULL && *p != '0');


    return(ans);
}


/*
 * Write out the contents of the metadata file.
 *
 * Each line should be: folder_name cache_file uidvalidity uidnext uid nmsgs
 *							     read_status date
 *
 * If delete_it is set, then remove the line instead of updating it.
 *
 * We have to be careful with the metadata, because it exists in user's
 * existing metadata files now, and it is oriented towards only RemImap.
 * We would have to change the version number and the format of the lines
 * in the file if we add another type.
 */
void
rd_write_metadata(rd, delete_it)
    REMDATA_S *rd;
    int        delete_it;
{
    char *tempfile;
    FILE *fp_old = NULL, *fp_new = NULL;
    char *p, *pinerc_dir = NULL, *metafile = NULL;
    char *rel_filename, *key;
    char  line[MAILTMPLEN];
    int   fd;
    struct variable *vars = ps_global->vars;

    dprint(7, (debugfile, "rd_write_metadata \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd || rd->flags & NO_META_UPDATE ||
       (rd->flags & DERIVE_CACHE &&
        !(VAR_REMOTE_ABOOK_METADATA && VAR_REMOTE_ABOOK_METADATA[0])))
      return;

    if(rd->type != RemImap){
	q_status_message(SM_ORDER, 3, 5,
			 "rd_write_metadata: type not supported");
	return;
    }

    dprint(9, (debugfile, " - rd_write_metadata: rn=%s lf=%s\n",
	   rd->rn ? rd->rn : "?", rd->lf ? rd->lf : "?"));

    key = rd->rn;

    if(!(metafile = rd_metadata_name()))
      goto io_err;

    if(!(tempfile = tempfile_in_same_dir(metafile, "a9", &pinerc_dir)))
      goto io_err;

    if((fd = open(tempfile, O_TRUNC|O_WRONLY|O_CREAT, 0600)) >= 0)
      fp_new = fdopen(fd, "w");
    
    if(pinerc_dir && rd->lf && strlen(rd->lf) > strlen(pinerc_dir))
      rel_filename = rd->lf + strlen(pinerc_dir) + 1;
    else
      rel_filename = rd->lf;

    if(pinerc_dir)
      fs_give((void **)&pinerc_dir);

    fp_old = fopen(metafile, "r");

    if(fp_new && fp_old){
	/*
	 * Write the header line.
	 */
	if(fprintf(fp_new, "%s %s Pine Metadata\n",
		   PMAGIC, METAFILE_VERSION_NUM) == EOF)
	  goto io_err;

	while((p = fgets(line, sizeof(line), fp_old)) != NULL){
	    /*
	     * Skip the header line and any lines that don't begin
	     * with a "{".
	     */
	    if(line[0] != '{')
	      continue;

	    /* skip the old cache line for this key */
	    if(strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == TAB)
	      continue;
	    
	    /* add this line to new version of file */
	    if(fputs(p, fp_new) == EOF)
	      goto io_err;
	}
    }

    /* add the cache line for this key */
    /* Warning: this is type RemImap specific right now! */
    if(!delete_it){
	if(!tempfile ||
	   !fp_old ||
	   !fp_new ||
	   p ||
	   fprintf(fp_new, "%s\t%s\t%lu\t%lu\t%lu\t%lu\t%c\t%s\n",
			    key ? key : "",
			    rel_filename ? rel_filename : "",
			    rd->t.i.uidvalidity, rd->t.i.uidnext, rd->t.i.uid,
			    rd->t.i.chk_nmsgs,
			    rd->read_status ? rd->read_status : '?',
			    rd->t.i.chk_date ? rd->t.i.chk_date : "no-match")
				== EOF)
	  goto io_err;
    }

    if(fclose(fp_new) == EOF){
	fp_new = NULL;
	goto io_err;
    }

    if(fclose(fp_old) == EOF){
	fp_old = NULL;
	goto io_err;
    }

    if(rename_file(tempfile, metafile) < 0)
      goto io_err;

    if(tempfile)
      fs_give((void **)&tempfile);
    
    if(metafile)
      fs_give((void **)&metafile);
    
    return;

io_err:
    dprint(2,
     (debugfile, "io_err in rd_write_metadata(%s), tempfile=%s: %s\n",
      metafile ? metafile : "<NULL>", tempfile ? tempfile : "<NULL>",
      error_description(errno)));
    q_status_message2(SM_ORDER, 3, 5,
		    "Trouble updating metafile %.200s, continuing (%.200s)",
		    metafile ? metafile : "<NULL>", error_description(errno));
    if(tempfile){
	(void)unlink(tempfile);
	fs_give((void **)&tempfile);
    }
    if(metafile)
      fs_give((void **)&metafile);
    if(fp_old)
      (void)fclose(fp_old);
    if(fp_new)
      (void)fclose(fp_new);
}


void
rd_update_metadata(rd, date)
    REMDATA_S *rd;
    char      *date;
{
    if(!rd)
      return;

    dprint(9, (debugfile, " - rd_update_metadata: rn=%s lf=%s\n",
	   rd->rn ? rd->rn : "?", rd->lf ? rd->lf : "<none>"));

    switch(rd->type){
      case RemImap:
	if(rd->t.i.stream){
	    ps_global->noshow_error = 1;
	    rd_ping_stream(rd);
	    if(rd->t.i.stream){
		/*
		 * If nmsgs < 2 then something is wrong. Maybe it is just
		 * that we haven't been told about the messages we've
		 * appended ourselves. Try closing and re-opening the stream
		 * to see those.
		 */
		if(rd->t.i.stream->nmsgs < 2 ||
		   (rd->t.i.shouldbe_nmsgs &&
		    (rd->t.i.shouldbe_nmsgs != rd->t.i.stream->nmsgs))){
		    pine_mail_check(rd->t.i.stream);
		    if(rd->t.i.stream->nmsgs < 2 ||
		       (rd->t.i.shouldbe_nmsgs &&
			(rd->t.i.shouldbe_nmsgs != rd->t.i.stream->nmsgs))){
			rd_close_remote(rd);
			rd_open_remote(rd);
		    }
		}

		rd->t.i.chk_nmsgs = rd->t.i.stream ? rd->t.i.stream->nmsgs : 0L;

		/*
		 * If nmsgs < 2 something is wrong.
		 */
		if(rd->t.i.chk_nmsgs < 2){
		    rd->t.i.uidvalidity = 0L;
		    rd->t.i.uid = 0L;
		    rd->t.i.uidnext = 0L;
		}
		else{
		    rd->t.i.uidvalidity = rd->t.i.stream->uid_validity;
		    rd->t.i.uid = mail_uid(rd->t.i.stream,
					   rd->t.i.stream->nmsgs);
		    /*
		     * Uid_last is not always valid. If the last known uid is
		     * greater than uid_last, go with it instead (uid+1).
		     * If our guess is wrong (too low), the penalty is not
		     * harsh. When the uidnexts don't match we open the
		     * mailbox to check the uid of the actual last message.
		     * If it was a false hit then we adjust uidnext so it
		     * will be correct the next time through.
		     */
		    rd->t.i.uidnext = max(rd->t.i.stream->uid_last,rd->t.i.uid)
					+ 1;
		}
	    }

	    ps_global->noshow_error = 0;

	    /*
	     * Save the date so that we can check if it changed next time
	     * we go to write.
	     */
	    if(date){
		if(rd->t.i.chk_date)
		  fs_give((void **)&rd->t.i.chk_date);
		
		rd->t.i.chk_date = cpystr(date);
	    }

	    rd_write_metadata(rd, 0);
	}

	rd->t.i.shouldbe_nmsgs = 0;

	break;

      default:
	q_status_message(SM_ORDER, 3, 5,
			 "rd_update_metadata: type not supported");
	break;
    }
}


char *
rd_metadata_name()
{
    char        *p, *q, *metafile;
    char         path[MAXPATH], pinerc_dir[MAXPATH];
    struct variable *vars = ps_global->vars;

    dprint(9, (debugfile, "rd_metadata_name\n"));

    pinerc_dir[0] = '\0';
    if(ps_global->pinerc){
	char *prcn = ps_global->pinerc;
	char *lc;

	if((lc = last_cmpnt(prcn)) != NULL){
	    int to_copy;

	    to_copy = (lc - prcn > 1) ? (lc - prcn - 1) : 1;
	    strncpy(pinerc_dir, prcn, min(to_copy, sizeof(pinerc_dir)-1));
	    pinerc_dir[min(to_copy, sizeof(pinerc_dir)-1)] = '\0';
	}
	else{
	    pinerc_dir[0] = '.';
	    pinerc_dir[1] = '\0';
	}
    }

    /*
     * If there is no metadata file specified in the pinerc, create a filename.
     */
    if(!(VAR_REMOTE_ABOOK_METADATA && VAR_REMOTE_ABOOK_METADATA[0])){
	if(pinerc_dir[0] && (p = tempfile_in_same_dir(ps_global->pinerc,
						      meta_prefix, NULL))){
	    /* fill in the pinerc variable */
	    q = p + strlen(pinerc_dir) + 1;
	    set_variable(V_REMOTE_ABOOK_METADATA, q, 1, 0, Main);
	    dprint(2, (debugfile, "creating name for metadata file: %s\n",
		   q ? q : "?"));

	    /* something's broken, return NULL rab */
	    if(!VAR_REMOTE_ABOOK_METADATA || !VAR_REMOTE_ABOOK_METADATA[0]){
		(void)unlink(p);
		fs_give((void **)&p);
		return(NULL);
	    }

	    fs_give((void **)&p);
	}
	else{
	    q_status_message(SM_ORDER, 3, 5,
		"can't create metadata file in pinerc directory, continuing");
	    return(NULL);
	}
    }

    build_path(path, pinerc_dir ? pinerc_dir : NULL,
	       VAR_REMOTE_ABOOK_METADATA, sizeof(path));
    metafile = path;

    /*
     * If the metadata file doesn't exist, create it.
     */
    if(can_access(metafile, ACCESS_EXISTS) != 0){
	int fd;

	if((fd = open(metafile, O_CREAT|O_EXCL|O_WRONLY, 0600)) < 0){

	    set_variable(V_REMOTE_ABOOK_METADATA, NULL, 1, 0, Main);

	    q_status_message2(SM_ORDER, 3, 5,
		       "can't create cache file %.200s, continuing (%.200s)",
		       metafile, error_description(errno));
	    dprint(2, (debugfile, "can't create metafile %s: %s\n",
		       metafile ? metafile : "?", error_description(errno)));

	    return(NULL);
	}

	dprint(2, (debugfile, "created metadata file: %s\n",
	       metafile ? metafile : "?"));

	(void)close(fd);
    }

    return(cpystr(metafile));;
}


REMDATA_META_S *
rd_find_our_metadata(key, flags)
    char          *key;
    unsigned long *flags;
{
    char        *p, *q, *metafile;
    char         line[MAILTMPLEN];
    REMDATA_META_S *rab = NULL;
    FILE        *fp;

    dprint(9, (debugfile, "rd_find_our_metadata \"%s\"\n", key ? key : "?"));

    if(!(key && *key))
      return rab;

try_once_more:

    if((metafile = rd_metadata_name()) == NULL)
      return rab;

    /*
     * Open the metadata file and get some information out.
     */
    fp = fopen(metafile, "r");
    if(fp == NULL){
	q_status_message2(SM_ORDER, 3, 5,
		   "can't open metadata file %.200s, continuing (%.200s)",
		   metafile, error_description(errno));
	dprint(2, (debugfile,
		   "can't open existing metadata file %s: %s\n",
		   metafile ? metafile : "?", error_description(errno)));
	if(flags)
	  (*flags) |= NO_META_UPDATE;

	fs_give((void **)&metafile);

	return rab;
    }

    /*
     * If we make it to this point where we have opened the metadata file
     * we return a structure (possibly empty) instead of just a NULL pointer.
     */
    rab = (REMDATA_META_S *)fs_get(sizeof(*rab));
    memset(rab, 0, sizeof(*rab));

    /*
     * Check for header line. If it isn't there or is incorrect,
     * return with the empty rab. This call also positions the file pointer
     * past the header line.
     */
    if(rd_meta_is_broken(fp)){

	dprint(2, (debugfile,
		   "metadata file is broken, creating new one: %s\n",
		   metafile ? metafile : "?"));

	/* Make it size zero so we won't copy any bad stuff */
	if(fp_file_size(fp) != 0){
	    int fd;

	    (void)fclose(fp);
	    (void)unlink(metafile);

	    if((fd = open(metafile, O_TRUNC|O_WRONLY|O_CREAT, 0600)) < 0){
		q_status_message2(SM_ORDER, 3, 5,
		       "can't create metadata file %.200s, continuing (%.200s)",
			 metafile, error_description(errno));
		dprint(2, (debugfile,
			   "can't create metadata file %s: %s\n",
			   metafile ? metafile : "?",
			   error_description(errno)));
		fs_give((void **)&rab);
		fs_give((void **)&metafile);
		return NULL;
	    }

	    (void)close(fd);
	}
	else
	  (void)fclose(fp);

	fs_give((void **)&metafile);
	return(rab);
    }

    fs_give((void **)&metafile);

    /* Look for our line */
    while((p = fgets(line, sizeof(line), fp)) != NULL)
      if(strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == TAB)
	break;
    
#define SKIP_TO_TAB(p) do{while(*p && *p != TAB)p++;}while(0)

    /*
     * The line should be TAB-separated with fields:
     * folder_name cache_file uidvalidity uidnext uid nmsgs read_status date
     * This part is highly RemImap-specific right now.
     */
    if(p){				/* Found the line, parse it. */
      SKIP_TO_TAB(p);			/* skip to TAB following folder_name */
      if(*p == TAB){
	q = ++p;			/* q points to cache_file */
	SKIP_TO_TAB(p);			/* skip to TAB following cache_file */
	if(*p == TAB){
	  *p = '\0';
	  rab->local_cache_file = cpystr(q);
	  q = ++p;			/* q points to uidvalidity */
	  SKIP_TO_TAB(p);		/* skip to TAB following uidvalidity */
	  if(*p == TAB){
	    *p = '\0';
	    rab->uidvalidity = strtoul(q,(char **)NULL,10);
	    q = ++p;			/* q points to uidnext */
	    SKIP_TO_TAB(p);		/* skip to TAB following uidnext */
	    if(*p == TAB){
	      *p = '\0';
	      rab->uidnext = strtoul(q,(char **)NULL,10);
	      q = ++p;			/* q points to uid */
	      SKIP_TO_TAB(p);		/* skip to TAB following uid */
	      if(*p == TAB){
	        *p = '\0';
	        rab->uid = strtoul(q,(char **)NULL,10);
	        q = ++p;		/* q points to nmsgs */
	        SKIP_TO_TAB(p);		/* skip to TAB following nmsgs */
	        if(*p == TAB){
	          *p = '\0';
	          rab->nmsgs = strtoul(q,(char **)NULL,10);
	          q = ++p;		/* q points to read_status */
	          SKIP_TO_TAB(p);	/* skip to TAB following read_status */
	          if(*p == TAB){
	            *p = '\0';
	            rab->read_status = *q;  /* just a char, not whole string */
	            q = ++p;		/* q points to date */
		    while(*p && *p != '\n' && *p != '\r')  /* skip to newline */
		      p++;
		
		    *p = '\0';
		    rab->date = cpystr(q);
	          }
		}
	      }
	    }
	  }
	}
      }
    }

    (void)fclose(fp);
    return(rab);
}


/*
 * Returns: -1 if this doesn't look like a metafile or some error,
 *               or if it looks like a non-current metafile.
 *           0 if it looks like a current metafile.
 *
 * A side effect is that the file pointer will be pointing to the second
 * line if 0 is returned.
 */
int
rd_meta_is_broken(fp)
    FILE *fp;
{
    char buf[MAILTMPLEN];

    if(fp == (FILE *)NULL)
      return -1;

    if(fp_file_size(fp) <
		    (long)(SIZEOF_PMAGIC + SIZEOF_SPACE + SIZEOF_VERSION_NUM))
      return -1;
    
    if(fseek(fp, (long)TO_FIND_HDR_PMAGIC, 0))
      return -1;

    /* check for magic */
    if(fread(buf, sizeof(char), (unsigned)SIZEOF_PMAGIC, fp) != SIZEOF_PMAGIC)
      return -1;

    buf[SIZEOF_PMAGIC] = '\0';
    if(strcmp(buf, PMAGIC) != 0)
      return -1;

    /*
     * If we change to a new version, we may want to add code here to convert
     * or to cleanup after the old version. For example, it might just
     * remove the old cache files here.
     */

    /* check for matching version number */
    if(fseek(fp, (long)TO_FIND_VERSION_NUM, 0))
      return -1;
    
    if(fread(buf, sizeof(char), (unsigned)SIZEOF_VERSION_NUM, fp) !=
       SIZEOF_VERSION_NUM)
      return -1;
    
    buf[SIZEOF_VERSION_NUM] = '\0';
    if(strcmp(buf, METAFILE_VERSION_NUM) != 0)
      return -1;

    /* Position file pointer to second line */
    if(fseek(fp, (long)TO_FIND_HDR_PMAGIC, 0))
      return -1;
    
    if(fgets(buf, sizeof(buf), fp) == NULL)
      return -1;

    return 0;
}


/*
 * Open a data stream to the remote data.
 */
void
rd_open_remote(rd)
    REMDATA_S *rd;
{
    long openmode = SP_USEPOOL | SP_TEMPUSE;

    dprint(7, (debugfile, "rd_open_remote \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd)
      return;
    
    if(rd_stream_exists(rd)){
	rd->last_use = get_adj_time();
	return;
    }

    switch(rd->type){
      case RemImap:

	if(rd->access == ReadOnly)
	  openmode |= OP_READONLY;

	ps_global->noshow_error = 1;
	rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode, NULL);
	ps_global->noshow_error = 0;

        /* Don't try to reopen if there was a problem (auth failure, etc.) */
	if(!rd->t.i.stream)  
	  rd->flags |= USER_SAID_NO; /* Caution: overloading USER_SAID_NO */
	
	if(rd->t.i.stream && rd->t.i.stream->halfopen){
	    /* this is a failure */
	    rd_close_remote(rd);
	}

	if(rd->t.i.stream)
	  rd->last_use = get_adj_time();

	break;

      default:
	q_status_message(SM_ORDER, 3, 5, "rd_open_remote: type not supported");
	break;
    }
}


/*
 * Close a data stream to the remote data.
 */
void
rd_close_remote(rd)
    REMDATA_S *rd;
{
    if(!rd || !rd_stream_exists(rd))
      return;

    switch(rd->type){
      case RemImap:
	ps_global->noshow_error = 1;
	pine_mail_close(rd->t.i.stream);
	rd->t.i.stream = NULL;
	ps_global->noshow_error = 0;
	break;

      default:
	q_status_message(SM_ORDER, 3, 5, "rd_close_remote: type not supported");
	break;
    }
}


int
rd_stream_exists(rd)
    REMDATA_S *rd;
{
    if(!rd)
      return(0);

    switch(rd->type){
      case RemImap:
	return(rd->t.i.stream ? 1 : 0);

      default:
	q_status_message(SM_ORDER, 3,5, "rd_stream_exists: type not supported");
	break;
    }
    
    return(0);
}


/*
 * Ping the stream and return 1 if it is still alive.
 */
int
rd_ping_stream(rd)
    REMDATA_S *rd;
{
    int ret = 0;

    dprint(7, (debugfile, "rd_ping_stream \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd)
      return(ret);

    if(!rd_stream_exists(rd)){
	switch(rd->type){
	  case RemImap:
	    /*
	     * If this stream is already actually open, officially open
	     * it and use it.
	     */
	    if(sp_stream_get(rd->rn, SP_MATCH)){
		long openflags = SP_USEPOOL | SP_TEMPUSE;

		if(rd->access == ReadOnly)
		  openflags |= OP_READONLY;

		rd->t.i.stream = pine_mail_open(NULL, rd->rn, openflags, NULL);
	    }

	    break;

	  default:
	    break;
	}
    }

    if(!rd_stream_exists(rd))
      return(ret);

    switch(rd->type){
      case RemImap:
	ps_global->noshow_error = 1;
	if(pine_mail_ping(rd->t.i.stream))
	  ret = 1;
	else
	  rd->t.i.stream = NULL;

	ps_global->noshow_error = 0;
	break;

      default:
	q_status_message(SM_ORDER, 3, 5, "rd_ping_stream: type not supported");
	break;
    }
    
    return(ret);
}


/*
 * Change readonly access to readwrite if appropriate. Call this if
 * the remote data ought to be readwrite but it is marked readonly in
 * the metadata.
 */
void
rd_check_readonly_access(rd)
    REMDATA_S *rd;
{
    if(rd && rd->read_status == 'R'){
	switch(rd->type){
	  case RemImap:
	    rd_open_remote(rd);
	    if(rd->t.i.stream && !rd->t.i.stream->rdonly){
		rd->read_status = 'W';
		rd->access = ReadWrite;
	    }

	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 5,
			     "rd_check_readonly_access: type not supported");
	    break;
	}
    }
}


/*
 * Initialize remote data. The remote data is initialized
 * with no data. On success, the local file will be empty and the remote
 * folder (if type RemImap) will contain a header message and an empty
 * data message.
 * The remote folder already exists before we get here, though it may be empty.
 *
 * Returns -1 on failure
 *          0 on success
 */
int
rd_init_remote(rd, add_only_first_msg)
    REMDATA_S *rd;
    int        add_only_first_msg;
{
    int  err = 0;
    char date[200];

    dprint(7, (debugfile, "rd_init_remote \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(rd && rd->type != RemImap){
	dprint(1, (debugfile, "rd_init_remote: type not supported\n"));
	return -1;
    }

    /*
     * The rest is currently type RemImap-specific. 
     */

    if(!(rd->flags & NO_FILE || rd->lf) ||
       !rd->rn || !rd->so || !rd->t.i.stream ||
       !rd->t.i.special_hdr){
	dprint(1, (debugfile,
	       "rd_init_remote: Unexpected error: %s is NULL\n",
	       !(rd->flags & NO_FILE || rd->lf) ? "localfile" :
		!rd->rn ? "remotename" :
		 !rd->so ? "so" :
		  !rd->t.i.stream ? "stream" :
		   !rd->t.i.special_hdr ? "special_hdr" : "?"));
	return -1;
    }

    /* already init'd */
    if(rd->t.i.stream->nmsgs >= 2 ||
       (rd->t.i.stream->nmsgs >= 1 && add_only_first_msg))
      return err;

    dprint(7, (debugfile, " - rd_init_remote(%s): %s\n",
	   rd->t.i.special_hdr ? rd->t.i.special_hdr : "?",
	   rd->rn ? rd->rn : "?"));
    
    /*
     * We want to protect the user who specifies an actual address book
     * file on the remote system as a remote address book. For example,
     * they might say address-book={host}.addressbook where .addressbook
     * is just a file on host. Remote address books are folders, not
     * files.  We also want to protect the user who specifies an existing
     * mail folder as a remote address book. We do that by requiring the
     * first message in the folder to be our header message.
     */

    if(rd->t.i.stream->rdonly){
	q_status_message1(SM_ORDER | SM_DING, 7, 10,
		 "Can't initialize folder \"%.200s\" (write permission)", rd->rn);
	if(rd->t.i.stream->nmsgs > 0)
	  q_status_message(SM_ORDER | SM_DING, 7, 10,
	   "Choose a new, unused folder for the remote data");

	dprint(1, (debugfile,
	       "Can't initialize remote folder \"%s\"\n",
	       rd->rn ? rd->rn : "?"));
	dprint(1, (debugfile,
	       "   No write permission for that remote folder.\n"));
	dprint(1, (debugfile,
		   "   Choose a new unused folder for the remote data.\n"));
	err = -1;
    }

    if(!err){
	if(rd->t.i.stream->nmsgs == 0){
	    int we_cancel;

	    we_cancel = busy_alarm(1, "Initializing remote data", NULL, 0);
	    rfc822_date(date);
	    /*
	     * The first message in a remote data folder is a special
	     * header message. It is there as a way to explain what the
	     * folder is to users who open it trying to read it. It is also
	     * used as a consistency check so that we don't use a folder
	     * that was being used for something else as a remote
	     * data folder.
	     */
	    err = rd_add_hdr_msg(rd, date);
	    if(rd->t.i.stream->nmsgs == 0)
	      rd_ping_stream(rd);
	    if(rd->t.i.stream && rd->t.i.stream->nmsgs == 0)
	      pine_mail_check(rd->t.i.stream);

	    if(we_cancel)
	      cancel_busy_alarm(-1);
	}
	else{
	    char *eptr = NULL;

	    err = rd_chk_for_hdr_msg(&(rd->t.i.stream), rd, &eptr);
	    if(err){
		q_status_message1(SM_ORDER | SM_DING, 5, 5,
		     "\"%.200s\" has invalid format, can't initialize", rd->rn);

		dprint(1, (debugfile,
		       "Can't initialize remote data \"%s\"\n",
		       rd->rn ? rd->rn : "?"));

		if(eptr){
		    q_status_message1(SM_ORDER, 3, 5, "%.200s", eptr);
		    dprint(1, (debugfile, "%s\n", eptr ? eptr : "?"));
		    fs_give((void **)&eptr);
		}
	    }
	}
    }

    /*
     * Add the second (empty) message.
     */
    if(!err && !add_only_first_msg){
	char *tempfile = NULL;
	int   fd;

	if(rd->flags & NO_FILE){
	    if(so_truncate(rd->sonofile, 0L) == 0)
	      err = -1;
	}
	else{
	    if(!(tempfile = tempfile_in_same_dir(rd->lf, "a8", NULL))){
		q_status_message1(SM_ORDER | SM_DING, 3, 5,
				  "Error opening temporary file: %.200s",
				  error_description(errno));
		dprint(2, (debugfile, "init_remote: Error opening file: %s\n",
		       error_description(errno)));
		err = -1;
	    }

	    if(!err &&
	       (fd = open(tempfile, O_TRUNC|O_WRONLY|O_CREAT, 0600)) < 0){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  "Error opening temporary file %.200s: %.200s",
				  tempfile, error_description(errno));
		dprint(2, (debugfile,
		       "init_remote: Error opening temporary file: %s: %s\n",
		       tempfile ? tempfile : "?", error_description(errno)));
		(void)unlink(tempfile);
		err = -1;
	    }
	    else
	      (void)close(fd);

	    if(!err && rename_file(tempfile, rd->lf) < 0){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
			      "Error creating cache file %.200s: %.200s",
			      rd->lf, error_description(errno));
		(void)unlink(tempfile);
		err = -1;
	    }

	    if(tempfile)
	      fs_give((void **)&tempfile);
	}

	if(!err){
	    err = rd_update_remote(rd, date);
	    if(err){
		q_status_message1(SM_ORDER | SM_DING, 3, 5,
				  "Error copying to remote folder: %.200s",
				  error_description(errno));
		
		q_status_message(SM_ORDER | SM_DING, 5, 5,
				 "Creation of remote data folder failed");
	    }
	}
    }

    if(!err){
	rd_update_metadata(rd, date);
	/* turn off out of date flag */
	rd->flags &= ~REM_OUTOFDATE;
    }

    return(err ? -1 : 0);
}


/*
 * IMAP stream should already be open to a remote folder.
 * Check the first message in the folder to be sure it is the right
 * kind of message, not some message from some other folder.
 *
 * Returns 0 if ok, < 0 if invalid format.
 *
 */
int
rd_chk_for_hdr_msg(streamp, rd, errmsg)
    MAILSTREAM **streamp;
    REMDATA_S   *rd;
    char       **errmsg;
{
    char *fields[3], *values[3];
    char *h;
    int   tried_again = 0;
    int   ret;
    MAILSTREAM *st = NULL;

    fields[0] = rd->t.i.special_hdr;
    fields[1] = "received";
    fields[2] = NULL;

try_again:
    ret = -1;

    if(!streamp || !*streamp){
	dprint(1, (debugfile, "rd_chk_for_hdr_msg: stream is null\n"));
    }
    else if((*streamp)->nmsgs == 0){
	ret = -2;
	dprint(1, (debugfile,
		   "rd_chk_for_hdr_msg: stream has nmsgs=0, try a ping\n"));
	if(!pine_mail_ping(*streamp))
	  *streamp = NULL;

	if(*streamp && (*streamp)->nmsgs == 0){
	    dprint(1, (debugfile,
	       "rd_chk_for_hdr_msg: still looks like nmsgs=0, try a check\n"));
	    pine_mail_check(*streamp);
	}

	if(*streamp && (*streamp)->nmsgs == 0){
	    dprint(1, (debugfile,
	       "rd_chk_for_hdr_msg: still nmsgs=0, try re-opening stream\n"));

	    if(rd_stream_exists(rd))
	      rd_close_remote(rd);

	    rd_open_remote(rd);
	    if(rd_stream_exists(rd))
	      st = rd->t.i.stream;
	}

	if(!st)
	  st = *streamp;

	if(st && st->nmsgs == 0){
	    dprint(1, (debugfile,
		       "rd_chk_for_hdr_msg: can't see header message\n"));
	}
    }
    else
      st = *streamp;

    if(st && st->nmsgs != 0
       && (h=pine_fetchheader_lines(st, 1L, NULL, fields))){
	simple_header_parse(h, fields, values);
	ret = -3;
	if(values[1])
	  ret = -4;
	else if(values[0]){
	    rd->cookie = strtoul(values[0], (char **)NULL, 10);
	    if(rd->cookie == 0)
	      ret = -5;
	    else if(rd->cookie == 1){
		if(rd->flags & COOKIE_ONE_OK || tried_again)
		  ret = 0;
		else
		  ret = -6;
	    }
	    else if(rd->cookie > 1)
	      ret = 0;
	}

	if(values[0])
	  fs_give((void **)&values[0]);

	if(values[1])
	  fs_give((void **)&values[1]);

	fs_give((void **)&h);
    }
    
    
    if(ret && ret != -6 && errmsg){
	*errmsg = (char *)fs_get(500 * sizeof(char));
	(*errmsg)[0] = '\0';
    }

    if(ret == -1){
	/* null stream */
	if(errmsg)
	  sprintf(*errmsg, "Can't open remote address book \"%.100s\"", rd->rn);
    }
    else if(ret == -2){
	/* no messages in folder */
	if(errmsg)
	  sprintf(*errmsg,
		  "Error: no messages in remote address book \"%.100s\"!",
		  rd->rn);
    }
    else if(ret == -3){
	/* no cookie */
	if(errmsg)
	  sprintf(*errmsg,
		  "First msg in \"%.100s\" should have \"%.100s\" header",
		  rd->rn, rd->t.i.special_hdr);
    }
    else if(ret == -4){
	/* Received header */
	if(errmsg)
	  sprintf(*errmsg,
		  "Suspicious Received headers in first msg in \"%.100s\"",
		  rd->rn);
    }
    else if(ret == -5){

	/* cookie is 0 */

	/*
	 * This is a failure and should not happen, but we're not going to
	 * fail on this condition.
	 */
	dprint(1, (debugfile, "Unexpected value in \"%s\" header of \"%s\"\n",
	       rd->t.i.special_hdr ? rd->t.i.special_hdr : "?",
	       rd->rn ? rd->rn : "?"));
	ret = 0;
    }
    else if(ret == -6){
	dprint(1, (debugfile,
		   "rd_chk_for_hdr_msg: cookie is 1, try to upgrade it\n"));

	if(rd_remote_is_readonly(rd)){
	    dprint(1, (debugfile,
		   "rd_chk_for_hdr_msg:  can't upgrade, readonly\n"));
	    ret = 0;			/* stick with 1 */
	}
	else{
	    /* cookie is 1, upgrade it */
	    if(rd_upgrade_cookies(rd, st->nmsgs, 0) == 0){
		/* now check again */
		if(!tried_again){
		    tried_again++;
		    goto try_again;
		}
	    }

	    /*
	     * This is actually a failure but we've decided that this
	     * failure is ok.
	     */
	    ret = 0;
	}
    }

    if(errmsg && *errmsg)
      dprint(1, (debugfile, "rd_chk_for_hdr_msg: %s\n", *errmsg));

    return ret;
}


/*
 * For remote data, this adds the explanatory header
 * message to the remote IMAP folder.
 *
 * Args:    rd -- Remote data handle
 *        date -- The date string to use
 *
 * Result: 0 success
 *        -1 failure
 */
int
rd_add_hdr_msg(rd, date)
    REMDATA_S *rd;
    char      *date;
{
    int           err = 0;

    if(!rd|| rd->type != RemImap || !rd->rn || !rd->so || !rd->t.i.special_hdr){
	dprint(1, (debugfile,
	       "rd_add_hdr_msg: Unexpected error: %s is NULL\n",
	       !rd ? "rd" :
		!rd->rn ? "remotename" :
		 !rd->so ? "so" :
		  !rd->t.i.special_hdr ? "special_hdr" : "?"));
	return -1;
    }

    err = rd_store_fake_hdrs(rd, "Header Message for Remote Data",
			     "plain", date);

    /* Write the dummy message */
    if(!strucmp(rd->t.i.special_hdr, REMOTE_ABOOK_SUBTYPE)){
	if(!err && so_puts(rd->so,
	    "This folder contains a single Pine addressbook.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "This message is just an explanatory message.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The last message in the folder is the live addressbook data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The rest of the messages contain previous revisions of the addressbook data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "To restore a previous revision just delete and expunge all of the messages\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "which come after it.\015\012") == 0)
	  err = -1;
    }
    else if(!strucmp(rd->t.i.special_hdr, REMOTE_PINERC_SUBTYPE)){
	if(!err && so_puts(rd->so,
	    "This folder contains a Pine config file.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "This message is just an explanatory message.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The last message in the folder is the live config data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The rest of the messages contain previous revisions of the data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "To restore a previous revision just delete and expunge all of the messages\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "which come after it.\015\012") == 0)
	  err = -1;
    }
    else{
	if(!err && so_puts(rd->so,
	    "This folder contains remote Pine data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "This message is just an explanatory message.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The last message in the folder is the live data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The rest of the messages contain previous revisions of the data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "To restore a previous revision just delete and expunge all of the messages\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "which come after it.\015\012") == 0)
	  err = -1;
    }

    /* Take the message from "so" to the remote folder */
    if(!err){
	MAILSTREAM *st;

	if((st = rd->t.i.stream) != NULL)
	  rd->t.i.shouldbe_nmsgs = rd->t.i.stream->nmsgs + 1;
	else
	  st = adrbk_handy_stream(rd->rn);

	err = write_fcc(rd->rn, NULL, rd->so, st, "remote data", NULL) ? 0 : -1;
    }
    
    return err;
}


/*
 * Write some fake header lines into storage object so.
 *
 * Args: so    -- storage object to write into
 *     subject -- subject to put in header
 *     subtype -- subtype to put in header
 *     date    -- date to put in header
 */
int
rd_store_fake_hdrs(rd, subject, subtype, date)
    REMDATA_S *rd;
    char      *subject;
    char      *subtype;
    char      *date;
{
    ENVELOPE     *fake_env;
    BODY         *fake_body;
    ADDRESS      *fake_from;
    int           err = 0;
    char          vers[50], *p;
    unsigned long r = 0L;

    if(!rd|| rd->type != RemImap || !rd->so || !rd->t.i.special_hdr)
      return -1;
    
    fake_env = (ENVELOPE *)fs_get(sizeof(ENVELOPE));
    memset(fake_env, 0, sizeof(ENVELOPE));
    fake_body = (BODY *)fs_get(sizeof(BODY));
    memset(fake_body, 0, sizeof(BODY));
    fake_from = (ADDRESS *)fs_get(sizeof(ADDRESS));
    memset(fake_from, 0, sizeof(ADDRESS));

    fake_env->subject = cpystr(subject);
    fake_env->date = (unsigned char *) cpystr(date);
    fake_from->personal = cpystr("Pine Remote Data");
    fake_from->mailbox = cpystr("nobody");
    fake_from->host = cpystr("nowhere");
    fake_env->from = fake_from;
    fake_body->type = REMOTE_DATA_TYPE;
    fake_body->subtype = cpystr(subtype);

    p = tmp_20k_buf;
    *p = '\0';

    if(rd->cookie > 0)
      r = rd->cookie;

    if(!r){
	int i;

	for(i = 100; i > 0 && r < 1000000; i--)
	  r = random();
	
	if(r < 1000000)
	  r = 1712836L + getpid();
	
	rd->cookie = r;
    }

    sprintf(vers, "%ld", r);

    rfc822_header_line(&p, rd->t.i.special_hdr, fake_env, vers);
    rfc822_header(p+strlen(p), fake_env, fake_body);
    mail_free_envelope(&fake_env);
    mail_free_body(&fake_body);

    /* Write the fake headers */
    if(so_puts(rd->so, tmp_20k_buf) == 0)
      err = -1;
    
    return err;
}


/*
 * We have discovered that the data in the remote folder is suspicious.
 * In some cases it is just because it is from an old version of pine.
 * We have decided to update the data so that it won't look suspicious
 * next time.
 *
 * Args -- only_update_last  If set, that means to just add a new last message
 *                           by calling rd_update_remote. Don't create a new
 *                           header message and delete the old header message.
 *         nmsgs             Not used if only_update_last is set
 */
int
rd_upgrade_cookies(rd, nmsgs, only_update_last)
    REMDATA_S  *rd;
    long        nmsgs;
    int         only_update_last;
{
    char date[200];
    int  err = 0;

    /*
     * We need to copy the data from the last message, add a new header
     * message with a random cookie, add the data back in with the
     * new cookie, and delete the old messages.
     */

    /* get data */
    rd->flags |= COOKIE_ONE_OK;

    /*
     * The local copy may be newer than the remote copy. We don't want to
     * blast the local copy in that case. The BELIEVE_CACHE flag tells us
     * to not do the blasting.
     */
    if(rd->flags & BELIEVE_CACHE)
      rd->flags &= ~BELIEVE_CACHE;
    else{
	dprint(1, (debugfile, "rd_upgrade_cookies:  copy abook data\n"));
	err = rd_update_local(rd);
    }

    if(!err && !only_update_last){
	rd->cookie = 0;		/* causes new cookie to be generated */
	rfc822_date(date);
	dprint(1, (debugfile, "rd_upgrade_cookies:  add new hdr msg to end\n"));
	err = rd_add_hdr_msg(rd, date);
    }

    if(!err){
	dprint(1, (debugfile, "rd_upgrade_cookies:  copy back data\n"));
	err = rd_update_remote(rd, NULL);
    }

    rd->flags &= ~COOKIE_ONE_OK;

    if(!err && !only_update_last){
	char sequence[20];

	/*
	 * We've created a new header message and added a new copy of the
	 * data after it. Only problem is that the new copy will have used
	 * the original header message to get its cookie (== 1) from. We
	 * could have deleted the original messages before the last step
	 * to get it right but that would delete all copies of the data
	 * temporarily. Delete now and then re-update.
	 */
	rd_ping_stream(rd);
	rd_open_remote(rd);
	if(rd->t.i.stream && rd->t.i.stream->nmsgs >= nmsgs+2){
	    sprintf(sequence, "1:%ld", nmsgs);
	    mail_flag(rd->t.i.stream, sequence, "\\DELETED", ST_SET);
	    mail_expunge(rd->t.i.stream);
	    err = rd_update_remote(rd, NULL);
	}
    }

    return(err);
}


/*
 * Copy remote data to local file. If needed, the remote data is initialized
 * with no data. On success, the local file contains the remote data (which
 * means it will be empty if we initialized).
 *
 * Returns != 0 on failure
 *            0 on success
 */
int
rd_update_local(rd)
    REMDATA_S *rd;
{
    char     *error;
    STORE_S  *store;
    gf_io_t   pc;
    int       i, we_cancel = 0;
    BODY     *body = NULL;
    ENVELOPE *env;
    char     *tempfile = NULL;


    if(!rd || !(rd->flags & NO_FILE || rd->lf) || !rd->rn){
	dprint(1, (debugfile,
	       "rd_update_local: Unexpected error: %s is NULL\n",
	       !rd ? "rd" :
		!(rd->flags & NO_FILE || rd->lf) ? "localfile" :
		 !rd->rn ? "remotename" : "?"));

	return -1;
    }

    dprint(3, (debugfile, " - rd_update_local(%s): %s => %s\n",
	   rd->type == RemImap ? "Imap" : "?", rd->rn ? rd->rn : "?",
	   (rd->flags & NO_FILE) ? "<mem>" : (rd->lf ? rd->lf : "?")));
    
    switch(rd->type){
      case RemImap:
	if(!rd->so || !rd->t.i.special_hdr){
	    dprint(1, (debugfile,
		   "rd_update_local: Unexpected error: %s is NULL\n",
		   !rd->so ? "so" :
		    !rd->t.i.special_hdr ? "special_hdr" : "?"));
	    return -1;
	}

	rd_open_remote(rd);
	if(!rd_stream_exists(rd)){
	    if(rd->flags & NO_PERM_CACHE){
		dprint(1, (debugfile,
	     "rd_update_local: backtrack, remote folder does not exist yet\n"));
	    }
	    else{
		dprint(1, (debugfile,
		       "rd_update_local: Unexpected error: stream is NULL\n"));
	    }

	    return -1;
	}

	if(rd->t.i.stream){
	    char  ebuf[500];
	    char *eptr = NULL;
	    int   chk;

	    /* force ReadOnly */
	    if(rd->t.i.stream->rdonly){
		rd->read_status = 'R';
		rd->access = ReadOnly;
	    }
	    else
	      rd->read_status = 'W';

	    if(rd->t.i.stream->nmsgs < 2)
	      return(rd_init_remote(rd, 0));
	    else if(rd_chk_for_hdr_msg(&(rd->t.i.stream), rd, &eptr)){
		q_status_message1(SM_ORDER | SM_DING, 5, 5,
		     "Can't initialize \"%.200s\" (invalid format)", rd->rn);

		if(eptr){
		    q_status_message1(SM_ORDER, 3, 5, "%.200s", eptr);
		    dprint(1, (debugfile, "%s\n", eptr));
		    fs_give((void **)&eptr);
		}

		dprint(1, (debugfile,
		       "Can't initialize remote data \"%s\"\n",
		       rd->rn ? rd->rn : "?"));
		return -1;
	    }

	    we_cancel = busy_alarm(1, "Copying remote data", NULL, 0);

	    if(rd->flags & NO_FILE){
		store = rd->sonofile;
		so_truncate(store, 0L);
	    }
	    else{
		if(!(tempfile = tempfile_in_same_dir(rd->lf, "a8", NULL))){
		    q_status_message1(SM_ORDER | SM_DING, 3, 5,
				      "Error opening temporary file: %.200s",
				      error_description(errno));
		    dprint(2, (debugfile,
			  "rd_update_local: Error opening temporary file: %s\n",
			  error_description(errno)));
		    return -1;
		}

		/* Copy the data into tempfile */
		if((store = so_get(FileStar, tempfile, WRITE_ACCESS|OWNER_ONLY))
								    == NULL){
		    q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  "Error opening temporary file %.200s: %.200s",
				      tempfile, error_description(errno));
		    dprint(2, (debugfile,
		      "rd_update_local: Error opening temporary file: %s: %s\n",
		      tempfile ? tempfile : "?",
		      error_description(errno)));
		    (void)unlink(tempfile);
		    fs_give((void **)&tempfile);
		    return -1;
		}
	    }

	    /*
	     * Copy from the last message in the folder.
	     */
	    if(!pine_mail_fetchstructure(rd->t.i.stream, rd->t.i.stream->nmsgs,
					 &body)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
				 "Can't access remote IMAP data");
		dprint(2, (debugfile, "Can't access remote IMAP data\n"));
		if(tempfile){
		    (void)unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_alarm(-1);

		return -1;
	    }

	    if(!body ||
	       body->type != REMOTE_DATA_TYPE ||
	       !body->subtype ||
	       strucmp(body->subtype, rd->t.i.special_hdr)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
				 "Remote IMAP folder has wrong contents");
		dprint(2, (debugfile,
		       "Remote IMAP folder has wrong contents\n"));
		if(tempfile){
		    (void)unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_alarm(-1);

		return -1;
	    }
	    
	    if(!(env = pine_mail_fetchenvelope(rd->t.i.stream,
					       rd->t.i.stream->nmsgs))){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
				 "Can't access check date in remote data");
		dprint(2, (debugfile,
		       "Can't access check date in remote data\n"));
		if(tempfile){
		    (void)unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_alarm(-1);

		return -1;
	    }

	    if(rd && rd->flags & USER_SAID_YES)
	      chk = 0;
	    else
	      chk = rd_check_for_suspect_data(rd);

	    switch(chk){
	      case -1:		/* suspicious data, user says abort */
		if(tempfile){
		    (void)unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_alarm(-1);

		return -1;

	      case 1:		/* suspicious data, user says go ahead */
		if(rd_remote_is_readonly(rd)){
		    dprint(1, (debugfile,
			   "rd_update_local:  can't upgrade, readonly\n"));
		}
		else
		  /* attempt to upgrade cookie in last message */
		  (void)rd_upgrade_cookies(rd, 0, 1);

	        break;

	      case 0:		/* all is ok */
	      default:
	        break;
	    }


	    /* store may have been written to at this point, so we'll clear it out  */
	    so_truncate(store, 0L);

	    gf_set_so_writec(&pc, store);

	    error = detach(rd->t.i.stream, rd->t.i.stream->nmsgs, "1",
			   NULL, pc, NULL, 0);

	    gf_clear_so_writec(store);

	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    if(!(rd->flags & NO_FILE)){
		if(so_give(&store)){
		    sprintf(ebuf, "Error writing temp file: %.50s",
			    error_description(errno));
		    error = ebuf;
		}
	    }

	    if(error){
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
			      "%.200s: Error copying remote IMAP data", error);
		dprint(2, (debugfile, "rd_update_local: Error copying: %s\n",
		       error ? error : "?"));
		if(tempfile){
		    (void)unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		return -1;
	    }

	    if(tempfile && (i = rename_file(tempfile, rd->lf)) < 0){
#ifdef	_WINDOWS
		if(i == -5){
		    q_status_message2(SM_ORDER | SM_DING, 3, 4,
				    "Error updating local file: %.200s: %.200s",
				      rd->lf, error_description(errno));
		    q_status_message(SM_ORDER, 3, 4,
				 "Perhaps another process has the file open?");
		    dprint(2, (debugfile,
	"Rename_file(%s,%s): %s: returned -5, another process has file open?\n",
			   tempfile ? tempfile : "?",
			   rd->lf ? rd->lf : "?",
			   error_description(errno)));
		}
		else
#endif	/* _WINDOWS */
		{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  "Error updating cache file %.200s: %.200s",
				  rd->lf, error_description(errno));
		dprint(2, (debugfile,
		       "Error updating cache file %s: rename(%s,%s): %s\n",
		       tempfile ? tempfile : "?",
		       tempfile ? tempfile : "?",
		       rd->lf ? rd->lf : "?",
		       error_description(errno)));
		}

		(void)unlink(tempfile);
		fs_give((void **)&tempfile);
		return -1;
	    }

	    dprint(5, (debugfile,
		   "in rd_update_local, setting chk_date to ->%s<-\n",
		   env->date ? (char *)env->date : "?"));
	    rd_update_metadata(rd, env->date);

	    /* turn off out of date flag */
	    rd->flags &= ~REM_OUTOFDATE;

	    if(tempfile)
	      fs_give((void **)&tempfile);

	    return 0;
	}
	else{
	    q_status_message1(SM_ORDER | SM_DING, 5, 5,
		 "Can't open remote IMAP folder \"%.200s\"", rd->rn);
	    dprint(1, (debugfile,
		   "Can't open remote IMAP folder \"%s\"\n",
		   rd->rn ? rd->rn : "?"));
	    rd->access = ReadOnly;
	    return -1;
	}

	break;

      default:
	dprint(1, (debugfile, "rd_update_local: Unsupported type\n"));
	return -1;
    }
}


/*
 * Copy local data to remote folder.
 *
 * Args         lf -- Local file name
 *              rn -- Remote name
 * header_to_check -- Name of indicator header in remote folder
 *       imapstuff -- Structure holding info about connection
 *      returndate -- Pointer to the date that was stored in the remote folder
 *
 * Returns !=0 on failure
 *          0 on success
 */
int
rd_update_remote(rd, returndate)
    REMDATA_S *rd;
    char      *returndate;
{
    STORE_S    *store;
    int         err = 0;
    long        openmode = SP_USEPOOL | SP_TEMPUSE;
    MAILSTREAM *st;
    char       *eptr = NULL;

    if(rd && rd->type != RemImap){
	dprint(1, (debugfile, "rd_update_remote: type not supported\n"));
	return -1;
    }

    if(!rd || !(rd->flags & NO_FILE || rd->lf) || !rd->rn ||
       !rd->so || !rd->t.i.special_hdr){
	dprint(1, (debugfile,
	       "rd_update_remote: Unexpected error: %s is NULL\n",
	       !rd ? "rd" :
	        !(rd->flags & NO_FILE || rd->lf) ? "localfile" :
		 !rd->rn ? "remotename" :
		  !rd->so ? "so" :
		    !rd->t.i.special_hdr ? "special_hdr" : "?"));
	return -1;
    }

    dprint(7, (debugfile, " - rd_update_remote(%s): %s => %s\n",
	   rd->t.i.special_hdr ? rd->t.i.special_hdr : "?",
	   rd->lf ? rd->lf : "<mem>",
	   rd->rn ? rd->rn : "?"));

    if(!(st = rd->t.i.stream))
      st = adrbk_handy_stream(rd->rn);

    if(!st)
      st = rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode, NULL);

    if(!st){
	q_status_message1(SM_ORDER | SM_DING, 5, 5,
	     "Can't open \"%.200s\" for copying", rd->rn);
	dprint(1, (debugfile,
	  "rd_update_remote: Can't open remote folder \"%s\" for copying\n",
	       rd->rn ? rd->rn : "?"));
	return 1;
    }

    rd->last_use = get_adj_time();
    err = rd_chk_for_hdr_msg(&st, rd, &eptr);
    if(err){
	q_status_message1(SM_ORDER | SM_DING, 5, 5,
			  "\"%.200s\" has invalid format", rd->rn);

	if(eptr){
	    q_status_message1(SM_ORDER, 3, 5, "%.200s", eptr);
	    dprint(1, (debugfile, "%s\n", eptr));
	    fs_give((void **)&eptr);
	}

	dprint(1, (debugfile,
	   "rd_update_remote: \"%s\" has invalid format\n",
	   rd->rn ? rd->rn : "?"));
	return 1;
    }

    errno = 0;

    /*
     * The data that will be going to the remote folder rn is
     * written into the following storage object and then copied to
     * the remote folder from there.
     */

    if(rd->flags & NO_FILE){
	store = rd->sonofile;
	if(store)
	  so_seek(store, 0L, 0);	/* rewind */
    }
    else
      store = so_get(FileStar, rd->lf, READ_ACCESS);

    if(store != NULL){
	char date[200];
	unsigned char c;
	unsigned char last_c = 0;

	/* Reset the storage object, since we may have already used it. */
	if(so_truncate(rd->so, 0L) == 0)
	  err = 1;

	rfc822_date(date);
	dprint(7, (debugfile,
	       "in rd_update_remote, storing date ->%s<-\n",
	       date ? date : "?"));
	if(!err && rd_store_fake_hdrs(rd, "Pine Remote Data Container",
				      rd->t.i.special_hdr, date))
	  err = 1;
	
	/* save the date for later comparisons */
	if(!err && returndate)
	  strncpy(returndate, date, 100);

	/* Write the data */
	while(!err && so_readc(&c, store)){
	    /*
	     * C-client expects CRLF-terminated lines. Convert them
	     * as we copy into c-client. Read ahead isn't available.
	     * Leave CRLF as is, convert LF to CRLF, leave CR as is.
	     */
	    if(last_c != '\r' && c == '\n'){
		/* Convert \n to CRFL */
		if(so_writec('\r', rd->so) == 0 || so_writec('\n', rd->so) == 0)
		  err = 1;
		
		last_c = 0;
	    }
	    else{
		last_c = c;
		if(so_writec((int) c, rd->so) == 0)
		  err = 1;
	    }
	}

	/*
	 * Take that message from so to the remote folder.
	 * We append to that folder and always
	 * use the final message as the active data.
	 */
	if(!err){
	    MAILSTREAM *st;

	    if((st = rd->t.i.stream) != NULL)
	      rd->t.i.shouldbe_nmsgs = rd->t.i.stream->nmsgs + 1;
	    else
	      st = adrbk_handy_stream(rd->rn);

	    err = write_fcc(rd->rn, NULL, rd->so, st,
			    "remote data", NULL) ? 0 : 1;
	    /*
	     * This flag keeps us from trying to trim the remote data
	     * when we haven't even written to it this session. It may
	     * be that the remote data needs trimming, but if we didn't
	     * make it worse than let's just leave it instead of going
	     * to the trouble of opening it in order to check.
	     */
	    if(!err)
	      rd->flags |= REMUPDATE_DONE;
	}


	if(!(rd->flags & NO_FILE))
	  so_give(&store);
    }
    else
      err = -1;

    if(err)
	dprint(2, (debugfile, "error in rd_update_remote for %s => %s\n",
	       rd->lf ? rd->lf : "<mem>", rd->rn ? rd->rn : "?"));

    return(err);
}


/*
 * Check to see if the remote data has changed since we cached it.
 *
 * Args    rd -- REMDATA handle
 *  do_it_now -- If > 0, check now regardless
 *               If = 0, check if time since last chk more than default
 *               If < 0, check if time since last chk more than -do_it_now
 */
void
rd_check_remvalid(rd, do_it_now)
    REMDATA_S *rd;
    long       do_it_now;
{
    time_t      chk_interval;
    long        openmode = SP_USEPOOL | SP_TEMPUSE;
    MAILSTREAM *stat_stream = NULL;
    int         we_cancel = 0, got_cmsgs = 0;
    extern MAILSTATUS mm_status_result;
    unsigned long current_nmsgs;

    dprint(7, (debugfile, "- rd_check_remvalid(%s) -\n",
	   (rd && rd->rn) ? rd->rn : ""));

    if(rd && rd->type != RemImap){
	dprint(1, (debugfile, "rd_check_remvalid: type not supported\n"));
	return;
    }

    if(!rd || rd->flags & REM_OUTOFDATE || rd->flags & USE_OLD_CACHE)
      return;

    if(!rd->t.i.chk_date){
	dprint(2, (debugfile,
	       "rd_check_remvalid: remote data %s changed (chk_date)\n",
	       rd->rn));
	rd->flags |= REM_OUTOFDATE;
	return;
    }

    if(rd->t.i.chk_nmsgs <= 1){
	dprint(2, (debugfile,
	  "rd_check_remvalid: remote data %s changed (chk_nmsgs <= 1)\n",
	       rd->rn ? rd->rn : "?"));
	rd->flags |= REM_OUTOFDATE;
	return;
    }

    if(do_it_now < 0L){
	chk_interval = -1L * do_it_now;
	do_it_now = 0L;
    }
    else
      chk_interval = ps_global->remote_abook_validity * 60L;

    /* too soon to check again */
    if(!do_it_now &&
       (chk_interval == 0L ||
        get_adj_time() <= rd->last_valid_chk + chk_interval))
      return;
    
    if(rd->access == ReadOnly)
      openmode |= OP_READONLY;

    rd->last_valid_chk = get_adj_time();
    mm_status_result.flags  = 0L;

    /* make sure the cache file is still there */
    if(rd->lf && can_access(rd->lf, READ_ACCESS) != 0){
	dprint(2, (debugfile,
	       "rd_check_remvalid: %s: cache file %s disappeared\n",
	       rd->rn ? rd->rn : "?",
	       rd->lf ? rd->lf : "?"));
	rd->flags |= REM_OUTOFDATE;
	return;
    }

    /*
     * If the stream is open we should check there instead of using
     * a STATUS command. Check to see if it is really still alive with the
     * ping. It would be convenient if we could use a status command
     * on the open stream but apparently that won't work everywhere.
     */
    rd_ping_stream(rd);

try_looking_in_stream:

    /*
     * Get the current number of messages in the folder to
     * compare with our saved number of messages.
     */
    current_nmsgs = 0;
    if(rd->t.i.stream){
	dprint(7, (debugfile, "using open remote data stream\n"));
	rd->last_use = get_adj_time();
	current_nmsgs = rd->t.i.stream->nmsgs;
	got_cmsgs++;
    }
    else{

	/*
	 * Try to use the imap status command
	 * to get the information as cheaply as possible.
	 * If NO_STATUSCMD is set we just bypass all this stuff.
	 */

	if(!(rd->flags & NO_STATUSCMD))
	  stat_stream = adrbk_handy_stream(rd->rn);

	/*
	 * If we don't have a stream to use for the status command we
	 * skip it and open the folder instead. Then, next time we want to
	 * check we'll probably be able to use that open stream instead of
	 * opening another one each time for the status command.
	 */
	if(stat_stream){
	    if(!LEVELSTATUS(stat_stream)){
		rd->flags |= NO_STATUSCMD;
		dprint(2, (debugfile,
   "rd_check_remvalid: remote data %s: server doesn't support status\n",
		       rd->rn ? rd->rn : "?"));
	    }
	    else{
		/*
		 * This sure seems like a crock. We have to check to
		 * see if the stream is actually open to the folder
		 * we want to do the status on because c-client can't
		 * do a status on an open folder. In this case, we fake
		 * the status command results ourselves.
		 * If we're so unlucky as to get back a stream that will
		 * work for the status command while we also have another
		 * stream that is rd->rn and we don't pick up on that,
		 * too bad.
		 */
		if(same_stream_and_mailbox(rd->rn, stat_stream)){
		    dprint(7, (debugfile,
			   "rd_check_remvalid: faking status\n"));
		    mm_status_result.flags = SA_MESSAGES | SA_UIDVALIDITY
					     | SA_UIDNEXT;
		    mm_status_result.messages = stat_stream->nmsgs;
		    mm_status_result.uidvalidity = stat_stream->uid_validity;
		    mm_status_result.uidnext = stat_stream->uid_last+1;
		}
		else{

		    dprint(7, (debugfile,
			   "rd_check_remvalid: trying status\n"));
		    ps_global->noshow_error = 1;
		    if(!pine_mail_status(stat_stream, rd->rn,
				    SA_UIDVALIDITY | SA_UIDNEXT | SA_MESSAGES)){
			/* failed, mark it so we won't try again */
			rd->flags |= NO_STATUSCMD;
			dprint(2, (debugfile,
		   "rd_check_remvalid: addrbook %s: status command failed\n",
		   rd->rn ? rd->rn : "?"));
			mm_status_result.flags = 0L;
		    }
		}

		ps_global->noshow_error = 0;
	    }
	}

	/* if we got back a result from the status command, use it */
	if(mm_status_result.flags){
	    dprint(7, (debugfile,
		   "rd_check_remvalid: got status_result 0x%x\n",
		   mm_status_result.flags));
	    if(mm_status_result.flags & SA_MESSAGES){
		current_nmsgs = mm_status_result.messages;
		got_cmsgs++;
	    }
	}
    }

    /*
     * Check current_nmsgs versus what we think it should be.
     * If they're different we know things have changed and we can
     * return now. If they're the same we don't know.
     */
    if(got_cmsgs && current_nmsgs != rd->t.i.chk_nmsgs){
	rd->flags |= REM_OUTOFDATE;
	dprint(2, (debugfile,
	       "rd_check_remvalid: remote data %s changed (current msgs (%ld) != chk_nmsgs (%ld))\n",
	       rd->rn ? rd->rn : "?", current_nmsgs, rd->t.i.chk_nmsgs));
	return;
    }
    
    /*
     * Get the current uidvalidity and uidnext values from the
     * folder to compare with our saved values.
     */
    if(rd->t.i.stream){
	if(rd->t.i.stream->uid_validity == rd->t.i.uidvalidity){
	    /*
	     * Uid is valid so we just have to check whether or not the
	     * uid of the last message has changed or not and return.
	     */
	    if(mail_uid(rd->t.i.stream, rd->t.i.stream->nmsgs) != rd->t.i.uid){
		/* uid has changed so we're out of date */
		rd->flags |= REM_OUTOFDATE;
		dprint(2, (debugfile,
		     "rd_check_remvalid: remote data %s changed (uid)\n",
		     rd->rn ? rd->rn : "?"));
	    }
	    else{
		dprint(7,(debugfile,"rd_check_remvalid: uids match\n"));
	    }

	    return;
	}
	else{
	    /*
	     * If the uidvalidity changed that probably means it can't
	     * be relied on to be meaningful, so don't use it in the future.
	     */
	    rd->flags |= NO_STATUSCMD;
	    dprint(7, (debugfile,
       "rd_check_remvalid: remote data %s uidvalidity changed, don't use uid\n",
		   rd->rn ? rd->rn : "?"));
	}
    }
    else{			/* stream not open, use status results */
	if(mm_status_result.flags & SA_UIDVALIDITY &&
	   mm_status_result.flags & SA_UIDNEXT &&
	   mm_status_result.uidvalidity == rd->t.i.uidvalidity){

	    /*
	     * Uids are valid.
	     */

	    if(mm_status_result.uidnext == rd->t.i.uidnext){
		/*
		 * Uidnext valid and unchanged, so the folder is unchanged.
		 */
		dprint(7, (debugfile, "rd_check_remvalid: uidnexts match\n"));
		return;
	    }
	    else{	/* uidnext changed, folder _may_ have changed */

		dprint(7, (debugfile,
   "rd_check_remvalid: uidnexts don't match, ours=%lu status=%lu\n",
		   rd->t.i.uidnext, mm_status_result.uidnext));

		/*
		 * Since c-client can't handle a status cmd on the selected
		 * mailbox, we may have had to guess at the value of uidnext,
		 * and we don't know for sure that this is a real change.
		 * We need to open the mailbox and find out for sure.
		 */
		we_cancel = busy_alarm(1, NULL, NULL, 0);
		ps_global->noshow_error = 1;
		if(rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode,
						 NULL)){
		    unsigned long last_msg_uid;

		    if(rd->t.i.stream->rdonly)
		      rd->read_status = 'R';

		    last_msg_uid = mail_uid(rd->t.i.stream,
					    rd->t.i.stream->nmsgs);
		    ps_global->noshow_error = 0;
		    rd->last_use = get_adj_time();
		    dprint(7, (debugfile,
			   "%s: opened to check uid (%ld)\n",
			   rd->rn ? rd->rn : "?", (long)rd->last_use));
		    if(last_msg_uid != rd->t.i.uid){	/* really did change */
			rd->flags |= REM_OUTOFDATE;
			dprint(2, (debugfile,
 "rd_check_remvalid: remote data %s changed, our uid=%lu real uid=%lu\n",
			       rd->rn ? rd->rn : "?",
			       rd->t.i.uid, last_msg_uid));
		    }
		    else{				/* false hit */
			/*
			 * The uid of the last message is the same as we
			 * remembered, so the problem is that our guess
			 * for the nextuid was wrong. It didn't actually
			 * change. Since we know the correct uidnext now we
			 * can reset that guess to the correct value for
			 * next time, avoiding this extra mail_open.
			 */
			dprint(2, (debugfile,
			       "rd_check_remvalid: remote data %s false change: adjusting uidnext from %lu to %lu\n",
			       rd->rn ? rd->rn : "?",
			       rd->t.i.uidnext,
			       mm_status_result.uidnext));
			rd->t.i.uidnext = mm_status_result.uidnext;
			rd_write_metadata(rd, 0);
		    }

		    if(we_cancel)
		      cancel_busy_alarm(-1);

		    return;
		}
		else{
		    ps_global->noshow_error = 0;
		    dprint(2, (debugfile,
			   "rd_check_remvalid: couldn't open %s\n",
			   rd->rn ? rd->rn : "?"));
		}
	    }
	}
	else{
	    /*
	     * If the status command doesn't return these or
	     * if the uidvalidity is changing that probably means it can't
	     * be relied on to be meaningful, so don't use it.
	     *
	     * We also come here if we don't have a stat_stream handy to
	     * look up the status. This happens, for example, if our
	     * address book is on a different server from the open mail
	     * folders. In that case, instead of opening a stream,
	     * doing a status command, and closing the stream, we open
	     * the stream and use it to check next time, too.
	     */
	    if(stat_stream){
		rd->flags |= NO_STATUSCMD;
		dprint(7, (debugfile,
		  "rd_check_remvalid: remote data %s don't use status\n",
		   rd->rn ? rd->rn : "?"));
	    }

	    dprint(7, (debugfile,
		   "opening remote data stream for validity check\n"));
	    we_cancel = busy_alarm(1, NULL, NULL, 0);
	    ps_global->noshow_error = 1;
	    rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode,
					  NULL);
	    ps_global->noshow_error = 0;
	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    we_cancel = 0;
	    if(rd->t.i.stream)
	      goto try_looking_in_stream;
	    else{
		dprint(7, (debugfile,
		  "rd_check_remvalid: cannot open remote mailbox\n"));
		return;
	    }
	}
    }

    /*
     * If we got here that means that we still don't know if the folder
     * changed or not. If the stream is open then it must be the case that
     * uidvalidity changed so we can't rely on using uids.  If the stream
     * isn't open, then either the status command didn't work or the
     * uidvalidity changed. In any case, we need to fall back to looking
     * inside the folder at the last message and checking whether or not the
     * Date of the last message is the one we remembered.
     */

    dprint(7, (debugfile,
	   "rd_check_remvalid: falling back to Date check\n"));

    /*
     * Fall back to looking in the folder at the Date header.
     */

    if(!rd->t.i.stream)
      we_cancel = busy_alarm(1, NULL, NULL, 0);

    ps_global->noshow_error = 1;
    if(rd->t.i.stream ||
       (rd->t.i.stream = context_open(NULL,NULL,rd->rn,openmode,NULL))){
	ENVELOPE *env = NULL;

	if(rd->t.i.stream->rdonly)
	  rd->read_status = 'R';

	if(rd->t.i.stream->nmsgs != rd->t.i.chk_nmsgs){
	    rd->flags |= REM_OUTOFDATE;
	    dprint(2, (debugfile,
   "rd_check_remvalid: remote data %s changed (expected nmsgs %ld, got %ld)\n",
		   rd->rn ? rd->rn : "?",
		   rd->t.i.chk_nmsgs, rd->t.i.stream->nmsgs));
	}
	else if(rd->t.i.stream->nmsgs > 1){
	    env = pine_mail_fetchenvelope(rd->t.i.stream,rd->t.i.stream->nmsgs);
	
	    if(!env || (env->date && strucmp(env->date, rd->t.i.chk_date))){
		rd->flags |= REM_OUTOFDATE;
		dprint(2, (debugfile,
		      "rd_check_remvalid: remote data %s changed (%s)\n",
		      rd->rn ? rd->rn : "?", env ? "date" : "not enough msgs"));
	    }
	}

	rd->last_use = get_adj_time();
	if(env)
	  dprint(7, (debugfile,
	         "%s: got envelope to check date (%ld)\n",
	         rd->rn ? rd->rn : "?", (long)rd->last_use));
    }
    /* else, we give up and go with the cache copy */

    ps_global->noshow_error = 0;

    if(we_cancel)
      cancel_busy_alarm(-1);

    dprint(7, (debugfile, "rd_check_remvalid: falling off end\n"));
}


/*
 * Returns nonzero if remote data is currently readonly.
 *
 * Returns 0 -- apparently writeable
 *         1 -- stream not open
 *         2 -- stream open but not writeable
 */
int
rd_remote_is_readonly(rd)
    REMDATA_S *rd;
{
    if(!rd || rd->access == ReadOnly || !rd_stream_exists(rd))
      return(1);
    
    switch(rd->type){
      case RemImap:
	if(rd->t.i.stream->rdonly)
	  return(2);
	
	break;

      default:
	q_status_message(SM_ORDER, 3, 5,
			 "rd_remote_is_readonly: type not supported");
	break;
    }

    return(0);
}


/*
 * Returns  0 if ok
 *         -1 if not ok and user says No
 *          1 if not ok but user says Yes, ok to use it
 */
int
rd_check_for_suspect_data(rd)
    REMDATA_S *rd;
{
    int           ans = -1;
    char         *fields[3], *values[3], *h;
    unsigned long cookie;

    if(!rd || rd->type != RemImap || !rd->so || !rd->rn || !rd->t.i.special_hdr)
      return -1;

    fields[0] = rd->t.i.special_hdr;
    fields[1] = "received";
    fields[2] = NULL;
    cookie = 0L;
    if(h=pine_fetchheader_lines(rd->t.i.stream, rd->t.i.stream->nmsgs,
				NULL, fields)){
	simple_header_parse(h, fields, values);
	if(values[1])				/* Received lines present! */
	  ans = rd_prompt_about_forged_remote_data(-1, rd, NULL);
	else if(values[0]){
	    cookie = strtoul(values[0], (char **)NULL, 10);
	    if(cookie == rd->cookie)		/* all's well */
	      ans = 0;
	    else
	      ans = rd_prompt_about_forged_remote_data(cookie > 1L
							  ? 100 : cookie,
						       rd, values[0]);
	}
	else
	  ans = rd_prompt_about_forged_remote_data(-2, rd, NULL);

	if(values[0])
	  fs_give((void **)&values[0]);

	if(values[1])
	  fs_give((void **)&values[1]);

	fs_give((void **)&h);
    }
    else					/* missing magic header */
      ans = rd_prompt_about_forged_remote_data(-2, rd, NULL);

    return ans;
}


int
rd_prompt_about_forged_remote_data(reason, rd, extra)
    int        reason;
    REMDATA_S *rd;
    char      *extra;
{
    char      tmp[2000];
    char     *unknown = "<unknown>";
    int       rv = -1;
    char *foldertype, *foldername, *special;

    foldertype = (rd && rd->t.i.special_hdr && !strucmp(rd->t.i.special_hdr, REMOTE_ABOOK_SUBTYPE)) ? "address book" : (rd && rd->t.i.special_hdr && !strucmp(rd->t.i.special_hdr, REMOTE_PINERC_SUBTYPE)) ? "configuration" : "data";
    foldername = (rd && rd->rn) ? rd->rn : unknown;
    special = (rd && rd->t.i.special_hdr) ? rd->t.i.special_hdr : unknown;

    dprint(1, (debugfile, "rd_check_out_forged_remote_data:\n"));
    dprint(1, (debugfile, " reason=%d\n", reason));
    dprint(1, (debugfile, " folder_type=%s\n", foldertype ? foldertype : "?"));
    dprint(1, (debugfile, " remotename=%s\n\n", foldername ? foldername : "?"));

    if(rd && rd->flags & USER_SAID_NO)
      return rv;

    if(reason == -2){
	dprint(1, (debugfile, "The special header \"%.20s\" is missing from the last message in the folder.\nThis indicates that something is wrong.\nYou should probably answer \"No\"\nso that you don't use the corrupt data.\nThen you should investigate further.\n", special ? special : "?"));
    }
    else if(reason == -1){
	dprint(1, (debugfile,  "The last message in the folder contains \"Received\" headers.\nThis usually indicates that the message was put there by the mail\ndelivery system. Pine does not add those Received headers.\nYou should probably answer \"No\" so that you don't use the corrupt data.\nThen you should investigate further.\n"));
    }
    else if(reason == 0){
	dprint(1, (debugfile, "The special header \"%.20s\" in the last message\nin the folder has an unexpected value (%.30s)\nafter it. This could indicate that something is wrong.\nThis value should not normally be put there by Pine.\nHowever, since there are no Received lines in the message we choose to\nbelieve that everything is ok and we will proceed.\n",
		special ? special : "?", (extra && *extra) ? extra : "?"));
    }
    else if(reason == 1){
	dprint(1, (debugfile, "The special header \"%.20s\" in the last message\nin the folder has an unexpected value (1)\nafter it. It appears that it may have been put there by a Pine\nwith a version number less than 4.50.\nSince there are no Received lines in the message we choose to believe that\nthe header was added by an old Pine and we will proceed.\n",
		special ? special : "?"));
    }
    else if(reason > 1){
	dprint(1, (debugfile, "The special header \"%.20s\" in the last message\nin the folder has an unexpected value (%.30s)\nafter it. This is the right sort of value that Pine would normally put there,\nbut it doesn't match the value from the first message in the folder.\nThis may indicate that something is wrong.\nHowever, since there are no Received lines in the message we choose to\nbelieve that everything is ok and we will proceed.\n",
		special ? special : "?", (extra && *extra) ? extra : "?"));
    }

    if(reason >= 0){
	/*
	 * This check should not really be here. We have a cookie that
	 * has the wrong value so something is possibly wrong.
	 * But we are worried that old pines will put the bad value in
	 * there (which they will) and then the questions will bother
	 * users and mystify them. So we're just going to pretend the user
	 * said Yes in this case, and we'll try to fix the cookie.
	 * We still catch Received lines and use that or the complete absence
	 * of a special header as indicators of trouble.
	 */
	rd->flags |= USER_SAID_YES;
	return(1);
    }

    if(ps_global->ttyo){
	static struct key forge_keys[] =
	   {HELP_MENU,
	    NULL_MENU,
	    {"Y","Yes, continue",{MC_YES,1,{'y'}},KS_NONE},
	    {"N","No",{MC_NO,1,{'n'}},KS_NONE},
	    NULL_MENU,
	    NULL_MENU,
	    PREVPAGE_MENU,
	    NEXTPAGE_MENU,
	    PRYNTTXT_MENU,
	    WHEREIS_MENU,
	    FWDEMAIL_MENU,
	    {"S", "Save", {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
	INST_KEY_MENU(forge_keymenu, forge_keys);
	SCROLL_S  sargs;
	STORE_S  *in_store, *out_store;
	gf_io_t   pc, gc;
	HANDLE_S *handles = NULL;
	int       the_answer = 'n';

	if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS)) ||
	   !(out_store = so_get(CharStar, NULL, EDIT_ACCESS)))
	  goto try_wantto;

	sprintf(tmp, "<HTML><P>The data in the remote %.20s folder<P><CENTER>%.50s</CENTER><P>looks suspicious. The reason for the suspicion is<P><CENTER>",
		foldertype, foldername);
	so_puts(in_store, tmp);

	if(reason == -2){
	    sprintf(tmp, "header \"%.20s\" is missing</CENTER><P>The special header \"%.20s\" is missing from the last message in the folder. This indicates that something is wrong. You should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further.",
		    special, special);
	    so_puts(in_store, tmp);
	}
	else if(reason == -1){
	    so_puts(in_store, "\"Received\" headers detected</CENTER><P>The last message in the folder contains \"Received\" headers. This usually indicates that the message was put there by the mail delivery system. Pine does not add those Received headers. You should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further.");
	}
	else if(reason == 0){
	    sprintf(tmp, "Unexpected value for header \"%.20s\"</CENTER><P>The special header \"%.20s\" in the last message in the folder has an unexpected value (%.30s) after it. This probably indicates that something is wrong. This value would not normally be put there by Pine. You should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further.",
		    special, special, (extra && *extra) ? extra : "?");
	    so_puts(in_store, tmp);
	}
	else if(reason == 1){
	    sprintf(tmp, "Unexpected value for header \"%.20s\"</CENTER><P>The special header \"%.20s\" in the last message in the folder has an unexpected value (1) after it. It appears that it may have been put there by a Pine with a version number less than 4.50. If you believe that you have changed this data with an older Pine more recently than you've changed it with this version of Pine, then you can probably safely answer \"Yes\". If you do not understand why this has happened, you should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further.",
		    special, special);
	    so_puts(in_store, tmp);
	}
	else if(reason > 1){
	    sprintf(tmp, "Unexpected value for header \"%.20s\"</CENTER><P>The special header \"%.20s\" in the last message in the folder has an unexpected value (%.30s) after it. This is the right sort of value that Pine would normally put there, but it doesn't match the value from the first message in the folder. This may indicate that something is wrong. Unless you understand why this has happened, you should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further.",
		    special, special, (extra && *extra) ? extra : "?");
	    so_puts(in_store, tmp);
	}

	so_seek(in_store, 0L, 0);
	init_handles(&handles);
	gf_filter_init();
	gf_link_filter(gf_html2plain,
		       gf_html2plain_opt(NULL,
					 ps_global->ttyo->screen_cols, NULL,
					 &handles, GFHP_LOCAL_HANDLES));
	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);
	gf_pipe(gc, pc);
	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.handles  = handles;
	sargs.text.text     = so_text(out_store);
	sargs.text.src      = CharStar;
	sargs.bar.title     = "REMOTE DATA FORGERY WARNING";
	sargs.proc.tool     = rd_answer_forge_warning;
	sargs.proc.data.p   = (void *)&the_answer;
	sargs.keys.menu     = &forge_keymenu;
	setbitmap(sargs.keys.bitmap);

	scrolltool(&sargs);

	if(the_answer == 'y'){
	    rv = 1;
	    rd->flags |= USER_SAID_YES;
	}
	else if(rd)
	  rd->flags |= USER_SAID_NO;

	ps_global->mangled_screen = 1;
	ps_global->painted_body_on_startup = 0;
	ps_global->painted_footer_on_startup = 0;
	so_give(&in_store);
	so_give(&out_store);
	free_handles(&handles);
    }
    else{
	char *p = tmp;

	sprintf(p, "\nThe data in the remote %.20s folder\n\n   %.70s\n\nlooks suspicious. The reason for the suspicion is\n\n   ",
		foldertype, foldername);
	p += strlen(p);

	if(reason == -2){
	    sprintf(p, "header \"%.20s\" is missing\n\nThe special header \"%.20s\" is missing from the last message\nin the folder. This indicates that something is wrong.\nYou should probably answer \"No\" so that you don't use the corrupt data.\nThen you should investigate further.\n\n",
		    special, special);
	}
	else if(reason == -1){
	    sprintf(p, "\"Received\" headers detected\n\nThe last message in the folder contains \"Received\" headers.\nThis usually indicates that the message was put there by the\nmail delivery system. Pine does not add those Received headers.\nYou should probably answer \"No\" so that you don't use the corrupt data.\nThen you should investigate further.\n\n");
	}
	else if(reason == 0){
	    sprintf(p, "Unexpected value for header \"%.20s\"\n\nThe special header \"%.20s\" in the last message in the folder\nhas an unexpected value (%.30s) after it. This probably\nindicates that something is wrong. This value would not normally be put\nthere by Pine. You should probably answer \"No\" so that you don't use\nthe corrupt data. Then you should investigate further.\n\n",
		    special, special, (extra && *extra) ? extra : "?");
	}
	else if(reason == 1){
	    sprintf(p, "Unexpected value for header \"%.20s\"\n\nThe special header \"%.20s\" in the last message in the folder\nhas an unexpected value (1) after it. It appears that it may have been\nput there by a Pine with a version number less than 4.50.\nIf you believe that you have changed this data with an older Pine more\nrecently than you've changed it with this version of Pine, then you can\nprobably safely answer \"Yes\". If you do not understand why this has\nhappened, you should probably answer \"No\" so that you don't use the\ncorrupt data. Then you should investigate further.\n\n",
		    special, special);
	}
	else if(reason > 1){
	    sprintf(p, "Unexpected value for header \"%.20s\"\n\nThe special header \"%.20s\" in the last message in the folder\nhas an unexpected\nvalue (%.30s) after it. This is\nthe right sort of value that Pine would normally put there, but it\ndoesn't match the value from the first message in the folder. This may\nindicate that something is wrong. Unless you understand why this has happened,\nyou should probably answer \"No\" so that you don't use the\ncorrupt data. Then you should investigate further.\n\n",
		    special, special, (extra && *extra) ? extra : "?");
	}

try_wantto:
	p += strlen(p);
	sprintf(p, "Suspicious data in \"%.50s\": Continue anyway ",
		(rd && rd->t.i.special_hdr) ? rd->t.i.special_hdr
					    : unknown,
		(rd && rd->rn) ? rd->rn : unknown,
		short_str((rd && rd->rn) ? rd->rn : "<noname>", tmp+1900,
			  33, FrontDots));
	if(want_to(tmp, 'n', 'x', NO_HELP, WT_NORM) == 'y'){
	    rv = 1;
	    rd->flags |= USER_SAID_YES;
	}
	else if(rd)
	  rd->flags |= USER_SAID_NO;
    }

    if(rv < 0)
      q_status_message1(SM_ORDER, 1, 3, "Can't open remote %.200s",
			(rd && rd->rn) ? rd->rn : "<noname>");

    return(rv);
}


int
rd_answer_forge_warning(cmd, msgmap, sparms)
    int	       cmd;
    MSGNO_S   *msgmap;
    SCROLL_S  *sparms;
{
    int rv = 1;

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_YES :
	*(int *)(sparms->proc.data.p) = 'y';
	break;

      case MC_NO :
	*(int *)(sparms->proc.data.p) = 'n';
	break;

      default:
	panic("Unexpected command in rd_answer_forge_warning");
	break;
    }

    return(rv);
}


PINERC_S *
new_pinerc_s(name)
    char *name;
{
    PINERC_S *prc = NULL;

    if(name){
	prc = (PINERC_S *)fs_get(sizeof(*prc));
	memset((void *)prc, 0, sizeof(*prc));
	prc->name = cpystr(name);
	if(IS_REMOTE(name))
	  prc->type = RemImap;
	else
	  prc->type = Loc;
    }

    return(prc);
}


void
free_pinerc_s(prc)
    PINERC_S **prc;
{
    if(prc && *prc){
	if((*prc)->name)
	  fs_give((void **)&(*prc)->name);

	if((*prc)->rd)
	  rd_free_remdata(&(*prc)->rd);

	if((*prc)->pinerc_lines)
	  free_pinerc_lines(&(*prc)->pinerc_lines);

	fs_give((void **)prc);
    }
}


int
copy_pinerc(local, remote, err_msg)
    char  *local;
    char  *remote;
    char **err_msg;
{
    return(copy_localfile_to_remotefldr(RemImap, local, remote,
					(void *)REMOTE_PINERC_SUBTYPE,
					err_msg));
}


int
copy_abook(local, remote, err_msg)
    char  *local;
    char  *remote;
    char **err_msg;
{
    return(copy_localfile_to_remotefldr(RemImap, local, remote,
					(void *)REMOTE_ABOOK_SUBTYPE,
					err_msg));
}


/*
 * Copy local file to remote folder.
 *
 * Args remotetype -- type of remote folder
 *           local -- name of local file
 *          remote -- name of remote folder
 *         subtype --
 *
 * Returns 0 on success.
 */
int
copy_localfile_to_remotefldr(remotetype, local, remote, subtype, err_msg)
    RemType remotetype;
    char   *local;
    char   *remote;
    void   *subtype;
    char  **err_msg;
{
    int        retfail = -1;
    unsigned   flags;
    REMDATA_S *rd;

    dprint(9, (debugfile, "copy_localfile_to_remotefldr(%s,%s)\n",
	       local ? local : "<null>",
	       remote ? remote : "<null>"));

    *err_msg = (char *)fs_get(MAXPATH * sizeof(char));

    if(!local || !*local){
	sprintf(*err_msg, "No local file specified");
	return(retfail);
    }

    if(!remote || !*remote){
	sprintf(*err_msg, "No remote folder specified");
	return(retfail);
    }

    if(!IS_REMOTE(remote)){
	sprintf(*err_msg, "Remote folder name \"%s\" %s", remote,
		(*remote != '{') ? "must begin with \"{\"" : "not valid");
	return(retfail);
    }

    if(IS_REMOTE(local)){
	sprintf(*err_msg, "First argument \"%s\" must be a local filename",
	        local);
	return(retfail);
    }

    if(can_access(local, ACCESS_EXISTS) != 0){
	sprintf(*err_msg, "Local file \"%s\" does not exist", local);
	return(retfail);
    }

    if(can_access(local, READ_ACCESS) != 0){
	sprintf(*err_msg, "Can't read local file \"%s\": %s",
	        local, error_description(errno));
	return(retfail);
    }

    /*
     * Check if remote folder exists and create it if it doesn't.
     */
    flags = 0;
    rd = rd_create_remote(remotetype, remote, subtype,
			  &flags, "Error: ", "Can't copy to remote folder.");
    
    if(!rd || rd->access == NoExists){
	sprintf(*err_msg, "Can't create \"%s\"", remote);
	if(rd)
	  rd_free_remdata(&rd);

	return(retfail);
    }

    if(rd->access == MaybeRorW)
      rd->access = ReadWrite;

    rd->flags |= (NO_META_UPDATE | DO_REMTRIM);
    rd->lf = cpystr(local);

    rd_open_remote(rd);
    if(!rd_stream_exists(rd)){
	sprintf(*err_msg, "Can't open remote folder \"%s\"", rd->rn);
	rd_free_remdata(&rd);
	return(retfail);
    }

    if(rd_remote_is_readonly(rd)){
	sprintf(*err_msg, "Remote folder \"%s\" is readonly", rd->rn);
	rd_free_remdata(&rd);
	return(retfail);
    }

    switch(rd->type){
      case RemImap:
	/*
	 * Empty folder, add a header msg.
	 */
	if(rd->t.i.stream->nmsgs == 0){
	    if(rd_init_remote(rd, 1) != 0){
		sprintf(*err_msg,
		  "Failed initializing remote folder \"%s\", check debug file",
		  rd->rn);
		rd_free_remdata(&rd);
		return(retfail);
	    }
	}

	fs_give((void **)err_msg);
	*err_msg = NULL;
	if(rd_chk_for_hdr_msg(&(rd->t.i.stream), rd, err_msg)){
		rd_free_remdata(&rd);
		return(retfail);
	}

	break;

      default:
	break;
    }

    if(rd_update_remote(rd, NULL) != 0){
	sprintf(*err_msg, "Error copying to remote folder \"%s\"", rd->rn);
	rd_free_remdata(&rd);
	return(retfail);
    }

    rd_update_metadata(rd, NULL);
    rd_close_remdata(&rd);
    
    fs_give((void **)err_msg);
    return(0);
}


/*
 * Side effect is that the appropriate global variable is set, and the
 * appropriate current_val is set.
 */
void
cur_rule_value(var, expand, cmdline)
    struct variable *var;
    int              expand, cmdline;
{
    int i;
    NAMEVAL_S *v;

    set_current_val(var, expand, cmdline);

    if(var == &ps_global->vars[V_SAVED_MSG_NAME_RULE]){
      if(ps_global->VAR_SAVED_MSG_NAME_RULE)
	for(i = 0; v = save_msg_rules(i); i++)
	  if(!strucmp(ps_global->VAR_SAVED_MSG_NAME_RULE, S_OR_L(v))){
	      ps_global->save_msg_rule = v->value;
	      break;
	  }
    }
#ifndef	_WINDOWS
    else if(var == &ps_global->vars[V_COLOR_STYLE]){
      if(ps_global->VAR_COLOR_STYLE)
	for(i = 0; v = col_style(i); i++)
	  if(!strucmp(ps_global->VAR_COLOR_STYLE, S_OR_L(v))){
	      ps_global->color_style = v->value;
	      break;
	  }
    }
#endif
    else if(var == &ps_global->vars[V_INDEX_COLOR_STYLE]){
      if(ps_global->VAR_INDEX_COLOR_STYLE)
	for(i = 0; v = index_col_style(i); i++)
	  if(!strucmp(ps_global->VAR_INDEX_COLOR_STYLE, S_OR_L(v))){
	      ps_global->index_color_style = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_TITLEBAR_COLOR_STYLE]){
      if(ps_global->VAR_TITLEBAR_COLOR_STYLE)
	for(i = 0; v = titlebar_col_style(i); i++)
	  if(!strucmp(ps_global->VAR_TITLEBAR_COLOR_STYLE, S_OR_L(v))){
	      ps_global->titlebar_color_style = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_FCC_RULE]){
      if(ps_global->VAR_FCC_RULE)
	for(i = 0; v = fcc_rules(i); i++)
	  if(!strucmp(ps_global->VAR_FCC_RULE, S_OR_L(v))){
	      ps_global->fcc_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_GOTO_DEFAULT_RULE]){
      if(ps_global->VAR_GOTO_DEFAULT_RULE)
	for(i = 0; v = goto_rules(i); i++)
	  if(!strucmp(ps_global->VAR_GOTO_DEFAULT_RULE, S_OR_L(v))){
	      ps_global->goto_default_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_INCOMING_STARTUP]){
      if(ps_global->VAR_INCOMING_STARTUP)
	for(i = 0; v = incoming_startup_rules(i); i++)
	  if(!strucmp(ps_global->VAR_INCOMING_STARTUP, S_OR_L(v))){
	      ps_global->inc_startup_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_PRUNING_RULE]){
      if(ps_global->VAR_PRUNING_RULE)
	for(i = 0; v = pruning_rules(i); i++)
	  if(!strucmp(ps_global->VAR_PRUNING_RULE, S_OR_L(v))){
	      ps_global->pruning_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_REOPEN_RULE]){
      if(ps_global->VAR_REOPEN_RULE)
	for(i = 0; v = reopen_rules(i); i++)
	  if(!strucmp(ps_global->VAR_REOPEN_RULE, S_OR_L(v))){
	      ps_global->reopen_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_FLD_SORT_RULE]){
      if(ps_global->VAR_FLD_SORT_RULE)
	for(i = 0; v = fld_sort_rules(i); i++)
	  if(!strucmp(ps_global->VAR_FLD_SORT_RULE, S_OR_L(v))){
	      ps_global->fld_sort_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_AB_SORT_RULE]){
      if(ps_global->VAR_AB_SORT_RULE)
	for(i = 0; v = ab_sort_rules(i); i++)
	  if(!strucmp(ps_global->VAR_AB_SORT_RULE, S_OR_L(v))){
	      ps_global->ab_sort_rule = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_THREAD_DISP_STYLE]){
      if(ps_global->VAR_THREAD_DISP_STYLE)
	for(i = 0; v = thread_disp_styles(i); i++)
	  if(!strucmp(ps_global->VAR_THREAD_DISP_STYLE, S_OR_L(v))){
	      ps_global->thread_disp_style = v->value;
	      break;
	  }
    }
    else if(var == &ps_global->vars[V_THREAD_INDEX_STYLE]){
      if(ps_global->VAR_THREAD_INDEX_STYLE)
	for(i = 0; v = thread_index_styles(i); i++)
	  if(!strucmp(ps_global->VAR_THREAD_INDEX_STYLE, S_OR_L(v))){
	      ps_global->thread_index_style = v->value;
	      break;
	  }
    }
}


void
convert_to_remote_config(ps, edit_exceptions)
    struct pine *ps;
    int edit_exceptions;
{
    char       rem_pinerc_prefix[MAILTMPLEN];
    char      *beg, *end;
    CONTEXT_S *context;
    int        abooks, sigs;

    if(edit_exceptions){
	q_status_message(SM_ORDER, 3, 5,
			 "eXceptions does not make sense with this command");
	return;
    }

    if(!ps->prc)
      panic("NULL prc in convert_to_remote_config");

    dprint(2, (debugfile, "convert_to_remote_config\n"));
    
    if(ps->prc->type == RemImap){	/* pinerc is already remote */
	char prompt[MAILTMPLEN];

	/*
	 * Check to see if there is anything at all to do. If there are
	 * address books to convert, sigfiles to convert, or rule files
	 * to comment on, we have something to do. Otherwise, just bail.
	 */
	abooks = any_addrbooks_to_convert(ps);
	sigs   = any_sigs_to_convert(ps);

	if(abooks || sigs){
	    sprintf(prompt,
		    "Config is already remote, convert%.20s%.20s%.20s ",
		    abooks ? " AddressBooks" : "",
		    (abooks && sigs) ? " and" : "",
		    sigs ? " signature files" : "");
	    if(want_to(prompt, 'y', 'x',
		       (abooks && sigs) ? h_convert_abooks_and_sigs :
			abooks           ? h_convert_abooks :
			 sigs             ? h_convert_sigs : NO_HELP,
		       WT_NORM) != 'y'){
		cmd_cancelled(NULL);
		return;
	    }
	}
    }

    /*
     * Figure out a good default for where to put the remote config.
     * If the default collection is remote we'll take the hostname and
     * and modifiers from there. If not, we'll try to get the hostname from
     * the inbox-path. In either case, we use the home directory on the
     * server, not the directory where the folder collection is (if different).
     * If we don't have a clue, we'll ask user.
     */
    if((context = default_save_context(ps->context_list)) != NULL &&
       IS_REMOTE(context_apply(rem_pinerc_prefix, context, "",
	         sizeof(rem_pinerc_prefix)))){
	/* just use the host from the default collection, not the whole path */
	if(end = strrindex(rem_pinerc_prefix, '}'))
	  *(end + 1) = '\0';
    }
    else{
	/* use host from inbox path */
	rem_pinerc_prefix[0] = '\0';
	if((beg = ps->VAR_INBOX_PATH)
	   && (*beg == '{' || (*beg == '*' && *++beg == '{'))
	   && (end = strindex(ps->VAR_INBOX_PATH, '}'))){
	    rem_pinerc_prefix[0] = '{';
	    strncpy(rem_pinerc_prefix+1, beg+1,
		    min(end-beg, sizeof(rem_pinerc_prefix)-2));
	    rem_pinerc_prefix[min(end-beg, sizeof(rem_pinerc_prefix)-2)] = '}';
	    rem_pinerc_prefix[min(end-beg+1,sizeof(rem_pinerc_prefix)-1)]='\0';
	}
    }

    /* ask about converting addrbooks to remote abooks */
    if(ps->prc->type != RemImap || abooks)
      if(convert_addrbooks_to_remote(ps, rem_pinerc_prefix,
				     sizeof(rem_pinerc_prefix)) == -1){
	  cmd_cancelled(NULL);
	  return;
      }

    /* ask about converting sigfiles to literal sigs */
    if(ps->prc->type != RemImap || sigs)
      if(convert_sigs_to_literal(ps, 1) == -1){
	  cmd_cancelled(NULL);
	  return;
      }

    warn_about_rule_files(ps);

    /* finally, copy the config file */
    if(ps->prc->type == Loc)
      convert_pinerc_to_remote(ps, rem_pinerc_prefix);
    else if(!(abooks || sigs))
      q_status_message(SM_ORDER, 3, 5,
		       "Cannot copy config file since it is already remote.");
}


void
convert_pinerc_to_remote(ps, rem_pinerc_prefix)
    struct pine *ps;
    char        *rem_pinerc_prefix;
{
#define DEF_FOLDER_NAME "remote_pinerc"
    char       prompt[MAILTMPLEN], rem_pinerc[MAILTMPLEN];
    char      *err_msg = NULL;
    int        i, rc, offset;
    HelpType   help;
    int        flags = OE_APPEND_CURRENT;

    ClearBody();
    ps->mangled_body = 1;
    strncpy(rem_pinerc, rem_pinerc_prefix, sizeof(rem_pinerc)-1);
    rem_pinerc[sizeof(rem_pinerc)-1] = '\0';

    if(*rem_pinerc == '\0'){
	sprintf(prompt, "Name of server to contain remote Pine config : ");
	help = NO_HELP;
	while(1){
	    rc = optionally_enter(rem_pinerc, -FOOTER_ROWS(ps), 0,
				  sizeof(rem_pinerc), prompt, NULL,
				  help, &flags);
	    removing_leading_and_trailing_white_space(rem_pinerc);
	    if(rc == 3){
		help = help == NO_HELP ? h_convert_pinerc_server : NO_HELP;
	    }
	    else if(rc == 1){
		cmd_cancelled(NULL);
		return;
	    }
	    else if(rc == 0){
		if(*rem_pinerc){
		    /* add brackets */
		    offset = strlen(rem_pinerc);
		    for(i = offset; i >= 0; i--)
		      rem_pinerc[i+1] = rem_pinerc[i];
		    
		    rem_pinerc[0] = '{';
		    rem_pinerc[++offset] = '}';
		    rem_pinerc[++offset] = '\0';
		    break;
		}
	    }
	}
    }

    /*
     * Add a default folder name.
     */
    if(*rem_pinerc){
	/*
	 * Add /user= to modify hostname so that user won't be asked who they
	 * are each time they login.
	 */
	if(!strstr(rem_pinerc, "/user=") && ps->VAR_USER_ID &&
	   ps->VAR_USER_ID[0]){
	    char *p;

	    p = rem_pinerc + strlen(rem_pinerc) - 1;
	    if(*p == '}')	/* this should be the case */
	      sprintf(p, "/user=\"%.64s\"}", ps->VAR_USER_ID); 
	}

	strncat(rem_pinerc, DEF_FOLDER_NAME,
		sizeof(rem_pinerc) - strlen(rem_pinerc) - 1);
	rem_pinerc[sizeof(rem_pinerc)-1] = '\0';
    }

    /* ask user about folder name for remote config */
    sprintf(prompt, "Folder to contain remote config : ");
    help = NO_HELP;
    while(1){
        rc = optionally_enter(rem_pinerc, -FOOTER_ROWS(ps), 0, 
			      sizeof(rem_pinerc), prompt, NULL, help, &flags);
	removing_leading_and_trailing_white_space(rem_pinerc);
        if(rc == 0 && *rem_pinerc){
	    break;
	}

        if(rc == 3){
	    help = (help == NO_HELP) ? h_convert_pinerc_folder : NO_HELP;
	}
	else if(rc == 1 || rem_pinerc[0] == '\0'){
	    cmd_cancelled(NULL);
	    return;
	}
    }

#ifndef	_WINDOWS
    /*
     * If we are on a Unix system, writing to a remote config, we want the
     * remote config to work smoothly from a PC, too. If we don't have a
     * user-id on the PC then we will be asked for our password.
     * So add user-id to the pinerc before we copy it.
     */
    if(!ps->vars[V_USER_ID].main_user_val.p && ps->VAR_USER_ID)
      ps->vars[V_USER_ID].main_user_val.p = cpystr(ps->VAR_USER_ID);
    
    ps->vars[V_USER_ID].is_used = 1;	/* so it will write to pinerc */
    ps->prc->outstanding_pinerc_changes = 1;
#endif

    if(ps->prc->outstanding_pinerc_changes)
      write_pinerc(ps, Main, WRP_NONE);

#ifndef	_WINDOWS
    ps->vars[V_USER_ID].is_used = 0;
#endif

    /* copy the pinerc */
    if(copy_pinerc(ps->prc->name, rem_pinerc, &err_msg)){
	if(err_msg){
	    q_status_message(SM_ORDER | SM_DING, 7, 10, err_msg);
	    fs_give((void **)&err_msg);
	}
	
	return;
    }

    /* tell user about command line flags */
    if(ps->prc->type != RemImap){
	STORE_S *store;
	SCROLL_S sargs;

	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     "Error allocating space for message.");
	    return;
	}

	so_puts(store, "\nYou may want to save a copy of this information!");
	so_puts(store, "\n\nYour Pine configuration data has been copied to");
	so_puts(store, "\n\n   ");
	so_puts(store, rem_pinerc);
	so_puts(store, "\n");
	so_puts(store, "\nTo use that remote configuration from this computer you will");
	so_puts(store, "\nhave to change the way you start Pine by using the command line option");
	so_puts(store, "\n\"pine -p <remote_folder>\". The command should probably be");
#ifdef	_WINDOWS
	so_puts(store, "\n\n   ");
	so_puts(store, "pine -p ");
	so_puts(store, rem_pinerc);
	so_puts(store, "\n");
	so_puts(store, "\nWith PC-Pine, you may want to create a shortcut which");
	so_puts(store, "\nhas the required arguments.");
#else
	so_puts(store, "\n\n   ");
	so_puts(store, "pine -p \"");
	so_puts(store, rem_pinerc);
	so_puts(store, "\"\n");
	so_puts(store, "\nThe quotes are there around the last argument to protect the special");
	so_puts(store, "\ncharacters in the folder name (like braces) from the command shell");
	so_puts(store, "\nyou use. If you are not running Pine from a command shell which knows");
	so_puts(store, "\nabout quoting, it is possible you will have to remove those quotes");
	so_puts(store, "\nfrom the command. For example, if you also use PC-Pine you will probably");
	so_puts(store, "\nwant to create a shortcut, and you would not need the quotes there.");
	so_puts(store, "\nWithout the quotes, the command might look like");
	so_puts(store, "\n\n   ");
	so_puts(store, "pine -p ");
	so_puts(store, rem_pinerc);
	so_puts(store, "\n");
	so_puts(store, "\nConsider creating an alias or shell script to execute this command to make");
	so_puts(store, "\nit more convenient.");
#endif
	so_puts(store, "\n\nIf you want to use your new remote configuration for this session, quit");
	so_puts(store, "\nPine now and restart with the changed command line options mentioned above.\n");

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text  = so_text(store);
	sargs.text.src   = CharStar;
	sargs.text.desc  = "Remote Config Information";
	sargs.bar.title  = "ABOUT REMOTE CONFIG";
	sargs.help.text  = NO_HELP;
	sargs.help.title = NULL;

	scrolltool(&sargs);

	so_give(&store);	/* free resources associated with store */
	ps->mangled_screen = 1;
    }
}
