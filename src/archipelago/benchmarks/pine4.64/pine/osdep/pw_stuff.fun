/*----------------------------------------------------------------------
      Pull the name out of the gcos field if we have that sort of /etc/passwd

   Args: gcos_field --  The long name or GCOS field to be parsed
         logname    --  Replaces occurances of & with logname string

 Result: returns pointer to buffer with name
  ----*/
static char *
gcos_name(gcos_field, logname)
    char *logname, *gcos_field;
{
    char *firstcp, *lastcp;

    /* The last character of the full name is the one preceding the first
     * '('. If there is no '(', then the full name ends at the end of the
     * gcos field.
     */
    if(lastcp = strindex(gcos_field, '('))
	*lastcp = '\0';

    /* The first character of the full name is the one following the 
     * last '-' before that ending character. NOTE: that's why we
     * establish the ending character first!
     * If there is no '-' before the ending character, then the fullname
     * begins at the beginning of the gcos field.
     */
    if(firstcp = strrindex(gcos_field, '-'))
	firstcp++;
    else
	firstcp = gcos_field;

    return(firstcp);
}


/*----------------------------------------------------------------------
      Fill in homedir, login, and fullname for the logged in user.
      These are all pointers to static storage so need to be copied
      in the caller.

 Args: ui    -- struct pointer to pass back answers

 Result: fills in the fields
  ----*/
void
get_user_info(ui)
    struct user_info *ui;
{
    struct passwd *unix_pwd;

    unix_pwd = getpwuid(getuid());
    if(unix_pwd == NULL) {
      ui->homedir = cpystr("");
      ui->login = cpystr("");
      ui->fullname = cpystr("");
    }else {
      ui->homedir = cpystr(unix_pwd->pw_dir);
      ui->login = cpystr(unix_pwd->pw_name);
      ui->fullname = cpystr(gcos_name(unix_pwd->pw_gecos, unix_pwd->pw_name));
    }
}


/*----------------------------------------------------------------------
      Look up a userid on the local system and return rfc822 address

 Args: name  -- possible login name on local system

 Result: returns NULL or pointer to alloc'd string
  ----*/
char *
local_name_lookup(name)
    char *name;
{
    struct passwd *pw = getpwnam(name);

    if(pw == NULL)
      return((char *)NULL);

    return(cpystr(gcos_name(pw->pw_gecos, name)));
}


