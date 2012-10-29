/*
 * Program:	RFC 2822 and MIME routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	27 July 1988
 * Last Edited:	8 September 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 *
 * This original version of this file is
 * Copyright 1988 Stanford University
 * and was developed in the Symbolic Systems Resources Group of the Knowledge
 * Systems Laboratory at Stanford University in 1987-88, and was funded by the
 * Biomedical Research Technology Program of the NationalInstitutes of Health
 * under grant number RR-00785.
 */

#define MAXGROUPDEPTH 50	/* RFC 822 doesn't allow nesting at all */
#define MAXMIMEDEPTH 50		/* more than any sane MIMEgram */

#define rfc822_write_address(dest,adr) \
  rfc822_write_address_full (dest,adr,NIL)

#define rfc822_parse_msg(en,bdy,s,i,bs,host,flags) \
  rfc822_parse_msg_full (en,bdy,s,i,bs,host,0,flags)

/* Function prototypes */

void rfc822_header (char *header,ENVELOPE *env,BODY *body);
void rfc822_address_line (char **header,char *type,ENVELOPE *env,ADDRESS *adr);
void rfc822_header_line (char **header,char *type,ENVELOPE *env,char *text);
char *rfc822_write_address_full (char *dest,ADDRESS *adr,char *base);
void rfc822_address (char *dest,ADDRESS *adr);
void rfc822_cat (char *dest,char *src,const char *specials);
void rfc822_write_body_header (char **header,BODY *body);
char *rfc822_default_subtype (unsigned short type);
void rfc822_parse_msg_full (ENVELOPE **en,BODY **bdy,char *s,unsigned long i,
			    STRING *bs,char *host,unsigned long depth,
			    unsigned long flags);
void rfc822_parse_content (BODY *body,STRING *bs,char *h,unsigned long depth,
			   unsigned long flags);
void rfc822_parse_content_header (BODY *body,char *name,char *s);
void rfc822_parse_parameter (PARAMETER **par,char *text);
void rfc822_parse_adrlist (ADDRESS **lst,char *string,char *host);
ADDRESS *rfc822_parse_address (ADDRESS **lst,ADDRESS *last,char **string,
			       char *defaulthost,unsigned long depth);
ADDRESS *rfc822_parse_group (ADDRESS **lst,ADDRESS *last,char **string,
			     char *defaulthost,unsigned long depth);
ADDRESS *rfc822_parse_mailbox (char **string,char *defaulthost);
long rfc822_phraseonly (char *end);
ADDRESS *rfc822_parse_routeaddr (char *string,char **ret,char *defaulthost);
ADDRESS *rfc822_parse_addrspec (char *string,char **ret,char *defaulthost);
char *rfc822_parse_domain (char *string,char **end);
char *rfc822_parse_phrase (char *string);
char *rfc822_parse_word (char *string,const char *delimiters);
char *rfc822_cpy (char *src);
char *rfc822_quote (char *src);
ADDRESS *rfc822_cpy_adr (ADDRESS *adr);
void rfc822_skipws (char **s);
char *rfc822_skip_comment (char **s,long trim);

typedef long (*soutr_t) (void *stream,char *string);
typedef long (*rfc822out_t) (char *t,ENVELOPE *env,BODY *body,soutr_t f,
			     void *s,long ok8bit);

long rfc822_output (char *t,ENVELOPE *env,BODY *body,soutr_t f,void *s,
		    long ok8bit);
void rfc822_encode_body_7bit (ENVELOPE *env,BODY *body);
void rfc822_encode_body_8bit (ENVELOPE *env,BODY *body);
long rfc822_output_body (BODY *body,soutr_t f,void *s);
void *rfc822_base64 (unsigned char *src,unsigned long srcl,unsigned long *len);
unsigned char *rfc822_binary (void *src,unsigned long srcl,unsigned long *len);
unsigned char *rfc822_qprint (unsigned char *src,unsigned long srcl,
			      unsigned long *len);
unsigned char *rfc822_8bit (unsigned char *src,unsigned long srcl,
			    unsigned long *len);
