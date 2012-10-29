/*
 * Program:	Interactive Mail Access Protocol 4rev1 (IMAP4R1) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	14 October 1988
 * Last Edited:	7 April 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Protocol levels */

/* As of October 15, 2003, it is believed that:
 *
 * Version	RFC		Status		Known Implementations
 * -------	---		------		---------------------
 * IMAP1	none		extinct		experimental TOPS-20 server
 * IMAP2	1064		extinct		old TOPS-20, SUMEX servers
 * IMAP2	1176		rare		TOPS-20, old UW servers
 * IMAP2bis	expired I-D	uncommon	old UW, Cyrus servers
 * IMAP3	1203		extinct		none (never implemented)
 * IMAP4	1730		rare		old UW, Cyrus, Netscape servers
 * IMAP4rev1	2060, 3501	ubiquitous	UW, Cyrus, and many others
 *
 * Most client implementations will only interoperate with an IMAP4rev1
 * server.  c-client based client implementations can interoperate with IMAP2,
 * IMAP2bis, IMAP4, and IMAP4rev1 servers, but only if they are very careful.
 */


/* Server protocol level and capabilities */

typedef struct imap_cap {
  unsigned int rfc1176 : 1;	/* server is RFC-1176 IMAP2 */
  unsigned int imap2bis : 1;	/* server is IMAP2bis */
  unsigned int imap4 : 1;	/* server is IMAP4 (RFC 1730) */
  unsigned int imap4rev1 : 1;	/* server is IMAP4rev1 */
  unsigned int acl : 1;		/* server has ACL (RFC 2086) */
  unsigned int quota : 1;	/* server has QUOTA (RFC 2087) */
  unsigned int litplus : 1;	/* server has LITERAL+ (RFC 2088) */
  unsigned int idle : 1;	/* server has IDLE (RFC 2177) */
  unsigned int mbx_ref : 1;	/* server has mailbox referrals (RFC 2193) */
  unsigned int log_ref : 1;	/* server has login referrals (RFC 2221) */
  unsigned int authanon : 1;	/* server has anonymous SASL (RFC 2245) */
  unsigned int namespace :1;	/* server has NAMESPACE (RFC 2342) */
  unsigned int uidplus : 1;	/* server has UIDPLUS (RFC 2359) */
  unsigned int starttls : 1;	/* server has STARTTLS (RFC 2595) */
				/* server disallows LOGIN command (RFC 2595) */
  unsigned int logindisabled : 1;
  unsigned int id : 1;		/* server has ID (RFC 2971) */
  unsigned int children : 1;	/* server has CHILDREN (RFC 3348) */
  unsigned int multiappend : 1;	/* server has multi-APPEND (RFC 3502) ;*/
  unsigned int binary : 1;	/* server has BINARY (RFC 3516) */
  unsigned int unselect : 1;	/* server has UNSELECT */
  unsigned int sasl_ir : 1;	/* server has SASL-IR initial response */
  unsigned int sort : 1;	/* server has SORT */
  unsigned int scan : 1;	/* server has SCAN */
  unsigned int extlevel;	/* extension data level supported by server */
				/* supported authenticators */
  unsigned int auth : MAXAUTHENTICATORS;
  THREADER *threader;		/* list of threaders */
} IMAPCAP;

/* IMAP4rev1 level or better */

#define LEVELIMAP4rev1(stream) imap_cap (stream)->imap4rev1

#define LEVELSTATUS LEVELIMAP4rev1


/* IMAP4 level or better (not including RFC 1730 design mistakes) */

#define LEVELIMAP4(stream) (imap_cap (stream)->imap4rev1 || \
			    imap_cap (stream)->imap4)


/* IMAP4 RFC-1730 level */

#define LEVEL1730(stream) imap_cap (stream)->imap4


/* IMAP2bis level or better */

#define LEVELIMAP2bis(stream) imap_cap (stream)->imap2bis


/* IMAP2 RFC-1176 level or better */

#define LEVEL1176(stream) imap_cap (stream)->rfc1176


/* IMAP2 RFC-1064 or better */

#define LEVEL1064(stream) 1

/* Has ACL extension */

#define LEVELACL(stream) imap_cap (stream)->acl


/* Has QUOTA extension */

#define LEVELQUOTA(stream) imap_cap (stream)->quota


/* Has LITERALPLUS extension */

#define LEVELLITERALPLUS(stream) imap_cap (stream)->litplus


/* Has IDLE extension */

#define LEVELIDLE(stream) imap_cap (stream)->idle


/* Has mailbox referrals */

#define LEVELMBX_REF(stream) imap_cap (stream)->mbx_ref


/* Has login referrals */

#define LEVELLOG_REF(stream) imap_cap (stream)->log_ref


/* Has AUTH=ANONYMOUS extension */

#define LEVELANONYMOUS(stream) imap_cap (stream)->authanon


/* Has NAMESPACE extension */

#define LEVELNAMESPACE(stream) imap_cap (stream)->namespace


/* Has UIDPLUS extension */

#define LEVELUIDPLUS(stream) imap_cap (stream)->uidplus


/* Has STARTTLS extension */

#define LEVELSTARTTLS(stream) imap_cap (stream)->starttls


/* Has LOGINDISABLED extension */

#define LEVELLOGINDISABLED(stream) imap_cap (stream)->logindisabled

/* Has ID extension */

#define LEVELID(stream) imap_cap (stream)->id


/* Has CHILDREN extension */

#define LEVELCHILDREN(stream) imap_cap (stream)->children


/* Has MULTIAPPEND extension */

#define LEVELMULTIAPPEND(stream) imap_cap (stream)->multiappend


/* Has BINARY extension */

#define LEVELBINARY(stream) imap_cap (stream)->binary


/* Has UNSELECT extension */

#define LEVELUNSELECT(stream) imap_cap (stream)->unselect


/* Has SASL initial response extension */

#define LEVELSASLIR(stream) imap_cap (stream)->sasl_ir


/* Has SORT extension */

#define LEVELSORT(stream) imap_cap (stream)->sort


/* Has at least one THREAD extension */

#define LEVELTHREAD(stream) ((imap_cap (stream)->threader) ? T : NIL)

/* Has SCAN extension */

#define LEVELSCAN(stream) imap_cap (stream)->scan

/* Body structure extension levels */

/* These are in BODYSTRUCTURE order.  Note that multipart bodies do not have
 * body-fld-md5.  This is alright, since all subsequent body structure
 * extensions are in both singlepart and multipart bodies.  If that ever
 * changes, this will have to be split.
 */

#define BODYEXTMD5 1		/* body-fld-md5 */
#define BODYEXTDSP 2		/* body-fld-dsp */
#define BODYEXTLANG 3		/* body-fld-lang */
#define BODYEXTLOC 4		/* body-fld-loc */


/* Function prototypes */

IMAPCAP *imap_cap (MAILSTREAM *stream);
char *imap_host (MAILSTREAM *stream);
long imap_cache (MAILSTREAM *stream,unsigned long msgno,char *seg,
		 STRINGLIST *stl,SIZEDTEXT *text);


/* Temporary */

long imap_setacl (MAILSTREAM *stream,char *mailbox,char *id,char *rights);
long imap_deleteacl (MAILSTREAM *stream,char *mailbox,char *id);
long imap_getacl (MAILSTREAM *stream,char *mailbox);
long imap_listrights (MAILSTREAM *stream,char *mailbox,char *id);
long imap_myrights (MAILSTREAM *stream,char *mailbox);
long imap_setquota (MAILSTREAM *stream,char *qroot,STRINGLIST *limits);
long imap_getquota (MAILSTREAM *stream,char *qroot);
long imap_getquotaroot (MAILSTREAM *stream,char *mailbox);
