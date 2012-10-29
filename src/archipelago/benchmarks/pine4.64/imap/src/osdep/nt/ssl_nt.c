/*
 * Program:	SSL authentication/encryption module for Windows 9x and NT
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	22 September 1998
 * Last Edited:	11 December 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#define SECURITY_WIN32
#include <sspi.h>
#include <schannel.h>


#define SSLBUFLEN 8192


/* SSL I/O stream */

typedef struct ssl_stream {
  TCPSTREAM *tcpstream;		/* TCP stream */
  CredHandle cred;		/* SSL credentials */
  CtxtHandle context;		/* SSL context */
				/* stream encryption sizes */
  SecPkgContext_StreamSizes sizes;
  size_t bufsize;
  int ictr;			/* input counter */
  char *iptr;			/* input pointer */
  int iextractr;		/* extra input counter */
  char *iextraptr;		/* extra input pointer */
  char *ibuf;			/* input buffer */
  char *obuf;			/* output buffer */
} SSLSTREAM;

#include "sslio.h"


/* Function prototypes */

static SSLSTREAM *ssl_start(TCPSTREAM *tstream,char *host,unsigned long flags);
static char *ssl_analyze_status (SECURITY_STATUS err,char *buf);
static long ssl_abort (SSLSTREAM *stream);

/* Secure Sockets Layer network driver dispatch */

static struct ssl_driver ssldriver = {
  ssl_open,			/* open connection */
  ssl_aopen,			/* open preauthenticated connection */
  ssl_getline,			/* get a line */
  ssl_getbuffer,		/* get a buffer */
  ssl_soutr,			/* output pushed data */
  ssl_sout,			/* output string */
  ssl_close,			/* close connection */
  ssl_host,			/* return host name */
  ssl_remotehost,		/* return remote host name */
  ssl_port,			/* return port number */
  ssl_localhost			/* return local host name */
};

				/* security function table */
static SecurityFunctionTable *sft = NIL;
static unsigned long ssltsz = 0;/* SSL maximum token length */


/* Define crypt32.dll stuff here in case a pre-IE5 Win9x system */

typedef DWORD (CALLBACK *CNTS) (DWORD,PCERT_NAME_BLOB,DWORD,LPSTR,DWORD);
typedef BOOL (CALLBACK *CGCC) (HCERTCHAINENGINE,PCCERT_CONTEXT,LPFILETIME,
			       HCERTSTORE,PCERT_CHAIN_PARA,DWORD,LPVOID,
			       PCCERT_CHAIN_CONTEXT *);
typedef BOOL (CALLBACK *CVCCP) (LPCSTR,PCCERT_CHAIN_CONTEXT,
				PCERT_CHAIN_POLICY_PARA,
				PCERT_CHAIN_POLICY_STATUS);
typedef VOID (CALLBACK *CFCC) (PCCERT_CHAIN_CONTEXT);

static CNTS certNameToStr = NIL;
static CGCC certGetCertificateChain = NIL;
static CVCCP certVerifyCertificateChainPolicy = NIL;
static CFCC certFreeCertificateChain = NIL;

/* One-time SSL initialization */

static int sslonceonly = 0;

void ssl_onceonlyinit (void)
{
  if (!sslonceonly++) {		/* only need to call it once */
    HINSTANCE lib;
    FARPROC pi;
    ULONG np;
    SecPkgInfo *pp;
    int i;
				/* get security library */
    if (((lib = LoadLibrary ("schannel.dll")) ||
	 (lib = LoadLibrary ("security.dll"))) &&
	(pi = GetProcAddress (lib,SECURITY_ENTRYPOINT)) &&
	(sft = (SecurityFunctionTable *) pi ()) &&
	!(sft->EnumerateSecurityPackages (&np,&pp))) {
				/* look for an SSL package */
      for (i = 0; (i < (int) np); i++) if (!strcmp (pp[i].Name,UNISP_NAME)) {
				/* note maximum token size and name */
	ssltsz = pp[i].cbMaxToken;
				/* apply runtime linkage */
	mail_parameters (NIL,SET_SSLDRIVER,(void *) &ssldriver);
	mail_parameters (NIL,SET_SSLSTART,(void *) ssl_start);
	if ((lib = LoadLibrary ("crypt32.dll")) &&
	    (certGetCertificateChain = (CGCC)
	     GetProcAddress (lib,"CertGetCertificateChain")) &&
	    (certVerifyCertificateChainPolicy = (CVCCP)
	     GetProcAddress (lib,"CertVerifyCertificateChainPolicy")) &&
	    (certFreeCertificateChain = (CFCC)
	     GetProcAddress (lib,"CertFreeCertificateChain")))
	  certNameToStr = (CNTS) GetProcAddress (lib,"CertNameToStrA");
	return;			/* all done */
      }
    }
  }
}

/* SSL open
 * Accepts: host name
 *	    contact service name
 *	    contact port number
 * Returns: SSL stream if success else NIL
 */

SSLSTREAM *ssl_open (char *host,char *service,unsigned long port)
{
  TCPSTREAM *stream = tcp_open (host,service,port);
  return stream ? ssl_start (stream,host,port) : NIL;
}

  
/* SSL authenticated open
 * Accepts: host name
 *	    service name
 *	    returned user name buffer
 * Returns: SSL stream if success else NIL
 */

SSLSTREAM *ssl_aopen (NETMBX *mb,char *service,char *usrbuf)
{
  return NIL;			/* don't use this mechanism with SSL */
}

/* Start SSL/TLS negotiations
 * Accepts: open TCP stream of session
 *	    user's host name
 *	    flags
 * Returns: SSL stream if success else NIL
 */

static SSLSTREAM *ssl_start (TCPSTREAM *tstream,char *host,unsigned long flags)
{
  SECURITY_STATUS e;
  ULONG a;
  TimeStamp t;
  SecBuffer ibuf[2],obuf[1];
  SecBufferDesc ibufs,obufs;
  SCHANNEL_CRED tlscred;
  CERT_CONTEXT *cert;
  CERT_CHAIN_PARA chparam;
  CERT_CHAIN_CONTEXT *chain;
  SSL_EXTRA_CERT_CHAIN_POLICY_PARA policy;
  CERT_CHAIN_POLICY_PARA polparam;
  CERT_CHAIN_POLICY_STATUS status;
  char tmp[MAILTMPLEN],certname[256];
  char *reason = NIL;
  ULONG req = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT |
    ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY |
      ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_EXTENDED_ERROR |
	ISC_REQ_MANUAL_CRED_VALIDATION;
  LPSTR usage[] = {
    szOID_PKIX_KP_SERVER_AUTH,
    szOID_SERVER_GATED_CRYPTO,
    szOID_SGC_NETSCAPE
  };
  PWSTR whost = NIL;
  char *buf = (char *) fs_get (ssltsz);
  unsigned long size = 0;
  sslcertificatequery_t scq =
    (sslcertificatequery_t) mail_parameters (NIL,GET_SSLCERTIFICATEQUERY,NIL);
  sslfailure_t sf = (sslfailure_t) mail_parameters (NIL,GET_SSLFAILURE,NIL);
  SSLSTREAM *stream = (SSLSTREAM *) memset (fs_get (sizeof (SSLSTREAM)),0,
					    sizeof (SSLSTREAM));
  stream->tcpstream = tstream;	/* bind TCP stream */
				/* initialize TLS credential */
  memset (&tlscred,0,sizeof (SCHANNEL_CRED));
  tlscred.dwVersion = SCHANNEL_CRED_VERSION;
  tlscred.grbitEnabledProtocols = SP_PROT_TLS1;

				/* acquire credentials */
  if (sft->AcquireCredentialsHandle
      (NIL,UNISP_NAME,SECPKG_CRED_OUTBOUND,NIL,(flags & NET_TLSCLIENT) ?
       &tlscred : NIL,NIL,NIL,&stream->cred,&t)
      != SEC_E_OK) reason = "Acquire credentials handle failed";
  else while (!reason) {	/* negotiate security context */
				/* initialize buffers */
    ibuf[0].cbBuffer = size; ibuf[0].pvBuffer = buf;
    ibuf[1].cbBuffer = 0; ibuf[1].pvBuffer = NIL;
    obuf[0].cbBuffer = 0; obuf[0].pvBuffer = NIL;
    ibuf[0].BufferType = obuf[0].BufferType = SECBUFFER_TOKEN;
    ibuf[1].BufferType = SECBUFFER_EMPTY;
				/* initialize buffer descriptors */
    ibufs.ulVersion = obufs.ulVersion = SECBUFFER_VERSION;
    ibufs.cBuffers = 2; obufs.cBuffers = 1;
    ibufs.pBuffers = ibuf; obufs.pBuffers = obuf;
				/* negotiate security */
    e = sft->InitializeSecurityContext
      (&stream->cred,size ? &stream->context : NIL,host,req,0,
       SECURITY_NETWORK_DREP,size? &ibufs:NIL,0,&stream->context,&obufs,&a,&t);
				/* have an output buffer we need to send? */
    if (obuf[0].pvBuffer && obuf[0].cbBuffer) {
      if (!tcp_sout (stream->tcpstream,obuf[0].pvBuffer,obuf[0].cbBuffer))
	reason = "Unexpected TCP output disconnect";
				/* free the buffer */
      sft->FreeContextBuffer (obuf[0].pvBuffer);
    }
    if (!reason) switch (e) {	/* negotiation state */
    case SEC_I_INCOMPLETE_CREDENTIALS:
      break;			/* server wants client auth */
    case SEC_I_CONTINUE_NEEDED:
      if (size) {		/* continue, read any data? */
				/* yes, anything regurgiated back to us? */
	if (ibuf[1].BufferType == SECBUFFER_EXTRA) {
				/* yes, set this as the new data */
	  memmove (buf,buf + size - ibuf[1].cbBuffer,ibuf[1].cbBuffer);
	  size = ibuf[1].cbBuffer;
	  break;
	}
	size = 0;		/* otherwise, read more stuff from server */
      }
    case SEC_E_INCOMPLETE_MESSAGE:
				/* need to read more data from server */
      if (!tcp_getdata (stream->tcpstream))
	reason = "Unexpected TCP input disconnect";
      else {
	memcpy (buf+size,stream->tcpstream->iptr,stream->tcpstream->ictr);
	size += stream->tcpstream->ictr;
				/* empty it from TCP's buffers */
	stream->tcpstream->iptr += stream->tcpstream->ictr;
	stream->tcpstream->ictr = 0;
      }
      break;

    case SEC_E_OK:		/* success, any data to be regurgitated? */
      if (ibuf[1].BufferType == SECBUFFER_EXTRA) {
				/* yes, set this as the new data */
	memmove (stream->tcpstream->iptr = stream->tcpstream->ibuf,
		 buf + size - ibuf[1].cbBuffer,ibuf[1].cbBuffer);
	stream->tcpstream->ictr = ibuf[1].cbBuffer;
      }
      if (certNameToStr && !(flags & NET_NOVALIDATECERT)) {
				/* need validation, make wchar of host */
	if (!((size = MultiByteToWideChar (CP_ACP,0,host,-1,NIL,0)) &&
	      (whost = (PWSTR) fs_get (size*sizeof (WCHAR))) &&
	      MultiByteToWideChar (CP_ACP,0,host,-1,whost,size)))
	  fatal ("Can't make wchar of host name!");
				/* get certificate */
	if ((sft->QueryContextAttributes
	     (&stream->context,SECPKG_ATTR_REMOTE_CERT_CONTEXT,&cert) !=
	     SEC_E_OK) || !cert) {
	  reason = "*Unable to get certificate";
	  strcpy (certname,"<no certificate>");
	}
	else {			/* get certificate subject name */
	  (*certNameToStr) (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
			    &cert->pCertInfo->Subject,CERT_X500_NAME_STR,
			    certname,255);
				/* build certificate chain */
	  memset (&chparam,0,sizeof (chparam));
	  chparam.cbSize = sizeof (chparam);
	  chparam.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
	  chparam.RequestedUsage.Usage.rgpszUsageIdentifier = usage;
	  chparam.RequestedUsage.Usage.cUsageIdentifier =
	    sizeof (usage) / sizeof (LPSTR);
	  if (!(*certGetCertificateChain)
	      (NIL,cert,NIL,cert->hCertStore,&chparam,NIL,NIL,&chain))
	    reason = ssl_analyze_status (GetLastError (),tmp);
	  else {		/* validate certificate chain */
	    memset (&policy,0,sizeof (SSL_EXTRA_CERT_CHAIN_POLICY_PARA));
	    policy.cbStruct = sizeof (SSL_EXTRA_CERT_CHAIN_POLICY_PARA);
	    policy.dwAuthType = AUTHTYPE_SERVER;
	    policy.fdwChecks = NIL;
	    policy.pwszServerName = whost;
	    memset (&polparam,0,sizeof (polparam));
	    polparam.cbSize = sizeof (polparam);
	    polparam.pvExtraPolicyPara = &policy;
	    memset (&status,0,sizeof (status));
	    status.cbSize = sizeof (status);
	    if (!(*certVerifyCertificateChainPolicy)
		(CERT_CHAIN_POLICY_SSL,chain,&polparam,&status))
	      reason = ssl_analyze_status (GetLastError (),tmp);
	    else if (status.dwError)
	      reason = ssl_analyze_status (status.dwError,tmp);
	    (*certFreeCertificateChain) (chain);
	  }
	}
	if (whost) fs_give ((void **) &whost);

	if (reason) {		/* got an error? */
				/* application callback */
	  if (scq) reason = (*scq) ((*reason == '*') ? reason + 1 : reason,
				    host,certname) ? NIL : "";
	  else if (*certname) {	/* error message to return via mm_log() */
	    sprintf (buf,"*%.128s: %.255s",
		     (*reason == '*') ? reason + 1 : reason,certname);
	    reason = buf;
	  }
	}
      }
      if (reason ||
	  (reason = ssl_analyze_status
	   (sft->QueryContextAttributes
	    (&stream->context,SECPKG_ATTR_STREAM_SIZES,&stream->sizes),buf)))
	break;			/* error in certificate or getting sizes */
      fs_give ((void **) &buf);	/* flush temporary buffer */
				/* make maximum-sized buffers */
      stream->bufsize = stream->sizes.cbHeader +
	stream->sizes.cbMaximumMessage + stream->sizes.cbTrailer;
      if (stream->sizes.cbMaximumMessage < SSLBUFLEN)
	fatal ("cbMaximumMessage is less than SSLBUFLEN!");
      else if (stream->sizes.cbMaximumMessage < 16384) {
	sprintf (tmp,"WINDOWS BUG: cbMaximumMessage = %ld, should be 16384",
		 (long) stream->sizes.cbMaximumMessage);
	mm_log (tmp,NIL);
      }
      stream->ibuf = (char *) fs_get (stream->bufsize);
      stream->obuf = (char *) fs_get (stream->bufsize);
      return stream;
    default:
      reason = ssl_analyze_status (e,buf);
    }
  }
  ssl_close (stream);		/* failed to do SSL */
  stream = NIL;			/* no stream returned */
  switch (*reason) {		/* analyze reason */
  case '*':			/* certificate failure */
    ++reason;			/* skip over certificate failure indication */
				/* pass to error callback */
    if (sf) (*sf) (host,reason,flags);
    else {			/* no error callback, build error message */
      sprintf (tmp,"Certificate failure for %.80s: %.512s",host,reason);
      mm_log (tmp,ERROR);
    }
  case '\0':			/* user answered no to certificate callback */
    if (flags & NET_TRYSSL)	/* return dummy stream to stop tryssl */
      stream = (SSLSTREAM *) memset (fs_get (sizeof (SSLSTREAM)),0,
				     sizeof (SSLSTREAM));
    break;
  default:			/* non-certificate failure */
    if (flags & NET_TRYSSL);	/* no error output if tryssl */
				/* pass to error callback */
    else if (sf) (*sf) (host,reason,flags);
    else {			/* no error callback, build error message */
      sprintf (tmp,"TLS/SSL failure for %.80s: %.512s",host,reason);
      mm_log (tmp,ERROR);
    }
    break;
  }
  fs_give ((void **) &buf);	/* flush temporary buffer */
  return stream;
}

/* Generate error text from SSL error code
 * Accepts: SSL status
 *	    scratch buffer
 * Returns: text if error status, else NIL
 */

static char *ssl_analyze_status (SECURITY_STATUS err,char *buf)
{
  switch (err) {
  case SEC_E_OK:		/* no error */
  case SEC_I_CONTINUE_NEEDED:
  case SEC_I_INCOMPLETE_CREDENTIALS:
  case SEC_E_INCOMPLETE_MESSAGE:
    return NIL;
  case SEC_E_NO_AUTHENTICATING_AUTHORITY:
    mm_log ("unexpected SEC_E_NO_AUTHENTICATING_AUTHORITY",NIL);
    return "*No authority could be contacted for authentication";
  case SEC_E_WRONG_PRINCIPAL:
    mm_log ("unexpected SEC_E_WRONG_PRINCIPAL",NIL);
  case CERT_E_CN_NO_MATCH:
    return "*Server name does not match certificate";
  case SEC_E_UNTRUSTED_ROOT:
    mm_log ("unexpected SEC_E_UNTRUSTED_ROOT",NIL);
  case CERT_E_UNTRUSTEDROOT:
    return "*Self-signed certificate or untrusted authority";
  case SEC_E_CERT_EXPIRED:
    mm_log ("unexpected SEC_E_CERT_EXPIRED",NIL);
  case CERT_E_EXPIRED:
    return "*Certificate has expired";
  case CERT_E_REVOKED:
    return "*Certificate revoked";
  case SEC_E_INVALID_TOKEN:
    return "Invalid token, probably not an SSL server";
  case SEC_E_UNSUPPORTED_FUNCTION:
    return "SSL not supported on this machine - upgrade your system software";
  }
  sprintf (buf,"Unexpected SSPI or certificate error %lx - report this",err);
  return buf;
}

/* SSL receive line
 * Accepts: SSL stream
 * Returns: text line string or NIL if failure
 */

char *ssl_getline (SSLSTREAM *stream)
{
  int n,m;
  char *st,*ret,*stp;
  char c = '\0';
  char d;
				/* make sure have data */
  if (!ssl_getdata (stream)) return NIL;
  st = stream->iptr;		/* save start of string */
  n = 0;			/* init string count */
  while (stream->ictr--) {	/* look for end of line */
    d = *stream->iptr++;	/* slurp another character */
    if ((c == '\015') && (d == '\012')) {
      ret = (char *) fs_get (n--);
      memcpy (ret,st,n);	/* copy into a free storage string */
      ret[n] = '\0';		/* tie off string with null */
      return ret;
    }
    n++;			/* count another character searched */
    c = d;			/* remember previous character */
  }
				/* copy partial string from buffer */
  memcpy ((ret = stp = (char *) fs_get (n)),st,n);
				/* get more data from the net */
  if (!ssl_getdata (stream)) fs_give ((void **) &ret);
				/* special case of newline broken by buffer */
  else if ((c == '\015') && (*stream->iptr == '\012')) {
    stream->iptr++;		/* eat the line feed */
    stream->ictr--;
    ret[n - 1] = '\0';		/* tie off string with null */
  }
				/* else recurse to get remainder */
  else if (st = ssl_getline (stream)) {
    ret = (char *) fs_get (n + 1 + (m = strlen (st)));
    memcpy (ret,stp,n);		/* copy first part */
    memcpy (ret + n,st,m);	/* and second part */
    fs_give ((void **) &stp);	/* flush first part */
    fs_give ((void **) &st);	/* flush second part */
    ret[n + m] = '\0';		/* tie off string with null */
  }
  return ret;
}

/* SSL receive buffer
 * Accepts: SSL stream
 *	    size in bytes
 *	    buffer to read into
 * Returns: T if success, NIL otherwise
 */

long ssl_getbuffer (SSLSTREAM *stream,unsigned long size,char *buffer)
{
  unsigned long n;
  while (size > 0) {		/* until request satisfied */
    if (!ssl_getdata (stream)) return NIL;
    n = min (size,stream->ictr);/* number of bytes to transfer */
				/* do the copy */
    memcpy (buffer,stream->iptr,n);
    buffer += n;		/* update pointer */
    stream->iptr += n;
    size -= n;			/* update # of bytes to do */
    stream->ictr -= n;
  }
  buffer[0] = '\0';		/* tie off string */
  return T;
}

/* SSL receive data
 * Accepts: TCP/IP stream
 * Returns: T if success, NIL otherwise
 */

long ssl_getdata (SSLSTREAM *stream)
{
  SECURITY_STATUS status;
  SecBuffer buf[4];
  SecBufferDesc msg;
  size_t n = 0;
  size_t i;
  while (stream->ictr < 1) {	/* decrypted buffer empty? */
    do {			/* yes, make sure have data from TCP */
      if (stream->iextractr) {	/* have previous unread data? */
	memcpy (stream->ibuf + n,stream->iextraptr,stream->iextractr);
	n += stream->iextractr;	/* update number of bytes read */
	stream->iextractr = 0;	/* no more extra data */
      }
      else {			/* read from TCP */
	if (!tcp_getdata (stream->tcpstream)) return ssl_abort (stream);
				/* maximum amount of data to copy */
	if (!(i = min (stream->bufsize - n,stream->tcpstream->ictr)))
	  fatal ("incomplete SecBuffer exceeds maximum buffer size");
				/* do the copy */
	memcpy (stream->ibuf + n,stream->tcpstream->iptr,i);
	stream->tcpstream->iptr += i;
	stream->tcpstream->ictr -= i;
	n += i;			/* update number of bytes to decrypt */
      }
      buf[0].cbBuffer = n;	/* first SecBuffer gets data */
      buf[0].pvBuffer = stream->ibuf;
      buf[0].BufferType = SECBUFFER_DATA;
				/* subsequent ones are for spares */
      buf[1].BufferType = buf[2].BufferType = buf[3].BufferType =
	SECBUFFER_EMPTY;
      msg.ulVersion = SECBUFFER_VERSION;
      msg.cBuffers = 4;		/* number of SecBuffers */
      msg.pBuffers = buf;	/* first SecBuffer */

    } while ((status = ((DECRYPT_MESSAGE_FN) sft->Reserved4)
	      (&stream->context,&msg,0,NIL)) == SEC_E_INCOMPLETE_MESSAGE);
    switch (status) {
    case SEC_E_OK:		/* won */
    case SEC_I_RENEGOTIATE:	/* won but lost it after this buffer */
				/* hunt for a buffer */
      for (i = 0; (i < 4) && (buf[i].BufferType != SECBUFFER_DATA) ; i++);
      if (i < 4) {		/* found a buffer? */
				/* yes, set up pointer and counter */
	stream->iptr = buf[i].pvBuffer;
	stream->ictr = buf[i].cbBuffer;
				/* any unprocessed data? */
	while (++i < 4) if (buf[i].BufferType == SECBUFFER_EXTRA) {
				/* yes, note for next time around */
	  stream->iextraptr = buf[i].pvBuffer;
	  stream->iextractr = buf[i].cbBuffer;
	}
      }
      break;
    default:			/* anything else means we've lost */
      return ssl_abort (stream);
    }
  }
  return LONGT;
}

/* SSL send string as record
 * Accepts: SSL stream
 *	    string pointer
 * Returns: T if success else NIL
 */

long ssl_soutr (SSLSTREAM *stream,char *string)
{
  return ssl_sout (stream,string,(unsigned long) strlen (string));
}


/* SSL send string
 * Accepts: SSL stream
 *	    string pointer
 *	    byte count
 * Returns: T if success else NIL
 */

long ssl_sout (SSLSTREAM *stream,char *string,unsigned long size)
{
  SecBuffer buf[4];
  SecBufferDesc msg;
  char *s;
  size_t n;
  if (!stream->tcpstream) return NIL;
				/* until request satisfied */
  for (s = stream->ibuf,n = 0; size;) {
				/* header */
    buf[0].BufferType = SECBUFFER_STREAM_HEADER;
    memset (buf[0].pvBuffer = stream->obuf,0,
	    buf[0].cbBuffer = stream->sizes.cbHeader);
				/* message (up to maximum size) */
    buf[1].BufferType = SECBUFFER_DATA;
    memcpy (buf[1].pvBuffer = stream->obuf + stream->sizes.cbHeader,string,
	    buf[1].cbBuffer = min (size,SSLBUFLEN));
				/* trailer */
    buf[2].BufferType = SECBUFFER_STREAM_TRAILER;
    memset (buf[2].pvBuffer = ((char *) buf[1].pvBuffer) + buf[1].cbBuffer,0,
	    buf[2].cbBuffer = stream->sizes.cbTrailer);
				/* spare */
    buf[3].BufferType = SECBUFFER_EMPTY;
    msg.ulVersion = SECBUFFER_VERSION;
    msg.cBuffers = 4;		/* number of SecBuffers */
    msg.pBuffers = buf;		/* first SecBuffer */
    string += buf[1].cbBuffer;
    size -= buf[1].cbBuffer;	/* this many bytes processed */
				/* encrypt and send message */
    if ((((ENCRYPT_MESSAGE_FN) sft->Reserved3)
	 (&stream->context,0,&msg,NIL) != SEC_E_OK) ||
	!tcp_sout (stream->tcpstream,stream->obuf,
		   buf[0].cbBuffer + buf[1].cbBuffer + buf[2].cbBuffer))
      return ssl_abort (stream);/* encryption or sending failed */
  }
  return LONGT;
}

/* SSL close
 * Accepts: SSL stream
 */

void ssl_close (SSLSTREAM *stream)
{
  ssl_abort (stream);		/* nuke the stream */
  fs_give ((void **) &stream);	/* flush the stream */
}


/* SSL abort stream
 * Accepts: SSL stream
 * Returns: NIL always
 */

static long ssl_abort (SSLSTREAM *stream)
{
  if (stream->tcpstream) {	/* close TCP stream */
    sft->DeleteSecurityContext (&stream->context);
    sft->FreeCredentialHandle (&stream->cred);
    tcp_close (stream->tcpstream);
    stream->tcpstream = NIL;
  }
  if (stream->ibuf) fs_give ((void **) &stream->ibuf);
  if (stream->obuf) fs_give ((void **) &stream->obuf);
  return NIL;
}

/* SSL get host name
 * Accepts: SSL stream
 * Returns: host name for this stream
 */

char *ssl_host (SSLSTREAM *stream)
{
  return tcp_host (stream->tcpstream);
}


/* SSL get remote host name
 * Accepts: SSL stream
 * Returns: host name for this stream
 */

char *ssl_remotehost (SSLSTREAM *stream)
{
  return tcp_remotehost (stream->tcpstream);
}


/* SSL return port for this stream
 * Accepts: SSL stream
 * Returns: port number for this stream
 */

unsigned long ssl_port (SSLSTREAM *stream)
{
  return tcp_port (stream->tcpstream);
}


/* SSL get local host name
 * Accepts: SSL stream
 * Returns: local host name
 */

char *ssl_localhost (SSLSTREAM *stream)
{
  return tcp_localhost (stream->tcpstream);
}

#include "ssl_none.c"		/* currently no server support */
