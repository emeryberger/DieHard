/*
 * Program:	CRAM-MD5 authenticator
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	21 October 1998
 * Last Edited:	4 January 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* MD5 context */

#define MD5BLKLEN 64		/* MD5 block length */
#define MD5DIGLEN 16		/* MD5 digest length */

typedef struct {
  unsigned long chigh;		/* high 32bits of byte count */
  unsigned long clow;		/* low 32bits of byte count */
  unsigned long state[4];	/* state (ABCD) */
  unsigned char buf[MD5BLKLEN];	/* input buffer */
  unsigned char *ptr;		/* buffer position */
} MD5CONTEXT;


/* Prototypes */

long auth_md5_valid (void);
long auth_md5_client (authchallenge_t challenger,authrespond_t responder,
		      char *service,NETMBX *mb,void *stream,
		      unsigned long *trial,char *user);
char *auth_md5_server (authresponse_t responder,int argc,char *argv[]);
char *auth_md5_pwd (char *user);
char *apop_login (char *chal,char *user,char *md5,int argc,char *argv[]);
char *hmac_md5 (char *text,unsigned long tl,char *key,unsigned long kl);
void md5_init (MD5CONTEXT *ctx);
void md5_update (MD5CONTEXT *ctx,unsigned char *data,unsigned long len);
void md5_final (unsigned char *digest,MD5CONTEXT *ctx);
static void md5_transform (unsigned long *state,unsigned char *block);
static void md5_encode (unsigned char *dst,unsigned long *src,int len);
static void md5_decode (unsigned long *dst,unsigned char *src,int len);


/* Authenticator linkage */

AUTHENTICATOR auth_md5 = {
  AU_SECURE,			/* secure authenticator */
  "CRAM-MD5",			/* authenticator name */
  auth_md5_valid,		/* check if valid */
  auth_md5_client,		/* client method */
  auth_md5_server,		/* server method */
  NIL				/* next authenticator */
};

/* Check if CRAM-MD5 valid on this system
 * Returns: T, always
 */

long auth_md5_valid (void)
{
  struct stat sbuf;
				/* server forbids MD5 if no MD5 enable file */
  if (stat (MD5ENABLE,&sbuf)) auth_md5.server = NIL;
  return T;			/* MD5 is otherwise valid */
}


/* Client authenticator
 * Accepts: challenger function
 *	    responder function
 *	    SASL service name
 *	    parsed network mailbox structure
 *	    stream argument for functions
 *	    pointer to current trial count
 *	    returned user name
 * Returns: T if success, NIL otherwise, number of trials incremented if retry
 */

long auth_md5_client (authchallenge_t challenger,authrespond_t responder,
		      char *service,NETMBX *mb,void *stream,
		      unsigned long *trial,char *user)
{
  char pwd[MAILTMPLEN];
  void *challenge;
  unsigned long clen;
  long ret = NIL;
				/* get challenge */
  if (challenge = (*challenger) (stream,&clen)) {
    pwd[0] = NIL;		/* prompt user */
    mm_login (mb,user,pwd,*trial);
    if (!pwd[0]) {		/* user requested abort */
      fs_give ((void **) &challenge);
      (*responder) (stream,NIL,0);
      *trial = 0;		/* cancel subsequent attempts */
      ret = LONGT;		/* will get a BAD response back */
    }
    else {			/* got password, build response */
      sprintf (pwd,"%.65s %.33s",user,hmac_md5 (challenge,clen,
						pwd,strlen (pwd)));
      fs_give ((void **) &challenge);
				/* send credentials, allow retry if OK */
      if ((*responder) (stream,pwd,strlen (pwd))) {
	if (challenge = (*challenger) (stream,&clen))
	  fs_give ((void **) &challenge);
	else {
	  ++*trial;		/* can try again if necessary */
	  ret = LONGT;		/* check the authentication */
	}
      }
    }
  }
  memset (pwd,0,MAILTMPLEN);	/* erase password in case not overwritten */
  if (!ret) *trial = 65535;	/* don't retry if bad protocol */
  return ret;
}

/* Server authenticator
 * Accepts: responder function
 *	    argument count
 *	    argument vector
 * Returns: authenticated user name or NIL
 *
 * This is much hairier than it needs to be due to the necessary of zapping
 * the password data.
 */

static int md5try = MAXLOGINTRIALS;

char *auth_md5_server (authresponse_t responder,int argc,char *argv[])
{
  char *ret = NIL;
  char *p,*u,*user,*authuser,*hash,chal[MAILTMPLEN];
  unsigned long cl,pl;
				/* generate challenge */
  sprintf (chal,"<%lu.%lu@%s>",(unsigned long) getpid (),
	   (unsigned long) time (0),mylocalhost ());
				/* send challenge, get user and hash */
  if (user = (*responder) (chal,cl = strlen (chal),NIL)) {
				/* got user, locate hash */
    if (hash = strrchr (user,' ')) {
      *hash++ = '\0';		/* tie off user */
				/* see if authentication user */
      if (authuser = strchr (user,'*')) *authuser++ = '\0';
				/* get password */
      if (p = auth_md5_pwd ((authuser && *authuser) ? authuser : user)) {
	pl = strlen (p);
	u = (md5try && !strcmp (hash,hmac_md5 (chal,cl,p,pl))) ? user : NIL;
	memset (p,0,pl);	/* erase sensitive information */
	fs_give ((void **) &p);	/* flush erased password */
				/* now log in for real */
	if (u && authserver_login (u,authuser,argc,argv)) ret = myusername ();
	else if (md5try) --md5try;
      }
    }
    fs_give ((void **) &user);
  }
  if (!ret) sleep (3);		/* slow down possible cracker */
  return ret;
}

/* Return MD5 password for user
 * Accepts: user name
 * Returns: plaintext password if success, else NIL
 *
 * This is much hairier than it needs to be due to the necessary of zapping
 * the password data.  That's why we don't use stdio here.
 */

char *auth_md5_pwd (char *user)
{
  struct stat sbuf;
  int fd = open (MD5ENABLE,O_RDONLY,NIL);
  unsigned char *s,*t,*buf,*lusr,*lret;
  char *ret = NIL;
  if (fd >= 0) {		/* found the file? */
    fstat (fd,&sbuf);		/* yes, slurp it into memory */
    read (fd,buf = (char *) fs_get (sbuf.st_size + 1),sbuf.st_size);
				/* see if any uppercase characters in user */
    for (s = user; *s && !isupper (*s); s++);
				/* yes, make lowercase copy */
    lusr = *s ? lcase (cpystr (user)) : NIL;
    for (s = strtok (buf,"\015\012"),lret = NIL; s;
	 s = ret ? NIL : strtok (NIL,"\015\012"))
				/* must be valid entry line */
      if (*s && (*s != '#') && (t = strchr (s,'\t')) && t[1]) {
	*t++ = '\0';		/* found tab, tie off user, point to pwd */
	if (!strcmp (s,user)) ret = cpystr (t);
	else if (lusr && !lret) if (!strcmp (s,lusr)) lret = t;
      }
				/* accept case-independent name */
    if (!ret && lret) ret = cpystr (lret);
				/* don't need lowercase copy any more */
    if (lusr) fs_give ((void **) &lusr);
				/* erase sensitive information from buffer */
    memset (buf,0,sbuf.st_size + 1);
    fs_give ((void **) &buf);	/* flush the buffer */
    close (fd);			/* don't need file any longer */
  }
  return ret;			/* return password */
}

/* APOP server login
 * Accepts: challenge
 *	    desired user name
 *	    purported MD5
 *	    argument count
 *	    argument vector
 * Returns: authenticated user name or NIL
 */

char *apop_login (char *chal,char *user,char *md5,int argc,char *argv[])
{
  int i,j;
  char *ret = NIL;
  char *s,*authuser,tmp[MAILTMPLEN];
  unsigned char digest[MD5DIGLEN];
  MD5CONTEXT ctx;
  char *hex = "0123456789abcdef";
				/* see if authentication user */
  if (authuser = strchr (user,'*')) *authuser++ = '\0';
				/* get password */
  if (s = auth_md5_pwd ((authuser && *authuser) ? authuser : user)) {
    md5_init (&ctx);		/* initialize MD5 context */
				/* build string to get MD5 digest */
    sprintf (tmp,"%.128s%.128s",chal,s);
    memset (s,0,strlen (s));	/* erase sensitive information */
    fs_give ((void **) &s);	/* flush erased password */
    md5_update (&ctx,(unsigned char *) tmp,strlen (tmp));
    memset (tmp,0,MAILTMPLEN);	/* erase sensitive information */
    md5_final (digest,&ctx);
				/* convert to printable hex */
    for (i = 0, s = tmp; i < MD5DIGLEN; i++) {
      *s++ = hex[(j = digest[i]) >> 4];
      *s++ = hex[j & 0xf];
    }
    *s = '\0';			/* tie off hash text */
    memset (digest,0,MD5DIGLEN);/* erase sensitive information */
    if (md5try && !strcmp (md5,tmp) &&
	authserver_login (user,authuser,argc,argv))
      ret = cpystr (myusername ());
    else if (md5try) --md5try;
    memset (tmp,0,MAILTMPLEN);	/* erase sensitive information */
  }
  if (!ret) sleep (3);		/* slow down possible cracker */
  return ret;
}

/*
 * RFC 2104 HMAC hashing
 * Accepts: text to hash
 *	    text length
 *	    key
 *	    key length
 * Returns: hash as text, always
 */

char *hmac_md5 (char *text,unsigned long tl,char *key,unsigned long kl)
{
  int i,j;
  static char hshbuf[2*MD5DIGLEN + 1];
  char *s;
  MD5CONTEXT ctx;
  char *hex = "0123456789abcdef";
  unsigned char digest[MD5DIGLEN],k_ipad[MD5BLKLEN+1],k_opad[MD5BLKLEN+1];
  if (kl > MD5BLKLEN) {		/* key longer than pad length? */
    md5_init (&ctx);		/* yes, set key as MD5(key) */
    md5_update (&ctx,(unsigned char *) key,kl);
    md5_final (digest,&ctx);
    key = (char *) digest;
    kl = MD5DIGLEN;
  }
  memcpy (k_ipad,key,kl);	/* store key in pads */
  memset (k_ipad+kl,0,(MD5BLKLEN+1)-kl);
  memcpy (k_opad,k_ipad,MD5BLKLEN+1);
				/* XOR key with ipad and opad values */
  for (i = 0; i < MD5BLKLEN; i++) {
    k_ipad[i] ^= 0x36;
    k_opad[i] ^= 0x5c;
  }
  md5_init (&ctx);		/* inner MD5: hash ipad and text */
  md5_update (&ctx,k_ipad,MD5BLKLEN);
  md5_update (&ctx,(unsigned char *) text,tl);
  md5_final (digest,&ctx);
  md5_init (&ctx);		/* outer MD5: hash opad and inner results */
  md5_update (&ctx,k_opad,MD5BLKLEN);
  md5_update (&ctx,digest,MD5DIGLEN);
  md5_final (digest,&ctx);
				/* convert to printable hex */
  for (i = 0, s = hshbuf; i < MD5DIGLEN; i++) {
    *s++ = hex[(j = digest[i]) >> 4];
    *s++ = hex[j & 0xf];
  }
  *s = '\0';			/* tie off hash text */
  return hshbuf;
}

/* Everything after this point is derived from the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm
 */

/* You may wonder why these strange "a &= 0xffffffff;" statements are here.
 * This is to ensure correct results on machines with a unsigned long size of
 * larger than 32 bits.
 */

#define RND1(a,b,c,d,x,s,ac) \
 a += ((b & c) | (d & ~b)) + x + (unsigned long) ac; \
 a &= 0xffffffff; \
 a = b + ((a << s) | (a >> (32 - s)));

#define RND2(a,b,c,d,x,s,ac) \
 a += ((b & d) | (c & ~d)) + x + (unsigned long) ac; \
 a &= 0xffffffff; \
 a = b + ((a << s) | (a >> (32 - s)));

#define RND3(a,b,c,d,x,s,ac) \
 a += (b ^ c ^ d) + x + (unsigned long) ac; \
 a &= 0xffffffff; \
 a = b + ((a << s) | (a >> (32 - s)));

#define RND4(a,b,c,d,x,s,ac) \
 a += (c ^ (b | ~d)) + x + (unsigned long) ac; \
 a &= 0xffffffff; \
 a = b + ((a << s) | (a >> (32 - s)));

/* Initialize MD5 context
 * Accepts: context to initialize
 */

void md5_init (MD5CONTEXT *ctx)
{
  ctx->clow = ctx->chigh = 0;	/* initialize byte count to zero */
				/* initialization constants */
  ctx->state[0] = 0x67452301; ctx->state[1] = 0xefcdab89;
  ctx->state[2] = 0x98badcfe; ctx->state[3] = 0x10325476;
  ctx->ptr = ctx->buf;		/* reset buffer pointer */
}


/* MD5 add data to context
 * Accepts: context
 *	    input data
 *	    length of data
 */

void md5_update (MD5CONTEXT *ctx,unsigned char *data,unsigned long len)
{
  unsigned long i = (ctx->buf + MD5BLKLEN) - ctx->ptr;
				/* update double precision number of bytes */
  if ((ctx->clow += len) < len) ctx->chigh++;
  while (i <= len) {		/* copy/transform data, 64 bytes at a time */
    memcpy (ctx->ptr,data,i);	/* fill up 64 byte chunk */
    md5_transform (ctx->state,ctx->ptr = ctx->buf);
    data += i,len -= i,i = MD5BLKLEN;
  }
  memcpy (ctx->ptr,data,len);	/* copy final bit of data in buffer */
  ctx->ptr += len;		/* update buffer pointer */
}

/* MD5 Finalization
 * Accepts: destination digest
 *	    context
 */

void md5_final (unsigned char *digest,MD5CONTEXT *ctx)
{
  unsigned long i,bits[2];
  bits[0] = ctx->clow << 3;	/* calculate length in bits (before padding) */
  bits[1] = (ctx->chigh << 3) + (ctx->clow >> 29);
  *ctx->ptr++ = 0x80;		/* padding byte */
  if ((i = (ctx->buf + MD5BLKLEN) - ctx->ptr) < 8) {
    memset (ctx->ptr,0,i);	/* pad out buffer with zeros */
    md5_transform (ctx->state,ctx->buf);
				/* pad out with zeros, leaving 8 bytes */
    memset (ctx->buf,0,MD5BLKLEN - 8);
    ctx->ptr = ctx->buf + MD5BLKLEN - 8;
  }
  else if (i -= 8) {		/* need to pad this buffer? */
    memset (ctx->ptr,0,i);	/* yes, pad out with zeros, leaving 8 bytes */
    ctx->ptr += i;
  }
  md5_encode (ctx->ptr,bits,2);	/* make LSB-first length */
  md5_transform (ctx->state,ctx->buf);
				/* store state in digest */
  md5_encode (digest,ctx->state,4);
				/* erase context */
  memset (ctx,0,sizeof (MD5CONTEXT));
}

/* MD5 basic transformation
 * Accepts: state vector
 *	    current 64-byte block
 */

static void md5_transform (unsigned long *state,unsigned char *block)
{
  unsigned long a = state[0],b = state[1],c = state[2],d = state[3],x[16];
  md5_decode (x,block,16);	/* decode into 16 longs */
				/* round 1 */
  RND1 (a,b,c,d,x[ 0], 7,0xd76aa478); RND1 (d,a,b,c,x[ 1],12,0xe8c7b756);
  RND1 (c,d,a,b,x[ 2],17,0x242070db); RND1 (b,c,d,a,x[ 3],22,0xc1bdceee);
  RND1 (a,b,c,d,x[ 4], 7,0xf57c0faf); RND1 (d,a,b,c,x[ 5],12,0x4787c62a);
  RND1 (c,d,a,b,x[ 6],17,0xa8304613); RND1 (b,c,d,a,x[ 7],22,0xfd469501);
  RND1 (a,b,c,d,x[ 8], 7,0x698098d8); RND1 (d,a,b,c,x[ 9],12,0x8b44f7af);
  RND1 (c,d,a,b,x[10],17,0xffff5bb1); RND1 (b,c,d,a,x[11],22,0x895cd7be);
  RND1 (a,b,c,d,x[12], 7,0x6b901122); RND1 (d,a,b,c,x[13],12,0xfd987193);
  RND1 (c,d,a,b,x[14],17,0xa679438e); RND1 (b,c,d,a,x[15],22,0x49b40821);
				/* round 2 */
  RND2 (a,b,c,d,x[ 1], 5,0xf61e2562); RND2 (d,a,b,c,x[ 6], 9,0xc040b340);
  RND2 (c,d,a,b,x[11],14,0x265e5a51); RND2 (b,c,d,a,x[ 0],20,0xe9b6c7aa);
  RND2 (a,b,c,d,x[ 5], 5,0xd62f105d); RND2 (d,a,b,c,x[10], 9, 0x2441453);
  RND2 (c,d,a,b,x[15],14,0xd8a1e681); RND2 (b,c,d,a,x[ 4],20,0xe7d3fbc8);
  RND2 (a,b,c,d,x[ 9], 5,0x21e1cde6); RND2 (d,a,b,c,x[14], 9,0xc33707d6);
  RND2 (c,d,a,b,x[ 3],14,0xf4d50d87); RND2 (b,c,d,a,x[ 8],20,0x455a14ed);
  RND2 (a,b,c,d,x[13], 5,0xa9e3e905); RND2 (d,a,b,c,x[ 2], 9,0xfcefa3f8);
  RND2 (c,d,a,b,x[ 7],14,0x676f02d9); RND2 (b,c,d,a,x[12],20,0x8d2a4c8a);
				/* round 3 */
  RND3 (a,b,c,d,x[ 5], 4,0xfffa3942); RND3 (d,a,b,c,x[ 8],11,0x8771f681);
  RND3 (c,d,a,b,x[11],16,0x6d9d6122); RND3 (b,c,d,a,x[14],23,0xfde5380c);
  RND3 (a,b,c,d,x[ 1], 4,0xa4beea44); RND3 (d,a,b,c,x[ 4],11,0x4bdecfa9);
  RND3 (c,d,a,b,x[ 7],16,0xf6bb4b60); RND3 (b,c,d,a,x[10],23,0xbebfbc70);
  RND3 (a,b,c,d,x[13], 4,0x289b7ec6); RND3 (d,a,b,c,x[ 0],11,0xeaa127fa);
  RND3 (c,d,a,b,x[ 3],16,0xd4ef3085); RND3 (b,c,d,a,x[ 6],23, 0x4881d05);
  RND3 (a,b,c,d,x[ 9], 4,0xd9d4d039); RND3 (d,a,b,c,x[12],11,0xe6db99e5);
  RND3 (c,d,a,b,x[15],16,0x1fa27cf8); RND3 (b,c,d,a,x[ 2],23,0xc4ac5665);
				/* round 4 */
  RND4 (a,b,c,d,x[ 0], 6,0xf4292244); RND4 (d,a,b,c,x[ 7],10,0x432aff97);
  RND4 (c,d,a,b,x[14],15,0xab9423a7); RND4 (b,c,d,a,x[ 5],21,0xfc93a039);
  RND4 (a,b,c,d,x[12], 6,0x655b59c3); RND4 (d,a,b,c,x[ 3],10,0x8f0ccc92);
  RND4 (c,d,a,b,x[10],15,0xffeff47d); RND4 (b,c,d,a,x[ 1],21,0x85845dd1);
  RND4 (a,b,c,d,x[ 8], 6,0x6fa87e4f); RND4 (d,a,b,c,x[15],10,0xfe2ce6e0);
  RND4 (c,d,a,b,x[ 6],15,0xa3014314); RND4 (b,c,d,a,x[13],21,0x4e0811a1);
  RND4 (a,b,c,d,x[ 4], 6,0xf7537e82); RND4 (d,a,b,c,x[11],10,0xbd3af235);
  RND4 (c,d,a,b,x[ 2],15,0x2ad7d2bb); RND4 (b,c,d,a,x[ 9],21,0xeb86d391);
				/* update state */
  state[0] += a; state[1] += b; state[2] += c; state[3] += d;
  memset (x,0,sizeof (x));	/* erase sensitive data */
}

/* You may wonder why these strange "& 0xff" maskings are here.  This is to
 * ensure correct results on machines with a char size of larger than 8 bits.
 * For example, the KCC compiler on the PDP-10 uses 9-bit chars.
 */

/* MD5 encode unsigned long into LSB-first bytes
 * Accepts: destination pointer
 *	    source
 *	    length of source
 */ 

static void md5_encode (unsigned char *dst,unsigned long *src,int len)
{
  int i;
  for (i = 0; i < len; i++) {
    *dst++ = (unsigned char) (src[i] & 0xff);
    *dst++ = (unsigned char) ((src[i] >> 8) & 0xff);
    *dst++ = (unsigned char) ((src[i] >> 16) & 0xff);
    *dst++ = (unsigned char) ((src[i] >> 24) & 0xff);
  }
}


/* MD5 decode LSB-first bytes into unsigned long
 * Accepts: destination pointer
 *	    source
 *	    length of destination
 */ 

static void md5_decode (unsigned long *dst,unsigned char *src,int len)
{
  int i, j;
  for (i = 0, j = 0; i < len; i++, j += 4)
    dst[i] = ((unsigned long) (src[j] & 0xff)) |
      (((unsigned long) (src[j+1] & 0xff)) << 8) |
      (((unsigned long) (src[j+2] & 0xff)) << 16) |
	(((unsigned long) (src[j+3] & 0xff)) << 24);
}
