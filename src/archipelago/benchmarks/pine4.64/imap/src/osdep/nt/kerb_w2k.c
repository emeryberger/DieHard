/*
 * Program:	GSSAPI Kerberos Shim 5 for Windows 2000/XP IMAP Toolkit
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	6 March 2000
 * Last Edited:	3 May 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/*  The purpose of this module is to be a shim, so that the auth_gss.c module
 * (written for MIT Kerberos) will compile, link, and run with SSPI Kerberos
 * on Windows 2000 systems.
 *  There is no attempt whatsoever to make this be a complete implementation
 * of GSSAPI.  A number of shortcuts were taken that a real GSSAPI
 * implementation for SSPI can't do.
 *  Nor is there any attempt to make the types identical with MIT Kerberos;
 * you can't link this library with object files compiled with the MIT
 * Kerberos .h files.
 */


/* GSSAPI generic definitions */


#define SECURITY_WIN32
#include <security.h>


/* GSSAPI types for which we use SSPI equivalent types */

typedef ULONG OM_uint32;
typedef PCredHandle gss_cred_id_t;
typedef ULONG gss_cred_usage_t;
typedef PCtxtHandle gss_ctx_id_t;
typedef SEC_CHAR * gss_name_t;
typedef ULONG gss_qop_t;


/* Major status codes */

#define GSS_S_COMPLETE SEC_E_OK
#define GSS_S_BAD_MECH SEC_E_SECPKG_NOT_FOUND
#define GSS_S_CONTINUE_NEEDED SEC_I_CONTINUE_NEEDED
#define GSS_S_CREDENTIALS_EXPIRED SEC_E_CERT_EXPIRED
#define GSS_S_FAILURE SEC_E_INTERNAL_ERROR
#define GSS_S_NO_CRED SEC_E_NO_CREDENTIALS
#define GSS_S_NO_CONTEXT SEC_E_INVALID_HANDLE


/* Flag bits for context-level services */

#define GSS_C_DELEG_FLAG ISC_REQ_DELEGATE
#define GSS_C_MUTUAL_FLAG ISC_REQ_MUTUAL_AUTH
#define GSS_C_REPLAY_FLAG ISC_REQ_REPLAY_DETECT
#define GSS_C_SEQUENCE_FLAG ISC_REQ_SEQUENCE_DETECT
#define GSS_C_CONF_FLAG ISC_REQ_CONFIDENTIALITY


/* Credential usage options */

#define GSS_C_BOTH SECPKG_CRED_BOTH
#define GSS_C_INITIATE SECPKG_CRED_OUTBOUND
#define GSS_C_ACCEPT SECPKG_CRED_INBOUND


/* Major status codes defined by shim */

#define GSS_S_BAD_BINDINGS 100
#define GSS_S_BAD_NAME 101
#define GSS_S_BAD_NAMETYPE 102
#define GSS_S_BAD_STATUS 103

/* GSSAPI types as used in GSSAPI */


/* Buffer */

typedef struct gss_buffer_desc_struct {
  size_t length;
  void *value;
} gss_buffer_desc,*gss_buffer_t;


/* Object identifier */

typedef struct gss_OID_desc_struct {
  OM_uint32 length;
  void *elements;
} gss_OID_desc,*gss_OID;

typedef struct gss_OID_set_desc_struct {
  size_t count;
  gss_OID elements;
} gss_OID_set_desc,*gss_OID_set;


/* Unused, but needed in prototypes */

typedef void * gss_channel_bindings_t;


/* Default constants */

#define GSS_C_EMPTY_BUFFER {0,NIL}
#define GSS_C_NO_BUFFER ((gss_buffer_t) NIL)
#define GSS_C_NO_OID ((gss_OID) NIL)
#define GSS_C_NO_CONTEXT ((gss_ctx_id_t) NIL)
#define GSS_C_NO_CREDENTIAL ((gss_cred_id_t) NIL)
#define GSS_C_NO_CHANNEL_BINDINGS ((gss_channel_bindings_t) NIL)
#define GSS_C_QOP_DEFAULT NIL


/* Status code types for gss_display_status */

#define GSS_C_GSS_CODE 1
#define GSS_C_MECH_CODE 2


/* GSSAPI constants */

const gss_OID gss_nt_service_name;
#define GSS_C_NT_HOSTBASED_SERVICE gss_nt_service_name
const gss_OID gss_mech_krb5;
const gss_OID_set gss_mech_set_krb5;

/* GSSAPI prototypes */


OM_uint32 gss_accept_sec_context (OM_uint32 *minor_status,
				  gss_ctx_id_t *context_handle,
				  gss_cred_id_t acceptor_cred_handle,
				  gss_buffer_t input_token_buffer,
				  gss_channel_bindings_t input_chan_bindings,
				  gss_name_t *src_name,gss_OID *mech_type,
				  gss_buffer_t output_token,
				  OM_uint32 *ret_flags,OM_uint32 *time_rec,
				  gss_cred_id_t *delegated_cred_handle);
OM_uint32 gss_acquire_cred (OM_uint32 *minor_status,gss_name_t desired_name,
			    OM_uint32 time_req,gss_OID_set desired_mechs,
			    gss_cred_usage_t cred_usage,
			    gss_cred_id_t *output_cred_handle,
			    gss_OID_set *actual_mechs,OM_uint32 *time_rec);
OM_uint32 gss_delete_sec_context (OM_uint32 *minor_status,
				  gss_ctx_id_t *context_handle,
				  gss_buffer_t output_token);
OM_uint32 gss_display_name (OM_uint32 *minor_status,gss_name_t input_name,
			    gss_buffer_t output_name_buffer,
			    gss_OID *output_name_type);
OM_uint32 gss_display_status (OM_uint32 *minor_status,OM_uint32 status_value,
			      int status_type,gss_OID mech_type,
			      OM_uint32 *message_context,
			      gss_buffer_t status_string);
OM_uint32 gss_import_name (OM_uint32 *minor_status,
			   gss_buffer_t input_name_buffer,
			   gss_OID input_name_type,gss_name_t *output_name);
OM_uint32 gss_init_sec_context (OM_uint32 *minor_status,
				gss_cred_id_t claimant_cred_handle,
				gss_ctx_id_t *context_handle,
				gss_name_t target_name,gss_OID mech_type,
				OM_uint32 req_flags,OM_uint32 time_req,
				gss_channel_bindings_t input_chan_bindings,
				gss_buffer_t input_token,
				gss_OID *actual_mech_type,
				gss_buffer_t output_token,OM_uint32 *ret_flags,
				OM_uint32 *time_rec);
OM_uint32 gss_release_buffer (OM_uint32 *minor_status,gss_buffer_t buffer);
OM_uint32 gss_release_cred (OM_uint32 *minor_status,gss_cred_id_t *cred_handle);
OM_uint32 gss_release_name (OM_uint32 *minor_status,gss_name_t *input_name);
OM_uint32 gss_wrap (OM_uint32 *minor_status,gss_ctx_id_t context_handle,
		    int conf_req_flag,gss_qop_t qop_req,
		    gss_buffer_t input_message_buffer,int *conf_state,
		    gss_buffer_t output_message_buffer);
OM_uint32 gss_unwrap (OM_uint32 *minor_status,gss_ctx_id_t context_handle,
		      gss_buffer_t input_message_buffer,
		      gss_buffer_t output_message_buffer,int *conf_state,
		      gss_qop_t *qop_state);

/* Kerberos definitions */

long kerberos_server_valid (void);
long kerberos_try_kinit (OM_uint32 error);
char *kerberos_login (char *user,char *authuser,int argc,char *argv[]);


#define STRING WINSTRING	/* conflict with mail.h */
#include <NTSecAPI.h>

/* GSSAPI build-in object identifiers */

static gss_OID_desc oids[] = {	/* stupid C language makes this necessary */
  {10,"\052\206\110\206\367\022\001\002\001\004"},
  {9,"\052\206\110\206\367\022\001\002\002"}
};

				/* stupid C language ditto */
static gss_OID_set_desc oidsets[] = {
  {1,(gss_OID) oids+1}
};

				/* these are the real OIDs */
const gss_OID gss_nt_service_name = oids+0;
const gss_OID gss_mech_krb5 = oids+1;
const gss_OID_set gss_mech_set_krb5 = oidsets+0;


/* Other globals */

				/* substitute for GSS_C_NO_CREDENTIAL */
static gss_cred_id_t gss_default_cred = NIL;

/* GSSAPI import name (convert to full service principal name)
 * Accepts: pointer to return minor status
 *	    buffer containining input name
 *	    type of input name
 *	    pointer to return output internal name
 * Returns: major status, always
 */

OM_uint32 gss_import_name (OM_uint32 *minor_status,
			   gss_buffer_t input_name_buffer,
			   gss_OID input_name_type,gss_name_t *output_name)
{
  OM_uint32 major_status = GSS_S_COMPLETE;
  TimeStamp expiry;
  static CredHandle gss_cred;
  char *s,tmp[MAILTMPLEN];
  *minor_status = 0;		/* never any minor status */
  if (!gss_default_cred) {	/* default credentials set up yet? */
    if (AcquireCredentialsHandle/* no, acquire them now */
	(NIL,MICROSOFT_KERBEROS_NAME_A,SECPKG_CRED_OUTBOUND,NIL,NIL,NIL,NIL,
	 &gss_cred,&expiry) != SEC_E_OK) return GSS_S_FAILURE;
				/* have default credentials now */
    gss_default_cred = &gss_cred;
  }
				/* must be the gss_nt_service_name format */
  if (input_name_type != gss_nt_service_name)
    major_status = GSS_S_BAD_NAMETYPE;
				/* name must be of sane length */
  else if (input_name_buffer->length > (MAILTMPLEN/2))
    major_status = GSS_S_BAD_NAME;
  else {			/* copy name */
    memcpy (tmp,input_name_buffer->value,input_name_buffer->length);
    tmp[input_name_buffer->length] = '\0';
    if (s = strchr (tmp,'@')) {	/* find service/host/delimiter */
      *s = '/';			/* convert to full service principal name */
      *output_name = cpystr (tmp);
    }
    else major_status = GSS_S_BAD_NAME;
  }
  return major_status;
}

/* GSSAPI Initialize security context
 * Accepts: pointer to return minor status
 *	    claimant credential handle
 *	    context (NIL means "none assigned yet")
 *	    desired principal
 *	    desired mechanisms
 *	    required context attributes
 *	    desired lifetime
 *	    input channel bindings
 *	    input token buffer
 *	    pointer to return mechanism type
 *	    buffer to return output token
 *	    pointer to return flags
 *	    pointer to return context lifetime
 * Returns: major status, always
 */

OM_uint32 gss_init_sec_context (OM_uint32 *minor_status,
				gss_cred_id_t claimant_cred_handle,
				gss_ctx_id_t *context_handle,
				gss_name_t target_name,gss_OID mech_type,
				OM_uint32 req_flags,OM_uint32 time_req,
				gss_channel_bindings_t input_chan_bindings,
				gss_buffer_t input_token,
				gss_OID *actual_mech_type,
				gss_buffer_t output_token,OM_uint32 *ret_flags,
				OM_uint32 *time_rec)
{
  OM_uint32 i;
  OM_uint32 major_status;
  TimeStamp expiry;
  SecBuffer ibuf[1],obuf[1];
  SecBufferDesc ibufs,obufs;
  *minor_status = 0;		/* never any minor status */
				/* error if non-default time requested */
  if (time_req) return GSS_S_FAILURE;
  if (mech_type && memcmp (mech_type,gss_mech_krb5,sizeof (gss_OID)))
    return GSS_S_BAD_MECH;
				/* ditto if any channel bindings */
  if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS)
    return GSS_S_BAD_BINDINGS;

				/* apply default credential if necessary */
  if (claimant_cred_handle == GSS_C_NO_CREDENTIAL)
    claimant_cred_handle = gss_default_cred;
				/* create output buffer storage as needed */
  req_flags |= ISC_REQ_ALLOCATE_MEMORY;
				/* make output buffer */
  obuf[0].BufferType = SECBUFFER_TOKEN;
  obuf[0].cbBuffer = 0; obuf[0].pvBuffer = NIL;
				/* output buffer descriptor */
  obufs.ulVersion = SECBUFFER_VERSION;
  obufs.cBuffers = 1;
  obufs.pBuffers = obuf;
				/* first time caller? */
  if (*context_handle == GSS_C_NO_CONTEXT) {
				/* yes, set up output context handle */
    PCtxtHandle ctx = (PCtxtHandle) fs_get (sizeof (CtxtHandle));
    major_status = InitializeSecurityContext (claimant_cred_handle,NIL,
					      target_name,req_flags,0,
					      SECURITY_NETWORK_DREP,NIL,0,ctx,
					      &obufs,
					      ret_flags ? ret_flags : &i,
					      &expiry);
    *context_handle = ctx;	/* return updated context */
  }
  else {			/* no, make SSPI buffer from GSSAPI buffer */
    ibuf[0].BufferType = obuf[0].BufferType = SECBUFFER_TOKEN;
    ibuf[0].cbBuffer = input_token->length;
    ibuf[0].pvBuffer = input_token->value;
				/* input buffer descriptor */
    ibufs.ulVersion = SECBUFFER_VERSION;
    ibufs.cBuffers = 1;
    ibufs.pBuffers = ibuf;
    major_status = InitializeSecurityContext (claimant_cred_handle,
					      *context_handle,target_name,
					      req_flags,0,
					      SECURITY_NETWORK_DREP,&ibufs,0,
					      *context_handle,&obufs,
					      ret_flags ? ret_flags : &i,
					      &expiry);
  }
				/* return output */
  output_token->value = obuf[0].pvBuffer;
  output_token->length = obuf[0].cbBuffer;
				/* in case client wanted lifetime returned */
  if (time_rec) *time_rec = expiry.LowPart;
  return major_status;
}

/* GSSAPI display status text
 * Accepts: pointer to return minor status
 *	    status to display
 *	    status type
 *	    message context for continuation
 *	    buffer to write status string
 * Returns: major status, always
 */

OM_uint32 gss_display_status (OM_uint32 *minor_status,OM_uint32 status_value,
			      int status_type,gss_OID mech_type,
			      OM_uint32 *message_context,
			      gss_buffer_t status_string)
{
  char *s,tmp[MAILTMPLEN];
  *minor_status = 0;		/* never any minor status */
  if (*message_context) return GSS_S_FAILURE;
  switch (status_type) {	/* what type of status code? */
  case GSS_C_GSS_CODE:		/* major_status */
    switch (status_value) {	/* analyze status value */
    case GSS_S_FAILURE:
      s = "Unspecified failure"; break;
    case GSS_S_CREDENTIALS_EXPIRED:
      s = "Credentials expired"; break;
    case GSS_S_BAD_BINDINGS:
      s = "Bad bindings"; break;
    case GSS_S_BAD_MECH:
      s = "Bad mechanism type"; break;
    case GSS_S_BAD_NAME:
      s = "Bad name"; break;
    case GSS_S_BAD_NAMETYPE:
      s = "Bad name type"; break;
    case GSS_S_BAD_STATUS:
      s = "Bad status"; break;
    case GSS_S_NO_CONTEXT:
      s = "Invalid context handle"; break;
    case GSS_S_NO_CRED:
      s = "Unable to authenticate to Kerberos service";
      mail_parameters (NIL,DISABLE_AUTHENTICATOR,"GSSAPI");
      break;
    case SEC_E_NO_AUTHENTICATING_AUTHORITY:
      s = "No authenticating authority"; break;
    case SEC_E_TARGET_UNKNOWN:
      s = "Destination server unknown to Kerberos service"; break;
    default:
      sprintf (s = tmp,"SSPI code %lx",status_value);
    }
    break;
  case GSS_C_MECH_CODE:		/* minor status - drop into default */
  default:
    return GSS_S_BAD_STATUS;	/* bad status type */
  }
				/* return status string */
  status_string->length = strlen (status_string->value = cpystr (s));
  return GSS_S_COMPLETE;
}

/* GSSAPI delete security context
 * Accepts: pointer to return minor status
 *	    context to delete
 *	    output context token
 * Returns: major status, always
 */

OM_uint32 gss_delete_sec_context (OM_uint32 *minor_status,
				  gss_ctx_id_t *context_handle,
				  gss_buffer_t output_token)
{
  OM_uint32 major_status;
  *minor_status = 0;		/* never any minor status */
				/* output token not supported */
  major_status = output_token ? GSS_S_FAILURE :
    DeleteSecurityContext (*context_handle);
  fs_give ((void **) context_handle);
  return major_status;
}


/* GSSAPI release buffer
 * Accepts: pointer to return minor status
 *	    buffer to release
 * Returns: GSS_S_COMPLETE, always
 */

OM_uint32 gss_release_buffer (OM_uint32 *minor_status,gss_buffer_t buffer)
{
  *minor_status = 0;		/* never any minor status */
  fs_give (&buffer->value);
  return GSS_S_COMPLETE;
}


/* GSSAPI release name
 * Accepts: pointer to return minor status
 *	    pointer to name to release
 * Returns: GSS_S_COMPLETE, always
 */

OM_uint32 gss_release_name (OM_uint32 *minor_status,gss_name_t *input_name)
{
  *minor_status = 0;		/* never any minor status */
  fs_give (input_name);
  return GSS_S_COMPLETE;
}

/* GSSAPI wrap data
 * Accepts: pointer to return minor status
 *	    context handle
 *	    requested confidentiality
 *	    requested quality of protection
 *	    input message buffer
 *	    pointer to return confidentiality state
 *	    output message buffer
 * Returns: major status, always
 */

OM_uint32 gss_wrap (OM_uint32 *minor_status,gss_ctx_id_t context_handle,
		    int conf_req_flag,gss_qop_t qop_req,
		    gss_buffer_t input_message_buffer,int *conf_state,
		    gss_buffer_t output_message_buffer)
{
  OM_uint32 major_status;
  SecBuffer buf[3];
  SecBufferDesc bufs;
  SecPkgContext_Sizes sizes;
  *minor_status = NIL;		/* never any minor status */
  *conf_state = conf_req_flag;	/* same as requested */
  if ((major_status =		/* get trailer and padding sizes */
       QueryContextAttributes (context_handle,SECPKG_ATTR_SIZES,&sizes)) ==
      SEC_E_OK) {
				/* create big enough output buffer */
    output_message_buffer->value =
      fs_get (sizes.cbSecurityTrailer + input_message_buffer->length +
	      sizes.cbBlockSize);
    /* MSDN claims that for EncryptMessage() in Kerberos, you need an
     * uninitialized SECBUFFER_STREAM_HEADER; a SECBUFFER_DATA that "contains
     * the message to be encrypted.  The message is encrypted in place,
     * overwirting the original contents of its buffer"; an uninitialized
     * SECBUFFER_STREAM_TRAILER, and an uninitialized SECBUFFER_EMPTY.  I've
     * never been able to get it to work that way.
     */
    bufs.cBuffers = 3;		/* set up buffer descriptor */
    bufs.pBuffers = buf;
    bufs.ulVersion = SECBUFFER_VERSION;
    buf[0].BufferType = SECBUFFER_TOKEN;
    buf[0].pvBuffer = output_message_buffer->value;
    buf[0].cbBuffer = sizes.cbSecurityTrailer;
				/* I/O buffer */
    buf[1].BufferType = SECBUFFER_DATA;
    buf[1].pvBuffer = ((char *) buf[0].pvBuffer) + buf[0].cbBuffer;
    buf[1].cbBuffer = input_message_buffer->length;
    memcpy (buf[1].pvBuffer,input_message_buffer->value,buf[1].cbBuffer);
    buf[2].BufferType = SECBUFFER_PADDING;
    buf[2].pvBuffer = ((char *) buf[1].pvBuffer) + buf[1].cbBuffer;
    buf[2].cbBuffer = sizes.cbBlockSize;
    if ((major_status = EncryptMessage (context_handle,qop_req,&bufs,0)) ==
	GSS_S_COMPLETE) {
				/* slide data as necessary (how annoying!) */
      unsigned long i = sizes.cbSecurityTrailer - buf[0].cbBuffer;
      if (i) buf[1].pvBuffer =
	       memmove (((char *) buf[0].pvBuffer) + buf[0].cbBuffer,
			buf[1].pvBuffer,buf[1].cbBuffer);
      if (i += (input_message_buffer->length - buf[1].cbBuffer))
	buf[1].pvBuffer = memmove (((char *)buf[1].pvBuffer) + buf[1].cbBuffer,
		   buf[2].pvBuffer,buf[2].cbBuffer);
      output_message_buffer->length = buf[0].cbBuffer + buf[1].cbBuffer +
	buf[2].cbBuffer;
    }
    else fs_give (&output_message_buffer->value);
  }
  return major_status;		/* return status */
}

/* GSSAPI unwrap data
 * Accepts: pointer to return minor status
 *	    context handle
 *	    input message buffer
 *	    output message buffer
 *	    pointer to return confidentiality state
 *	    pointer to return quality of protection
 * Returns: major status, always
 */

OM_uint32 gss_unwrap (OM_uint32 *minor_status,gss_ctx_id_t context_handle,
		      gss_buffer_t input_message_buffer,
		      gss_buffer_t output_message_buffer,int *conf_state,
		      gss_qop_t *qop_state)
{
  OM_uint32 major_status;
  SecBuffer buf[2];
  SecBufferDesc bufs;
  *minor_status = NIL;		/* never any minor status */
  *conf_state = NIL;		/* or confidentiality state */
  /* MSDN implies that all that is needed for DecryptMessage() in Kerberos
   * is a single SECBUFFER_DATA which "contains the encrypted message.  The
   * encrypted message is decrypted in place, overwriting the original
   * contents of its buffer."  I've never been able to get it to work without
   * using a SECBUFFER_STREAM for input and an uninitialized SECBUFFER_DATA
   * for output.
   * It *does* overwrite the input buffer, but not at the same point; e.g.
   * with an input pointer of 0xa140a8 and size of 53, the output ends up
   * at 0xa140d5 and size of 4.
   */
  bufs.cBuffers = 2;		/* set up buffer descriptor */
  bufs.pBuffers = buf;
  bufs.ulVersion = SECBUFFER_VERSION;
				/* input buffer */
  buf[0].BufferType = SECBUFFER_STREAM;
  buf[0].pvBuffer = input_message_buffer->value;
  buf[0].cbBuffer = input_message_buffer->length;
				/* output buffer */
  buf[1].BufferType = SECBUFFER_DATA;
  buf[1].pvBuffer = NIL;
  buf[1].cbBuffer = 0;
				/* decrypt and copy to output buffer */
  if ((major_status = DecryptMessage (context_handle,&bufs,0,qop_state)) ==
      SEC_E_OK)
   memcpy (output_message_buffer->value = fs_get (buf[1].cbBuffer),
	   buf[1].pvBuffer,output_message_buffer->length = buf[1].cbBuffer);
  return major_status;		/* return status */
}

/* From here on are server-only functions, currently unused */


/* GSSAPI acquire credentials
 * Accepts: pointer to return minor status
 *	    desired principal
 *	    desired lifetime
 *	    desired mechanisms
 *	    credentials usage
 *	    pointer to return credentials handle
 *	    pointer to return mechanisms
 *	    pointer to return lifetime
 * Returns: GSS_S_FAILURE, always
 */

OM_uint32 gss_acquire_cred (OM_uint32 *minor_status,gss_name_t desired_name,
			    OM_uint32 time_req,gss_OID_set desired_mechs,
			    gss_cred_usage_t cred_usage,
			    gss_cred_id_t *output_cred_handle,
			    gss_OID_set *actual_mechs,OM_uint32 *time_rec)
{
  *minor_status = 0;		/* never any minor status */
  return GSS_S_FAILURE;		/* server only */
}


/* GSSAPI release credentials
 * Accepts: pointer to return minor status
 *	    credentials handle to free
 * Returns: GSS_S_COMPLETE, always
 */

OM_uint32 gss_release_cred (OM_uint32 *minor_status,gss_cred_id_t *cred_handle)
{
  *minor_status = 0;		/* never any minor status */
  return GSS_S_FAILURE;		/* server only */
}

/* GSSAPI Accept security context
 * Accepts: pointer to return minor status
 *	    context
 *	    acceptor credentials
 *	    input token buffer
 *	    input channel bindings
 *	    pointer to return source name
 *	    pointer to return mechanism type
 *	    buffer to return output token
 *	    pointer to return flags
 *	    pointer to return context lifetime
 *	    pointer to return delegated credentials
 * Returns: GSS_S_FAILURE, always
 */

OM_uint32 gss_accept_sec_context (OM_uint32 *minor_status,
				  gss_ctx_id_t *context_handle,
				  gss_cred_id_t acceptor_cred_handle,
				  gss_buffer_t input_token_buffer,
				  gss_channel_bindings_t input_chan_bindings,
				  gss_name_t *src_name,gss_OID *mech_type,
				  gss_buffer_t output_token,
				  OM_uint32 *ret_flags,OM_uint32 *time_rec,
				  gss_cred_id_t *delegated_cred_handle)
{
  *minor_status = 0;		/* never any minor status */
  return GSS_S_FAILURE;		/* server only */
}


/* GSSAPI return printable name
 * Accepts: pointer to return minor status
 *	    internal name
 *	    buffer to return output name
 *	    output name type
 * Returns: GSS_S_FAILURE, always
 */

OM_uint32 gss_display_name (OM_uint32 *minor_status,gss_name_t input_name,
			    gss_buffer_t output_name_buffer,
			    gss_OID *output_name_type)
{
  *minor_status = 0;		/* never any minor status */
  return GSS_S_FAILURE;		/* server only */
}

/* Kerberos server valid check
 * Returns: T if have keytab, NIL otherwise
 */

long kerberos_server_valid ()
{
  return NIL;
}


/* Kerberos check for missing or expired credentials
 * Returns: T if should suggest running kinit, NIL otherwise
 */

long kerberos_try_kinit (OM_uint32 error)
{
  return NIL;
}

/* Kerberos server log in
 * Accepts: authorization ID as user name
 *	    authentication ID as Kerberos principal
 *	    argument count
 *	    argument vector
 * Returns: logged in user name if logged in, NIL otherwise
 */

char *kerberos_login (char *user,char *authuser,int argc,char *argv[])
{
  return NIL;
}
