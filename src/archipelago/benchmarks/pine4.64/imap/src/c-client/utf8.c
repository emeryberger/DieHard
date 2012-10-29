/*
 * Program:	UTF-8 routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 June 1997
 * Last Edited:	9 May 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <stdio.h>
#include <ctype.h>
#include "mail.h"
#include "osdep.h"
#include "misc.h"
#include "rfc822.h"
#include "utf8.h"

/*	*** IMPORTANT ***
 *
 *  There is a very important difference between "character set" and "charset",
 * and the comments in this file reflect these differences.  A "character set"
 * (also known as "coded character set") is a mapping between codepoints and
 * characters.  A "charset" is as defined in MIME, and incorporates one or more
 * coded character sets in a character encoding scheme.  See RFC 2130 for more
 * details.
 */


/* Character set conversion tables */

#include "iso_8859.c"		/* 8-bit single-byte coded graphic */
#include "koi8_r.c"		/* Cyrillic - Russia */
#include "koi8_u.c"		/* Cyrillic - Ukraine */
#include "tis_620.c"		/* Thai */
#include "viscii.c"		/* Vietnamese */
#include "windows.c"		/* Windows */
#include "ibm.c"		/* IBM */
#include "gb_2312.c"		/* Chinese (PRC) - simplified */
#include "gb_12345.c"		/* Chinese (PRC) - traditional */
#include "jis_0208.c"		/* Japanese - basic */
#include "jis_0212.c"		/* Japanese - supplementary */
#include "ksc_5601.c"		/* Korean */
#include "big5.c"		/* Taiwanese (ROC) - industrial standard */
#include "cns11643.c"		/* Taiwanese (ROC) - national standard */

/* EUC parameters */

#ifdef GBTOUNICODE		/* PRC simplified Chinese */
static const struct utf8_eucparam gb_param[] = {
  {BASE_GB2312_KU,BASE_GB2312_TEN,MAX_GB2312_KU,MAX_GB2312_TEN,
     (void *) gb2312tab},
  {0,0,0,0,NIL},
  {0,0,0,0,NIL},
};
#endif


#ifdef GB12345TOUNICODE		/* PRC traditional Chinese */
static const struct utf8_eucparam gbt_param[] = {
  {BASE_GB12345_KU,BASE_GB12345_TEN,MAX_GB12345_KU,MAX_GB12345_TEN,
     (void *) gb12345tab},
  {0,0,0,0,NIL},
  {0,0,0,0,NIL}
};
#endif


#ifdef BIG5TOUNICODE		/* ROC traditional Chinese */
static const struct utf8_eucparam big5_param[] = {
  {BASE_BIG5_KU,BASE_BIG5_TEN_0,MAX_BIG5_KU,MAX_BIG5_TEN_0,(void *) big5tab},
  {BASE_BIG5_KU,BASE_BIG5_TEN_1,MAX_BIG5_KU,MAX_BIG5_TEN_1,NIL}
};
#endif


#ifdef JISTOUNICODE		/* Japanese */
static const struct utf8_eucparam jis_param[] = {
  {BASE_JIS0208_KU,BASE_JIS0208_TEN,MAX_JIS0208_KU,MAX_JIS0208_TEN,
     (void *) jis0208tab},
  {MIN_KANA_8,0,MAX_KANA_8,0,(void *) KANA_8},
#ifdef JIS0212TOUNICODE		/* Japanese extended */
  {BASE_JIS0212_KU,BASE_JIS0212_TEN,MAX_JIS0212_KU,MAX_JIS0212_TEN,
     (void *) jis0212tab}
#else
  {0,0,0,0,NIL}
#endif
};
#endif


#ifdef KSCTOUNICODE		/* Korean */
static const struct utf8_eucparam ksc_param = {
  BASE_KSC5601_KU,BASE_KSC5601_TEN,MAX_KSC5601_KU,MAX_KSC5601_TEN,(void *) ksc5601tab};
#endif

/* List of supported charsets */

static const CHARSET utf8_csvalid[] = {
  {"US-ASCII",CT_ASCII,NIL,NIL,NIL},
  {"UTF-8",CT_UTF8,NIL,SC_UNICODE,NIL},
  {"UTF-7",CT_UTF7,NIL,SC_UNICODE,"UTF-8"},
  {"ISO-8859-1",CT_1BYTE0,NIL,SC_LATIN_1,NIL},
  {"ISO-8859-2",CT_1BYTE,(void *) iso8859_2tab,SC_LATIN_2,NIL},
  {"ISO-8859-3",CT_1BYTE,(void *) iso8859_3tab,SC_LATIN_3,NIL},
  {"ISO-8859-4",CT_1BYTE,(void *) iso8859_4tab,SC_LATIN_4,NIL},
  {"ISO-8859-5",CT_1BYTE,(void *) iso8859_5tab,SC_CYRILLIC,"KOI8-R"},
  {"ISO-8859-6",CT_1BYTE,(void *) iso8859_6tab,SC_ARABIC,NIL},
  {"ISO-8859-7",CT_1BYTE,(void *) iso8859_7tab,SC_GREEK,NIL},
  {"ISO-8859-8",CT_1BYTE,(void *) iso8859_8tab,SC_HEBREW,NIL},
  {"ISO-8859-9",CT_1BYTE,(void *) iso8859_9tab,SC_LATIN_5,NIL},
  {"ISO-8859-10",CT_1BYTE,(void *) iso8859_10tab,SC_LATIN_6,NIL},
  {"ISO-8859-11",CT_1BYTE,(void *) iso8859_11tab,SC_THAI,NIL},
#if 0				/* ISO 8859-12 reserved for ISCII(?) */
  {"ISO-8859-12",CT_1BYTE,(void *) iso8859_12tab,NIL,NIL},
#endif
  {"ISO-8859-13",CT_1BYTE,(void *) iso8859_13tab,SC_LATIN_7,NIL},
  {"ISO-8859-14",CT_1BYTE,(void *) iso8859_14tab,SC_LATIN_8,NIL},
  {"ISO-8859-15",CT_1BYTE,(void *) iso8859_15tab,SC_LATIN_9,NIL},
  {"ISO-8859-16",CT_1BYTE,(void *) iso8859_16tab,SC_LATIN_10,NIL},
  {"KOI8-R",CT_1BYTE,(void *) koi8rtab,SC_CYRILLIC,NIL},
  {"KOI8-U",CT_1BYTE,(void *) koi8utab,SC_CYRILLIC | SC_UKRANIAN,NIL},
  {"KOI8-RU",CT_1BYTE,(void *) koi8utab,SC_CYRILLIC | SC_UKRANIAN,"KOI8-U"},
  {"TIS-620",CT_1BYTE,(void *) tis620tab,SC_THAI,"ISO-8859-11"},
  {"VISCII",CT_1BYTE8,(void *) visciitab,SC_VIETNAMESE,NIL},

#ifdef GBTOUNICODE
  {"GB2312",CT_EUC,(void *) gb_param,SC_CHINESE_SIMPLIFIED,NIL},
  {"CN-GB",CT_EUC,(void *) gb_param,SC_CHINESE_SIMPLIFIED,"GB2312"},
#ifdef CNS1TOUNICODE
  {"ISO-2022-CN",CT_2022,NIL,SC_CHINESE_SIMPLIFIED | SC_CHINESE_TRADITIONAL,
   NIL},
#endif
#endif
#ifdef GB12345TOUNICODE
  {"CN-GB-12345",CT_EUC,(void *) gbt_param,SC_CHINESE_TRADITIONAL,"BIG5"},
#endif
#ifdef BIG5TOUNICODE
  {"BIG5",CT_DBYTE2,(void *) big5_param,SC_CHINESE_TRADITIONAL,NIL},
  {"CN-BIG5",CT_DBYTE2,(void *) big5_param,SC_CHINESE_TRADITIONAL,"BIG5"},
  {"BIG-5",CT_DBYTE2,(void *) big5_param,SC_CHINESE_TRADITIONAL,"BIG5"},
#endif
#ifdef JISTOUNICODE
  {"ISO-2022-JP",CT_2022,NIL,SC_JAPANESE,NIL},
  {"EUC-JP",CT_EUC,(void *) jis_param,SC_JAPANESE,"ISO-2022-JP"},
  {"SHIFT_JIS",CT_SJIS,NIL,SC_JAPANESE,"ISO-2022-JP"},
  {"SHIFT-JIS",CT_SJIS,NIL,SC_JAPANESE,"ISO-2022-JP"},
#ifdef JIS0212TOUNICODE
  {"ISO-2022-JP-1",CT_2022,NIL,SC_JAPANESE,"ISO-2022-JP"},
#ifdef GBTOUNICODE
#ifdef KSCTOUNICODE
  {"ISO-2022-JP-2",CT_2022,NIL,
     SC_LATIN_1 | SC_LATIN_2 | SC_LATIN_3 | SC_LATIN_4 | SC_LATIN_5 |
       SC_LATIN_6 | SC_LATIN_7 | SC_LATIN_8 | SC_LATIN_9 | SC_LATIN_10 |
	 SC_ARABIC | SC_CYRILLIC | SC_GREEK | SC_HEBREW | SC_THAI |
	   SC_VIETNAMESE | SC_CHINESE_SIMPLIFIED | SC_JAPANESE | SC_KOREAN
#ifdef CNS1TOUNICODE
	     | SC_CHINESE_TRADITIONAL
#endif
	       ,"UTF-8"},
#endif
#endif
#endif
#endif
#ifdef KSCTOUNICODE
  {"ISO-2022-KR",CT_2022,NIL,SC_KOREAN,"EUC-KR"},
  {"EUC-KR",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,NIL},
  {"KSC5601",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"KSC_5601",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"KS_C_5601-1987",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"KS_C_5601-1989",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"KS_C_5601-1992",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"KS_C_5601-1997",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
#endif

				/* deep sigh */
  {"WINDOWS-874",CT_1BYTE,(void *) windows_874tab,SC_THAI,"ISO-8859-11"},
#ifdef KSCTOUNICODE
  {"WINDOWS-949",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"CP949",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
  {"X-WINDOWS-949",CT_DBYTE,(void *) &ksc_param,SC_KOREAN,"EUC-KR"},
#endif
  {"WINDOWS-1250",CT_1BYTE,(void *) windows_1250tab,SC_LATIN_2,"ISO-8859-2"},
  {"WINDOWS-1251",CT_1BYTE,(void *) windows_1251tab,SC_CYRILLIC,"KOI8-R"},
  {"WINDOWS-1252",CT_1BYTE,(void *) windows_1252tab,SC_LATIN_1,"ISO-8859-1"},
  {"WINDOWS-1253",CT_1BYTE,(void *) windows_1253tab,SC_GREEK,"ISO-8859-7"},
  {"WINDOWS-1254",CT_1BYTE,(void *) windows_1254tab,SC_LATIN_5,"ISO-8859-9"},
  {"WINDOWS-1255",CT_1BYTE,(void *) windows_1255tab,SC_HEBREW,"ISO-8859-8"},
  {"WINDOWS-1256",CT_1BYTE,(void *) windows_1256tab,SC_ARABIC,"ISO-8859-6"},
  {"WINDOWS-1257",CT_1BYTE,(void *) windows_1257tab,SC_LATIN_7,"ISO-8859-13"},
  {"WINDOWS-1258",CT_1BYTE,(void *) windows_1258tab,SC_VIETNAMESE,"VISCII"},
				/* deeper sigh */
  {"IBM367",CT_ASCII,NIL,NIL,"US-ASCII"},
  {"IBM437",CT_1BYTE,(void *) ibm_437tab,SC_LATIN_1,"ISO-8859-1"},
  {"IBM737",CT_1BYTE,(void *) ibm_737tab,SC_GREEK,"ISO-8859-7"},
  {"IBM775",CT_1BYTE,(void *) ibm_775tab,SC_LATIN_7,"ISO-8859-13"},
  {"IBM850",CT_1BYTE,(void *) ibm_850tab,SC_LATIN_1,"ISO-8859-1"},
  {"IBM852",CT_1BYTE,(void *) ibm_852tab,SC_LATIN_2,"ISO-8859-2"},
  {"IBM855",CT_1BYTE,(void *) ibm_855tab,SC_CYRILLIC,"ISO-8859-5"},
  {"IBM857",CT_1BYTE,(void *) ibm_857tab,SC_LATIN_5,"ISO-8859-9"},
  {"IBM860",CT_1BYTE,(void *) ibm_860tab,SC_LATIN_1,"ISO-8859-1"},
  {"IBM861",CT_1BYTE,(void *) ibm_861tab,SC_LATIN_6,"ISO-8859-10"},
  {"IBM862",CT_1BYTE,(void *) ibm_862tab,SC_HEBREW,"ISO-8859-8"},
  {"IBM863",CT_1BYTE,(void *) ibm_863tab,SC_LATIN_1,"ISO-8859-1"},
  {"IBM864",CT_1BYTE,(void *) ibm_864tab,SC_ARABIC,"ISO-8859-6"},
  {"IBM865",CT_1BYTE,(void *) ibm_865tab,SC_LATIN_6,"ISO-8859-10"},
  {"IBM866",CT_1BYTE,(void *) ibm_866tab,SC_CYRILLIC,"KOI8-R"},
  {"IBM869",CT_1BYTE,(void *) ibm_869tab,SC_GREEK,"ISO-8859-7"},
  {"IBM874",CT_1BYTE,(void *) ibm_874tab,SC_THAI,"ISO-8859-11"},
				/* deepest sigh */
  {"ANSI_X3.4-1968",CT_ASCII,NIL,NIL,"US-ASCII"},
  NIL
};


/* Defaults for untagged text */

				/* untagged 7-bit */
static const CHARSET text_7bit = {"UNTAGGED-7BIT",CT_ASCII,NIL,NIL,"US-ASCII"};
				/* untagged 8-bit */
static const CHARSET text_8bit = {"UNTAGGED-8BIT",CT_UTF8,NIL,0xffffffff,NIL};
				/* untagged ISO 2022 */
static const CHARSET iso2022 = {"ISO-2022",CT_2022,NIL,0xffffffff,NIL};

/* Non-Unicode Script table */

static const SCRIPT utf8_scvalid[] = {
  {"Arabic",NIL,SC_ARABIC},
  {"Chinese Simplified","China, Singapore",SC_CHINESE_SIMPLIFIED},
  {"Chinese Traditional","Taiwan, Hong Kong, Macao",SC_CHINESE_TRADITIONAL},
  {"Cyrillic",NIL,SC_CYRILLIC},
  {"Cyrillic Ukranian",NIL,SC_UKRANIAN},
  {"Greek",NIL,SC_GREEK},
  {"Hebrew",NIL,SC_HEBREW},
  {"Japanese",NIL,SC_JAPANESE},
  {"Korean",NIL,SC_KOREAN},
  {"Latin-1","Western Europe",SC_LATIN_1},
  {"Latin-2","Eastern Europe",SC_LATIN_2},
  {"Latin-3","Southern Europe",SC_LATIN_3},
  {"Latin-4","Northern Europe",SC_LATIN_4},
  {"Latin-5","Turkish",SC_LATIN_5},
  {"Latin-6","Nordic",SC_LATIN_6},
  {"Latin-7","Baltic",SC_LATIN_7},
  {"Latin-8","Celtic",SC_LATIN_8},
  {"Latin-9","Euro",SC_LATIN_9},
  {"Latin-10","Balkan",SC_LATIN_10},
  {"Thai",NIL,SC_THAI},
  {"Vietnamese",NIL,SC_VIETNAMESE},
  NIL
};


/* Look up script name or return entire table
 * Accepts: script name or NIL
 * Returns: pointer to script table entry or NIL if unknown
 */

SCRIPT *utf8_script (char *script)
{
  unsigned long i;
  if (!script) return (SCRIPT *) &utf8_scvalid[0];
  else if (*script && (strlen (script) < 128))
    for (i = 0; utf8_scvalid[i].name; i++)
      if (!compare_cstring (script,utf8_scvalid[i].name))
	return (SCRIPT *) &utf8_scvalid[i];
  return NIL;			/* failed */
}

/* Look up charset name or return entire table
 * Accepts: charset name or NIL
 * Returns: charset table entry or NIL if unknown
 */

CHARSET *utf8_charset (char *charset)
{
  unsigned long i;
  if (!charset) return (CHARSET *) &utf8_csvalid[0];
  else if (*charset && (strlen (charset) < 128))
    for (i = 0; utf8_csvalid[i].name; i++)
      if (!compare_cstring (charset,utf8_csvalid[i].name))
	return (CHARSET *) &utf8_csvalid[i];
  return NIL;			/* failed */
}


/* Convert charset labelled sized text to UTF-8
 * Accepts: source sized text
 *	    charset
 *	    pointer to returned sized text if non-NIL
 *	    flags (currently non-zero if want error for unknown charset)
 * Returns: T if successful, NIL if failure
 */

long utf8_text (SIZEDTEXT *text,char *charset,SIZEDTEXT *ret,long flags)
{
  unsigned long i;
  char *t,tmp[MAILTMPLEN];
  const CHARSET *cs;
  if (!(charset && *charset)) {	/* if charset not specified */
    cs = &text_7bit;		/* start by assuming 7-bit */
				/* try harder if converting */
    if (ret) for (i = 0; i < text->size; i++) {
				/* looks like East Asian ISO 2022? */
      if ((text->data[i] == I2C_ESC) && (++i < text->size) &&
	  (text->data[i] == I2C_MULTI) && (++i < text->size)) {
	cs = &iso2022;		/* yes, set ISO-2022 */
	break;			/* punt scan */
      }
				/* remember if we see 8-bit anywhere */
      else if (text->data[i] & BIT8) cs = &text_8bit;
    }
  }
				/* look up charset */
  else if (!(cs = utf8_charset (charset)) && flags) {
    strcpy (tmp,"[BADCHARSET (");
    for (i = 0, t = tmp + strlen (tmp);
	 utf8_csvalid[i].name && (t < (tmp + MAILTMPLEN - 200));
	 i++,t += strlen (t)) sprintf (t,"%s ",utf8_csvalid[i].name);
    sprintf (t + strlen (t) - 1,")] Unknown charset: %.80s",charset);
    MM_LOG (tmp,ERROR);
  }

  if (ret) {			/* return value requested? */
    ret->data = text->data;	/* yes, default is source */
    ret->size = text->size;
    if (cs) switch (cs->type) {	/* convert if type known */
    case CT_ASCII:		/* 7-bit ASCII no table */
    case CT_UTF8:		/* variable UTF-8 encoded Unicode no table */
      break;
    case CT_1BYTE0:		/* 1 byte no table */
      utf8_text_1byte0 (text,ret,cs->tab);
      break;
    case CT_1BYTE:		/* 1 byte ASCII + table 0x80-0xff */
      utf8_text_1byte (text,ret,cs->tab);
      break;
    case CT_1BYTE8:		/* 1 byte table 0x00 - 0xff */
      utf8_text_1byte8 (text,ret,cs->tab);
      break;
    case CT_EUC:		/* 2 byte ASCII + utf8_eucparam base/CS2/CS3 */
      utf8_text_euc (text,ret,cs->tab);
      break;
    case CT_DBYTE:		/* 2 byte ASCII + utf8_eucparam */
      utf8_text_dbyte (text,ret,cs->tab);
      break;
    case CT_DBYTE2:		/* 2 byte ASCII + utf8_eucparam plane1/2 */
      utf8_text_dbyte2 (text,ret,cs->tab);
      break;
    case CT_UTF7:		/* variable UTF-7 encoded Unicode no table */
      utf8_text_utf7 (text,ret);
      break;
    case CT_2022:		/* variable ISO-2022 encoded no table*/
      utf8_text_2022 (text,ret);
      break;
    case CT_SJIS:		/* 2 byte Shift-JIS encoded JIS no table */
      utf8_text_sjis (text,ret);
      break;
    default:			/* unknown character set type */
      return NIL;
    }
  }
  return cs ? LONGT : NIL;	/* return charset lookup status */
}

/* Return map for UTF-8 -> character set
 * Accepts: character set name
 * Returns: map if character set found, else NIL
 *
 * This routine only supports character sets, not all possible charsets.  In
 * particular, it does not support any Unicode encodings or ISO 2022.
 *
 * No attempt is made to map "equivalent" Unicode characters or Unicode
 * characters that have the same glyph; nor is there any attempt to handle
 * combining characters or otherwise do any stringprep.  Maybe later.
 */

unsigned short *utf8_rmap (char *charset)
{
  unsigned short u,*tab;
  unsigned int i,ku,ten;
  struct utf8_eucparam *param;
  CHARSET *cs;
  static char *rmapcs = NIL;	/* current mapped character set */
				/* current UCS-2 -> character set map */
  static unsigned short *rmap = NIL;
				/* done if already mapped that character set */
  if (rmapcs && !compare_cstring (charset,rmapcs)) return rmap;
				/* lookup character set */
  if (!(cs = utf8_charset (charset))) return NIL;
  switch (cs->type) {		/* found charset, is a character set? */
  default:			/* unsupported charset */
    return NIL;
  case CT_ASCII:		/* 7-bit ASCII no table */
  case CT_1BYTE0:		/* 1 byte no table */
  case CT_1BYTE:		/* 1 byte ASCII + table 0x80-0xff */
  case CT_1BYTE8:		/* 1 byte table 0x00 - 0xff */
  case CT_EUC:			/* 2 byte ASCII + utf8_eucparam base/CS2/CS3 */
  case CT_DBYTE:		/* 2 byte ASCII + utf8_eucparam */
  case CT_DBYTE2:		/* 2 byte ASCII + utf8_eucparam plane1/2 */
  case CT_SJIS:			/* 2 byte Shift-JIS */
    rmapcs = cs->name;		/* remember current rmap charset */
    if (!rmap)			/* create reverse map if none yet */
      rmap = (unsigned short *) fs_get (65536 * sizeof (unsigned short));
				/* initialize table for ASCII */
    for (i = 0; i < 128; i++) rmap[i] = (unsigned short) i;
				/* populate remainder of table with NOCHAR */
    memset (rmap + 128,NOCHAR,(65536 - 128) * sizeof (unsigned short));

    switch (cs->type) {		/* additional reverse map actions */
    case CT_1BYTE0:		/* 1 byte no table */
      for (i = 128; i < 256; i++) rmap[i] = (unsigned short) i;
      break;
    case CT_1BYTE:		/* 1 byte ASCII + table 0x80-0xff */
      for (tab = (unsigned short *) cs->tab,i = 128; i < 256; i++)
	if (tab[i & BITS7] != UBOGON) rmap[tab[i & BITS7]] =
	  (unsigned short) i;
      break;
    case CT_1BYTE8:		/* 1 byte table 0x00 - 0xff */
      for (tab = (unsigned short *) cs->tab,i = 0; i < 256; i++)
	if (tab[i] != UBOGON) rmap[tab[i]] = (unsigned short) i;
      break;
    case CT_EUC:		/* 2 byte ASCII + utf8_eucparam base/CS2/CS3 */
      for (param = (struct utf8_eucparam *) cs->tab,
	   tab = (unsigned short *) param->tab,
	   ku = 0; ku < param->max_ku; ku++)
	for (ten = 0; ten < param->max_ten; ten++)
	  if ((u = tab[(ku * param->max_ten) + ten]) != UBOGON)
	    rmap[u] = ((ku + param->base_ku) << 8) + (ten + param->base_ten) +
	      0x8080;
      break;
    case CT_DBYTE:		/* 2 byte ASCII + utf8_eucparam */
      for (param = (struct utf8_eucparam *) cs->tab,
	   tab = (unsigned short *) param->tab,
	   ku = 0; ku < param->max_ku; ku++)
	for (ten = 0; ten < param->max_ten; ten++)
	  if ((u = tab[(ku * param->max_ten) + ten]) != UBOGON)
	    rmap[u] = ((ku + param->base_ku) << 8) + (ten + param->base_ten);
      break;
    case CT_DBYTE2:		/* 2 byte ASCII + utf8_eucparam plane1/2 */
      for (param = (struct utf8_eucparam *) cs->tab,
	   tab = (unsigned short *) param->tab,
	   ku = 0; ku < param->max_ku; ku++)
	for (ten = 0; ten < param->max_ten; ten++)
	  if ((u = tab[(ku * param->max_ten) + ten]) != UBOGON)
	    rmap[u] = ((ku + param->base_ku) << 8) + (ten + param->base_ten);
      param++;
      for (ku = 0; ku < param->max_ku; ku++)
	for (ten = 0; ten < param->max_ten; ten++)
	  if ((u = tab[(ku * param->max_ten) + ten]) != UBOGON)
	    rmap[u] = ((ku + param->base_ku) << 8) + (ten + param->base_ten);
      break;
    case CT_SJIS:		/* 2 byte Shift-JIS */
      for (ku = 0; ku <= MAX_JIS0208_KU; ku++)
	for (ten = 0; ten <= MAX_JIS0208_TEN; ten++)
	  if ((u = jis0208tab[ku][ten]) != UBOGON) {
	    int sku = ku + BASE_JIS0208_KU;
	    int sten = ten + BASE_JIS0208_TEN;
	    rmap[u] = ((((sku + 1) >> 1) + ((sku < 95) ? 112 : 176)) << 8) +
	      sten + ((sku % 2) ? ((sten > 95) ? 32 : 31) : 126);
	  }
				/* JIS Roman */
      rmap[UCS2_YEN] = JISROMAN_YEN;
      rmap[UCS2_OVERLINE] = JISROMAN_OVERLINE;
				/* JIS hankaku katakana */
      for (u = 0; u <= MAX_KANA_8 - MIN_KANA_8; u++)
	rmap[UCS2_KATAKANA + u] = MIN_KANA_8 + u;
      break;
    }
    break;
  }
				/* hack: map NBSP to SP if otherwise no map */
  if (rmap[0x00a0] == NOCHAR) rmap[0x00a0] = rmap[0x0020];
  return rmap;			/* return map */
}

/* Convert UTF8 sized text to charset
 * Accepts: source sized text
 *	    destination charset
 *	    pointer to returned sized text
 *	    substitute character if not in cs, else NIL to return failure
 * Returns: T if successful, NIL if failure
 *
 * This routine doesn't try to convert to all possible charsets; in particular
 * it doesn't support other Unicode encodings or any ISO 2022 other than
 * ISO-2022-JP.
 */

long utf8_cstext (SIZEDTEXT *text,char *charset,SIZEDTEXT *ret,
		  unsigned short errch)
{
  unsigned long i,u;
  unsigned char *s,*t;
  unsigned short c,*rmap;
  short iso2022 = !compare_cstring (charset,"ISO-2022-JP");
				/* can map this charset? */
  if (!(rmap = utf8_rmap (iso2022 ? "EUC-JP" : charset))) return NIL;
				/* yes, determine size of destination string */
  for (ret->size = 0,s = text->data,i = text->size; i;) {
				/* map, ignore BOM */
    if ((u = utf8_get (&s,&i)) == UCS2_BOM);
    else if ((u & U8GM_NONBMP) || (((c = rmap[u]) == NOCHAR) && !(c = errch)))
      return NIL;		/* not in BMP, or NOCHAR and no err char? */
    else switch (iso2022) {	/* depends upon ISO 2022 mode */
    case 0:			/* ISO 2022 not in effect */
      ret->size += (c > 0xff) ? 2 : 1;
      break;
    case 1:			/* ISO 2022 Roman */
				/* <ch> */
      if (c < 0x80) ret->size += 1;
      else {			/* JIS character */
	ret->size += 5;		/* ESC $ B <hi> <lo> */
	iso2022 = 2;		/* shift to ISO 2022 JIS */
      }
      break;
    case 2:			/* ISO 2022 JIS */
				/* <hi> <lo> */
      if (c > 0x7f) ret->size += 2;
      else {			/* ASCII character */
	ret->size += 4;		/* ESC ( J <ch> */
	iso2022 = 1;		/* shift to ISO 2022 Roman */
      }
      break;
    }
  }
  if (iso2022 == 2) {		/* ISO-2022-JP string must end in Roman */
    ret->size += 3;		/* ESC ( J */
    iso2022 = 1;		/* reset state to Roman */
  }

				/* make destination buffer */
  ret->data = (unsigned char *) fs_get (ret->size + 1);
				/* convert string */
  for (t = ret->data,s = text->data,i = text->size; i;) {
				/* map, ignore BOM */
    if ((u = utf8_get (&s,&i)) == UCS2_BOM);
				/* substitute error character for NOCHAR */
    else if ((u & U8GM_NONBMP) || ((c = rmap[u]) == NOCHAR)) c = errch;
    else switch (iso2022) {	/* depends upon ISO 2022 mode */
    case 0:			/* ISO 2022 not in effect */
				/* two-byte character */
      if (c > 0xff) *t++ = (unsigned char) (c >> 8);
				/* single-byte or low-byte of two-byte */
      *t++ = (unsigned char) (c & 0xff);
      break;
    case 1:			/* ISO 2022 Roman */
				/* <ch> */
      if (c < 0x80) *t++ = (unsigned char) c;
      else {			/* JIS character */
	*t++ = I2C_ESC;		/* ESC $ B <hi> <lo> */
	*t++ = I2C_MULTI;
	*t++ = I2CS_94x94_JIS_NEW;
	*t++ = (unsigned char) (c >> 8) & 0x7f;
	*t++ = (unsigned char) c & 0x7f;
	iso2022 = 2;		/* shift to ISO 2022 JIS */
      }
      break;
    case 2:			/* ISO 2022 JIS */
      if (c > 0x7f) {	/* <hi> <lo> */
	*t++ = (unsigned char) (c >> 8) & 0x7f;
	*t++ = (unsigned char) c & 0x7f;
      }
      else {			/* ASCII character */
	*t++ = I2C_ESC;		/* ESC ( J <ch> */
	*t++ = I2C_G0_94;
	*t++ = I2CS_94_JIS_ROMAN;
	*t++ = (unsigned char) c;
	iso2022 = 1;		/* shift to ISO 2022 Roman */
      }
      break;
    }
  }
  if (iso2022 == 2) {		/* ISO-2022-JP string must end in Roman */
    *t++ = I2C_ESC;		/* ESC ( J */
    *t++ = I2C_G0_94;
    *t++ = I2CS_94_JIS_ROMAN;
  }
  *t++ = NIL;			/* tie off returned data */
  return LONGT;			/* return success */
}

/* Return UCS-4 character from UTF-8 string
 * Accepts: pointer to string
 *	    remaining octets in string
 * Returns: UCS-4 character or negative if error, pointer and count updated
 */

unsigned long utf8_get (unsigned char **s,unsigned long *i)
{
  unsigned char c;
  unsigned long ret = 0;
  int more = 0;
  while (*i) {			/* until run out of input */
    (*i)--;			/* count this character */
				/* UTF-8 continuation? */
    if (((c = *(*s)++) > 0x7f) && (c < 0xc0)) {
      if (more) {
	ret <<= 6;		/* shift current value by 6 bits */
	ret |= c & 0x3f;
	if (!--more) return ret;/* last octet */
      }
      else return U8G_BADCONT;	/* continuation when not in progress */
    }
				/* incomplete UTF-8 character */
    else if (more) return U8G_INCMPLT;
				/* U+0000 - U+007f */
    else if (c < 0x80) return (unsigned long) c;
    else if (c < 0xe0) {	/* U+0080 - U+07ff */
      ret = c & 0x1f;		/* first 5 bits of 12 */
      more = 1;
    }
    else if (c < 0xf0) {	/* U+1000 - U+ffff */
      ret = c & 0x0f;		/* first 4 bits of 16 */
      more = 2;
    }
				/* non-BMP Unicode */
    else if (c < 0xf8) {	/* U+10000 - U+10ffff (and 110000 - 1fffff) */
      ret = c & 0x07;		/* first 3 bits of 20.5 (21) */
      more = 3;
    }
    else if (c < 0xfc) {	/* ISO 10646 200000 - 3fffffff */
      ret = c & 0x03;		/* first 2 bits of 26 */
      more = 4;
    }
    else if (c < 0xfe) {	/* ISO 10646 4000000-7fffffff*/
      ret = c & 0x01;		/* first bit of 31 */
      more = 5;
    }
    else return U8G_NOTUTF8;	/* not in ISO 10646 */
  }
				/* end of string */
  return more ? U8G_INCMPLT : U8G_ENDSTRG;
}

/* Convert charset labelled sized text to another charset
 * Accepts: source sized text
 *	    source charset
 *	    pointer to returned sized text
 *	    destination charset
 *	    substitute character if not in dest cs, else NIL to return failure
 * Returns: T if successful, NIL if failure
 *
 * This routine has the same restricts as utf8_cstext().
 */

long utf8_cstocstext (SIZEDTEXT *src,char *sc,SIZEDTEXT *dst,char *dc,
		      unsigned short errch)
{
  SIZEDTEXT utf8;
  const CHARSET *scs,*dcs;
  unsigned long i;
  long ret = NIL;
				/* lookup charsets */
  if (dc && (dcs = utf8_charset (dc))) {
    if (sc && *sc) scs = utf8_charset (sc);
    else {			/* try to infer source cs */
      scs = &text_7bit;		/* start by assuming 7-bit */
				/* try harder if converting */
      if (ret) for (i = 0; i < src->size; i++) {
				/* looks like East Asian ISO 2022? */
	if ((src->data[i] == I2C_ESC) && (++i < src->size) &&
	    (src->data[i] == I2C_MULTI) && (++i < src->size)) {
	  scs = &iso2022;	/* yes, set ISO-2022 */
	  break;		/* punt scan */
	}
				/* remember if we see 8-bit anywhere */
	else if (src->data[i] & BIT8) scs = &text_8bit;
      }
      sc = scs->name;		/* get charset name */
    }
				/* init temporary buffer */
    memset (&utf8,NIL,sizeof (SIZEDTEXT));
				/* source cs equivalent to dest cs? */
    if ((scs->type == dcs->type) && (scs->tab == dcs->tab)) {
      dst->data = src->data;	/* yes, just copy pointers */
      dst->size = src->size;
      ret = LONGT;
    }
				/* otherwise do the convertsion */
    else if (utf8_rmap (dc) && utf8_text (src,sc,&utf8,NIL) &&
	     utf8_cstext (&utf8,dc,dst,errch)) ret = LONGT;
				/* flush temporary buffer */
    if (utf8.data && (utf8.data != src->data) && (utf8.data != dst->data))
      fs_give ((void **) &utf8.data);
  }
  return ret;
}

/* Convert ISO 8859-1 to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 *	    conversion table
 */

void utf8_text_1byte0 (SIZEDTEXT *text,SIZEDTEXT *ret,void *tab)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c;
  for (ret->size = i = 0; i < text->size;
       ret->size += (text->data[i++] & BIT8) ? 2 : 1);
  (s = ret->data = (unsigned char *) fs_get (ret->size + 1))[ret->size] =NIL;
  for (i = 0; i < text->size;) {
    if ((c = text->data[i++]) & BIT8) {
      *s++ = 0xc0 | ((c >> 6) & 0x3f);
      *s++ = BIT8 | (c & 0x3f);
    }
    else *s++ = c;		/* ASCII character */
  }
}


/* Convert single byte ASCII+8bit character set sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 *	    conversion table
 */

void utf8_text_1byte (SIZEDTEXT *text,SIZEDTEXT *ret,void *tab)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c;
  unsigned short *tbl = (unsigned short *) tab;
  for (ret->size = i = 0; i < text->size; ret->size += UTF8_SIZE (c))
    if ((c = text->data[i++]) & BIT8) c = tbl[c & BITS7];
  (s = ret->data = (unsigned char *) fs_get (ret->size + 1))[ret->size] =NIL;
  for (i = 0; i < text->size;) {
    if ((c = text->data[i++]) & BIT8) c = tbl[c & BITS7];
    UTF8_PUT (s,c)		/* convert Unicode to UTF-8 */
  }
}

/* Convert single byte 8bit character set sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 *	    conversion table
 */

void utf8_text_1byte8 (SIZEDTEXT *text,SIZEDTEXT *ret,void *tab)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c;
  unsigned short *tbl = (unsigned short *) tab;
  for (ret->size = i = 0; i < text->size; ret->size += UTF8_SIZE (c))
    c = tbl[text->data[i++]];
  (s = ret->data = (unsigned char *) fs_get (ret->size + 1))[ret->size] =NIL;
  for (i = 0; i < text->size;) {
    c = tbl[text->data[i++]];
    UTF8_PUT (s,c)		/* convert Unicode to UTF-8 */
  }
}

/* Convert EUC sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 *	    EUC parameter table
 */

void utf8_text_euc (SIZEDTEXT *text,SIZEDTEXT *ret,void *tab)
{
  unsigned long i;
  unsigned char *s;
  unsigned int pass,c,c1,ku,ten;
  struct utf8_eucparam *p1 = (struct utf8_eucparam *) tab;
  struct utf8_eucparam *p2 = p1 + 1;
  struct utf8_eucparam *p3 = p1 + 2;
  unsigned short *t1 = (unsigned short *) p1->tab;
  unsigned short *t2 = (unsigned short *) p2->tab;
  unsigned short *t3 = (unsigned short *) p3->tab;
  for (pass = 0,s = NIL,ret->size = 0; pass <= 1; pass++) {
    for (i = 0; i < text->size;) {
				/* not CS0? */
      if ((c = text->data[i++]) & BIT8) {
				/* yes, must have another high byte */
	if ((i >= text->size) || !((c1 = text->data[i++]) & BIT8))
	  c = UBOGON;		/* out of space or bogon */
	else switch (c) {	/* check 8bit code set */
	case EUC_CS2:		/* CS2 */
	  if (p2->base_ku) {	/* CS2 set up? */
	    if (p2->base_ten)	/* yes, multibyte? */
	      c = ((i < text->size) && ((c = text->data[i++]) & BIT8) &&
		   ((ku = (c1 & BITS7) - p2->base_ku) < p2->max_ku) &&
		   ((ten = (c & BITS7) - p2->base_ten) < p2->max_ten)) ?
		     t2[(ku*p2->max_ten) + ten] : UBOGON;
	    else c = ((c1 >= p2->base_ku) && (c1 <= p2->max_ku)) ?
	      c1 + ((unsigned int) p2->tab) : UBOGON;
	  }	  
	  else {		/* CS2 not set up */
	    c = UBOGON;		/* swallow byte, say bogon */
	    if (i < text->size) i++;
	  }
	  break;
	case EUC_CS3:		/* CS3 */
	  if (p3->base_ku) {	/* CS3 set up? */
	    if (p3->base_ten)	/* yes, multibyte? */
	      c = ((i < text->size) && ((c = text->data[i++]) & BIT8) &&
		   ((ku = (c1 & BITS7) - p3->base_ku) < p3->max_ku) &&
		   ((ten = (c & BITS7) - p3->base_ten) < p3->max_ten)) ?
		     t3[(ku*p3->max_ten) + ten] : UBOGON;
	    else c = ((c1 >= p3->base_ku) && (c1 <= p3->max_ku)) ?
	      c1 + ((unsigned int) p3->tab) : UBOGON;
	  }	  
	  else {		/* CS3 not set up */
	    c = UBOGON;		/* swallow byte, say bogon */
	    if (i < text->size) i++;
	  }
	  break;

	default:
	  if (((ku = (c & BITS7) - p1->base_ku) >= p1->max_ku) ||
	      ((ten = (c1 & BITS7) - p1->base_ten) >= p1->max_ten)) c = UBOGON;
	  else if (((c = t1[(ku*p1->max_ten) + ten]) == UBOGON) &&
		   /* special hack for JIS X 0212: merge rows less than 10 */
		   ku && (ku < 10) && t3 && p3->base_ten)
	    c = t3[((ku - (p3->base_ku - p1->base_ku))*p3->max_ten) + ten];
	}
      }
      if (pass) UTF8_PUT (s,c)
      else ret->size += UTF8_SIZE (c);
    }
    if (!pass) (s = ret->data = (unsigned char *)
		fs_get (ret->size + 1))[ret->size] =NIL;
  }
}


/* Convert ASCII + double-byte sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 *	    conversion table
 */

void utf8_text_dbyte (SIZEDTEXT *text,SIZEDTEXT *ret,void *tab)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c,c1,ku,ten;
  struct utf8_eucparam *p1 = (struct utf8_eucparam *) tab;
  unsigned short *t1 = (unsigned short *) p1->tab;
  for (ret->size = i = 0; i < text->size; ret->size += UTF8_SIZE (c))
    if ((c = text->data[i++]) & BIT8)
      c = ((i < text->size) && (c1 = text->data[i++]) &&
	   ((ku = c - p1->base_ku) < p1->max_ku) &&
	   ((ten = c1 - p1->base_ten) < p1->max_ten)) ?
	     t1[(ku*p1->max_ten) + ten] : UBOGON;
  (s = ret->data = (unsigned char *) fs_get (ret->size + 1))[ret->size] = NIL;
  for (i = 0; i < text->size;) {
    if ((c = text->data[i++]) & BIT8)
      c = ((i < text->size) && (c1 = text->data[i++]) &&
	   ((ku = c - p1->base_ku) < p1->max_ku) &&
	   ((ten = c1 - p1->base_ten) < p1->max_ten)) ?
	     t1[(ku*p1->max_ten) + ten] : UBOGON;
    UTF8_PUT (s,c)		/* convert Unicode to UTF-8 */
  }
}

/* Convert ASCII + double byte 2 plane sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 *	    conversion table
 */

void utf8_text_dbyte2 (SIZEDTEXT *text,SIZEDTEXT *ret,void *tab)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c,c1,ku,ten;
  struct utf8_eucparam *p1 = (struct utf8_eucparam *) tab;
  struct utf8_eucparam *p2 = p1 + 1;
  unsigned short *t = (unsigned short *) p1->tab;
  for (ret->size = i = 0; i < text->size; ret->size += UTF8_SIZE (c))
    if ((c = text->data[i++]) & BIT8) {
      if ((i >= text->size) || !(c1 = text->data[i++]))
	c = UBOGON;		/* out of space or bogon */
      else if (c1 & BIT8)	/* high vs. low plane */
	c = ((ku = c - p2->base_ku) < p2->max_ku &&
	     ((ten = c1 - p2->base_ten) < p2->max_ten)) ?
	       t[(ku*(p1->max_ten + p2->max_ten)) + p1->max_ten + ten] :UBOGON;
      else c = ((ku = c - p1->base_ku) < p1->max_ku &&
		((ten = c1 - p1->base_ten) < p1->max_ten)) ?
		  t[(ku*(p1->max_ten + p2->max_ten)) + ten] : UBOGON;
    }
  (s = ret->data = (unsigned char *) fs_get (ret->size + 1))[ret->size] = NIL;
  for (i = 0; i < text->size;) {
    if ((c = text->data[i++]) & BIT8) {
      if ((i >= text->size) || !(c1 = text->data[i++]))
	c = UBOGON;		/* out of space or bogon */
      else if (c1 & BIT8)	/* high vs. low plane */
	c = ((ku = c - p2->base_ku) < p2->max_ku &&
	     ((ten = c1 - p2->base_ten) < p2->max_ten)) ?
	       t[(ku*(p1->max_ten + p2->max_ten)) + p1->max_ten + ten] :UBOGON;
      else c = ((ku = c - p1->base_ku) < p1->max_ku &&
		((ten = c1 - p1->base_ten) < p1->max_ten)) ?
		  t[(ku*(p1->max_ten + p2->max_ten)) + ten] : UBOGON;
    }
    UTF8_PUT (s,c)	/* convert Unicode to UTF-8 */
  }
}

#ifdef JISTOUNICODE		/* Japanese */
/* Convert Shift JIS sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to return sized text
 */

void utf8_text_sjis (SIZEDTEXT *text,SIZEDTEXT *ret)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c,c1,ku,ten;
  for (ret->size = i = 0; i < text->size; ret->size += UTF8_SIZE (c)) {
    if ((c = text->data[i++]) & BIT8) {
				/* half-width katakana */
      if ((c >= MIN_KANA_8) && (c <= MAX_KANA_8)) c += KANA_8;
      else if (i >= text->size) c = UBOGON;
      else {			/* Shift-JIS */
	c1 = text->data[i++];
	SJISTOJIS (c,c1);
	c = JISTOUNICODE (c,c1,ku,ten);
      }
    }
				/* compromise - do yen sign but not overline */
    else if (c == JISROMAN_YEN) c = UCS2_YEN;
  }
  (s = ret->data = (unsigned char *) fs_get (ret->size + 1))[ret->size] = NIL;
  for (i = 0; i < text->size;) {
    if ((c = text->data[i++]) & BIT8) {
				/* half-width katakana */
      if ((c >= MIN_KANA_8) && (c <= MAX_KANA_8)) c += KANA_8;
      else {			/* Shift-JIS */
	c1 = text->data[i++];
	SJISTOJIS (c,c1);
	c = JISTOUNICODE (c,c1,ku,ten);
      }
    }
				/* compromise - do yen sign but not overline */
    else if (c == JISROMAN_YEN) c = UCS2_YEN;
    UTF8_PUT (s,c)		/* convert Unicode to UTF-8 */
  }
}
#endif

/* Convert ISO-2022 sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to returned sized text
 */

void utf8_text_2022 (SIZEDTEXT *text,SIZEDTEXT *ret)
{
  unsigned long i;
  unsigned char *s;
  unsigned int pass,state,c,co,gi,gl,gr,g[4],ku,ten;
  for (pass = 0,s = NIL,ret->size = 0; pass <= 1; pass++) {
    gi = 0;			/* quell compiler warnings */
    state = I2S_CHAR;		/* initialize engine */
    g[0]= g[2] = I2CS_ASCII;	/* G0 and G2 are ASCII */
    g[1]= g[3] = I2CS_ISO8859_1;/* G1 and G3 are ISO-8850-1 */
    gl = I2C_G0; gr = I2C_G1;	/* left is G0, right is G1 */
    for (i = 0; i < text->size;) {
      c = text->data[i++];
      switch (state) {		/* dispatch based upon engine state */
      case I2S_ESC:		/* ESC seen */
	switch (c) {		/* process intermediate character */
	case I2C_MULTI:		/* multibyte character? */
	  state = I2S_MUL;	/* mark multibyte flag seen */
	  break;
        case I2C_SS2:		/* single shift GL to G2 */
	case I2C_SS2_ALT:	/* Taiwan SeedNet */
	  gl |= I2C_SG2;
	  break;
        case I2C_SS3:		/* single shift GL to G3 */
	case I2C_SS3_ALT:	/* Taiwan SeedNet */
	  gl |= I2C_SG3;
	  break;
        case I2C_LS2:		/* shift GL to G2 */
	  gl = I2C_G2;
	  break;
        case I2C_LS3:		/* shift GL to G3 */
	  gl = I2C_G3;
	  break;
        case I2C_LS1R:		/* shift GR to G1 */
	  gr = I2C_G1;
	  break;
        case I2C_LS2R:		/* shift GR to G2 */
	  gr = I2C_G2;
	  break;
        case I2C_LS3R:		/* shift GR to G3 */
	  gr = I2C_G3;
	  break;
	case I2C_G0_94: case I2C_G1_94: case I2C_G2_94:	case I2C_G3_94:
	  g[gi = c - I2C_G0_94] = (state == I2S_MUL) ? I2CS_94x94 : I2CS_94;
	  state = I2S_INT;	/* ready for character set */
	  break;
	case I2C_G0_96:	case I2C_G1_96: case I2C_G2_96:	case I2C_G3_96:
	  g[gi = c - I2C_G0_96] = (state == I2S_MUL) ? I2CS_96x96 : I2CS_96;
	  state = I2S_INT;	/* ready for character set */
	  break;
	default:		/* bogon */
	  if (pass) *s++ = I2C_ESC,*s++ = c;
	  else ret->size += 2;
	  state = I2S_CHAR;	/* return to previous state */
	}
	break;

      case I2S_MUL:		/* ESC $ */
	switch (c) {		/* process multibyte intermediate character */
	case I2C_G0_94: case I2C_G1_94: case I2C_G2_94:	case I2C_G3_94:
	  g[gi = c - I2C_G0_94] = I2CS_94x94;
	  state = I2S_INT;	/* ready for character set */
	  break;
	case I2C_G0_96:	case I2C_G1_96: case I2C_G2_96:	case I2C_G3_96:
	  g[gi = c - I2C_G0_96] = I2CS_96x96;
	  state = I2S_INT;	/* ready for character set */
	  break;
	default:		/* probably omitted I2CS_94x94 */
	  g[gi = I2C_G0] = I2CS_94x94 | c;
	  state = I2S_CHAR;	/* return to character state */
	}
	break;
      case I2S_INT:
	state = I2S_CHAR;	/* return to character state */
	g[gi] |= c;		/* set character set */
	break;

      case I2S_CHAR:		/* character data */
	switch (c) {
	case I2C_ESC:		/* ESC character */
	  state = I2S_ESC;	/* see if ISO-2022 prefix */
	  break;
	case I2C_SI:		/* shift GL to G0 */
	  gl = I2C_G0;
	  break;
	case I2C_SO:		/* shift GL to G1 */
	  gl = I2C_G1;
	  break;
        case I2C_SS2_ALT:	/* single shift GL to G2 */
	case I2C_SS2_ALT_7:
	  gl |= I2C_SG2;
	  break;
        case I2C_SS3_ALT:	/* single shift GL to G3 */
	case I2C_SS3_ALT_7:
	  gl |= I2C_SG3;
	  break;

	default:		/* ordinary character */
	  co = c;		/* note original character */
	  if (gl & (3 << 2)) {	/* single shifted? */
	    gi = g[gl >> 2];	/* get shifted character set */
	    gl &= 0x3;		/* cancel shift */
	  }
				/* select left or right half */
	  else gi = (c & BIT8) ? g[gr] : g[gl];
	  c &= BITS7;		/* make 7-bit */
	  switch (gi) {		/* interpret in character set */
	  case I2CS_ASCII:	/* ASCII */
	    break;		/* easy! */
	  case I2CS_BRITISH:	/* British ASCII */
				/* Pound sterling sign */
	    if (c == BRITISH_POUNDSTERLING) c = UCS2_POUNDSTERLING;
	    break;
	  case I2CS_JIS_ROMAN:	/* JIS Roman */
	  case I2CS_JIS_BUGROM:	/* old bugs */
	    switch (c) {	/* two exceptions to ASCII */
	    case JISROMAN_YEN:	/* Yen sign */
	      c = UCS2_YEN;
	      break;
				/* overline */
	    case JISROMAN_OVERLINE:
	      c = UCS2_OVERLINE;
	      break;
	    }
	    break;
	  case I2CS_JIS_KANA:	/* JIS hankaku katakana */
	    if ((c >= MIN_KANA_7) && (c <= MAX_KANA_7)) c += KANA_7;
	    break;

	  case I2CS_ISO8859_1:	/* Latin-1 (West European) */
	    c |= BIT8;		/* just turn on high bit */
	    break;
	  case I2CS_ISO8859_2:	/* Latin-2 (Czech, Slovak) */
	    c = iso8859_2tab[c];
	    break;
	  case I2CS_ISO8859_3:	/* Latin-3 (Dutch, Turkish) */
	    c = iso8859_3tab[c];
	    break;
	  case I2CS_ISO8859_4:	/* Latin-4 (Scandinavian) */
	    c = iso8859_4tab[c];
	    break;
	  case I2CS_ISO8859_5:	/* Cyrillic */
	    c = iso8859_5tab[c];
	    break;
	  case I2CS_ISO8859_6:	/* Arabic */
	    c = iso8859_6tab[c];
	    break;
	  case I2CS_ISO8859_7:	/* Greek */
	    c = iso8859_7tab[c];
	    break;
	  case I2CS_ISO8859_8:	/* Hebrew */
	    c = iso8859_8tab[c];
	    break;
	  case I2CS_ISO8859_9:	/* Latin-5 (Finnish, Portuguese) */
	    c = iso8859_9tab[c];
	    break;
	  case I2CS_TIS620:	/* Thai */
	    c = tis620tab[c];
	    break;
	  case I2CS_ISO8859_10:	/* Latin-6 (Northern Europe) */
	    c = iso8859_10tab[c];
	    break;
	  case I2CS_ISO8859_13:	/* Latin-7 (Baltic) */
	    c = iso8859_13tab[c];
	    break;
	  case I2CS_VSCII:	/* Vietnamese */
	    c = visciitab[c];
	    break;
	  case I2CS_ISO8859_14:	/* Latin-8 (Celtic) */
	    c = iso8859_14tab[c];
	    break;
	  case I2CS_ISO8859_15:	/* Latin-9 (Euro) */
	    c = iso8859_15tab[c];
	    break;
	  case I2CS_ISO8859_16:	/* Latin-10 (Baltic) */
	    c = iso8859_16tab[c];
	    break;

	  default:		/* all other character sets */
				/* multibyte character set */
	    if ((gi & I2CS_MUL) && !(c & BIT8) && isgraph (c)) {
	      c = (i < text->size) ? text->data[i++] : 0;
	      switch (gi) {
#ifdef GBTOUNICODE
	      case I2CS_GB:	/* GB 2312 */
		c = GBTOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef JISTOUNICODE
	      case I2CS_JIS_OLD:/* JIS X 0208-1978 */
	      case I2CS_JIS_NEW:/* JIS X 0208-1983 */
		c = JISTOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef JIS0212TOUNICODE
	      case I2CS_JIS_EXT:/* JIS X 0212-1990 */
		c = JIS0212TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef KSCTOUNICODE
	      case I2CS_KSC:	/* KSC 5601 */
		co |= BIT8;	/* make into EUC */
		c |= BIT8;
		c = KSCTOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS1TOUNICODE
	      case I2CS_CNS1:	/* CNS 11643 plane 1 */
		c = CNS1TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS2TOUNICODE
	      case I2CS_CNS2:	/* CNS 11643 plane 2 */
		c = CNS2TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS3TOUNICODE
	      case I2CS_CNS3:	/* CNS 11643 plane 3 */
		c = CNS3TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS4TOUNICODE
	      case I2CS_CNS4:	/* CNS 11643 plane 4 */
		c = CNS4TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS5TOUNICODE
	      case I2CS_CNS5:	/* CNS 11643 plane 5 */
		c = CNS5TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS6TOUNICODE
	      case I2CS_CNS6:	/* CNS 11643 plane 6 */
		c = CNS6TOUNICODE (co,c,ku,ten);
		break;
#endif
#ifdef CNS7TOUNICODE
	      case I2CS_CNS7:	/* CNS 11643 plane 7 */
		c = CNS7TOUNICODE (co,c,ku,ten);
		break;
#endif
	      default:		/* unknown multibyte, treat as UCS-2 */
		c |= (co << 8);	/* wrong, but nothing else to do */
		break;
	      }
	    }
	    else c = co;	/* unknown single byte, treat as 8859-1 */
	  }
	  if (pass) UTF8_PUT (s,c)
	  else ret->size += UTF8_SIZE (c);
	}
      }
    }
    if (!pass) (s = ret->data = (unsigned char *)
		fs_get (ret->size + 1))[ret->size] = NIL;
    else if (((unsigned long) (s - ret->data)) != ret->size)
      fatal ("ISO-2022 to UTF-8 botch");
  }
}

/* Convert UTF-7 sized text to UTF-8
 * Accepts: source sized text
 *	    pointer to returned sized text
 */

void utf8_text_utf7 (SIZEDTEXT *text,SIZEDTEXT *ret)
{
  unsigned long i;
  unsigned char *s;
  unsigned int c,c1,d,uc,pass,e,e1,state;
  for (pass = 0,s = NIL,ret->size = 0; pass <= 1; pass++) {
    c1 = d = uc = e = e1 = 0;
    for (i = 0,state = NIL; i < text->size;) {
      c = text->data[i++];	/* get next byte */
      switch (state) {
      case U7_PLUS:		/* previous character was + */
	if (c == '-') {		/* +- means textual + */
	  c = '+';
	  state = U7_ASCII;	/* revert to ASCII */
	  break;
	}
	state = U7_UNICODE;	/* enter Unicode state */
	e = e1 = 0;		/* initialize Unicode quantum position */
      case U7_UNICODE:		/* Unicode state */
	if (c == '-') state = U7_MINUS;
	else {			/* decode Unicode */
	  if (isupper (c)) c -= 'A';
	  else if (islower (c)) c -= 'a' - 26;
	  else if (isdigit (c)) c -= '0' - 52;
	  else if (c == '+') c = 62;
	  else if (c == '/') c = 63;
	  else state = U7_ASCII;/* end of modified BASE64 */
	}
	break;
      case U7_MINUS:		/* previous character was absorbed - */
	state = U7_ASCII;	/* revert to ASCII */
      case U7_ASCII:		/* ASCII state */
	if (c == '+') state = U7_PLUS;
	break;
      }

      switch (state) {		/* store character if in character mode */
      case U7_UNICODE:		/* Unicode */
	switch (e++) {		/* install based on BASE64 state */
	case 0:
	  c1 = c << 2;		/* byte 1: high 6 bits */
	  break;
	case 1:
	  d = c1 | (c >> 4);	/* byte 1: low 2 bits */
	  c1 = c << 4;		/* byte 2: high 4 bits */
	  break;
	case 2:
	  d = c1 | (c >> 2);	/* byte 2: low 4 bits */
	  c1 = c << 6;		/* byte 3: high 2 bits */
	  break;
	case 3:
	  d = c | c1;		/* byte 3: low 6 bits */
	  e = 0;		/* reinitialize mechanism */
	  break;
	}
	if (e == 1) break;	/* done if first BASE64 state */
	if (!e1) {		/* first byte of UCS-2 character */
	  uc = (d & 0xff) << 8;	/* note first byte */
	  e1 = T;		/* enter second UCS-2 state */
	  break;		/* done */
	}
	c = uc | (d & 0xff);	/* build UCS-2 character */
	e1 = NIL;		/* back to first UCS-2 state, drop in */
      case U7_ASCII:		/* just install if ASCII */
	if (pass) UTF8_PUT (s,c)
	else ret->size += UTF8_SIZE (c);
      }
    }
    if (!pass) (s = ret->data = (unsigned char *)
		fs_get (ret->size + 1))[ret->size] = NIL;
    else if (((unsigned long) (s - ret->data)) != ret->size)
      fatal ("UTF-7 to UTF-8 botch");
  }
}

/* Convert charset labelled searchpgm to UTF-8 in place
 * Accepts: search program
 *	    charset
 */

void utf8_searchpgm (SEARCHPGM *pgm,char *charset)
{
  SIZEDTEXT txt;
  SEARCHHEADER *hl;
  SEARCHOR *ol;
  SEARCHPGMLIST *pl;
  if (pgm) {			/* must have a search program */
    utf8_stringlist (pgm->bcc,charset);
    utf8_stringlist (pgm->cc,charset);
    utf8_stringlist (pgm->from,charset);
    utf8_stringlist (pgm->to,charset);
    utf8_stringlist (pgm->subject,charset);
    for (hl = pgm->header; hl; hl = hl->next) {
      if (utf8_text (&hl->line,charset,&txt,NIL)) {
	fs_give ((void **) &hl->line.data);
	hl->line.data = txt.data;
	hl->line.size = txt.size;
      }
      if (utf8_text (&hl->text,charset,&txt,NIL)) {
	fs_give ((void **) &hl->text.data);
	hl->text.data = txt.data;
	hl->text.size = txt.size;
      }
    }
    utf8_stringlist (pgm->body,charset);
    utf8_stringlist (pgm->text,charset);
    for (ol = pgm->or; ol; ol = ol->next) {
      utf8_searchpgm (ol->first,charset);
      utf8_searchpgm (ol->second,charset);
    }
    for (pl = pgm->not; pl; pl = pl->next) utf8_searchpgm (pl->pgm,charset);
  }
}


/* Convert charset labelled stringlist to UTF-8 in place
 * Accepts: string list
 *	    charset
 */

void utf8_stringlist (STRINGLIST *st,char *charset)
{
  SIZEDTEXT txt;
				/* convert entire stringstruct */
  if (st) do if (utf8_text (&st->text,charset,&txt,NIL)) {
    fs_give ((void **) &st->text.data);
    st->text.data = txt.data; /* transfer this text */
    st->text.size = txt.size;
  } while (st = st->next);
}

/* Convert MIME-2 sized text to UTF-8
 * Accepts: source sized text
 *	    charset
 * Returns: T if successful, NIL if failure
 */

#define MINENCWORD 9

long utf8_mime2text (SIZEDTEXT *src,SIZEDTEXT *dst)
{
  unsigned char *s,*se,*e,*ee,*t,*te;
  char *cs,*ce,*ls;
  SIZEDTEXT txt,rtxt;
  unsigned long i;
  dst->data = NIL;		/* default is no encoded words */
				/* look for encoded words */
  for (s = src->data, se = src->data + src->size; s < se; s++) {
    if (((se - s) > MINENCWORD) && (*s == '=') && (s[1] == '?') &&
      (cs = (char *) mime2_token (s+2,se,(unsigned char **) &ce)) &&
	(e = mime2_token ((unsigned char *) ce+1,se,&ee)) &&
	(t = mime2_text (e+2,se,&te)) && (ee == e + 1)) {
      if (mime2_decode (e,t,te,&txt)) {
	*ce = '\0';		/* temporarily tie off charset */
	if (ls = strchr (cs,'*')) *ls = '\0';
				/* convert to UTF-8 as best we can */
	if (!utf8_text (&txt,cs,&rtxt,NIL)) utf8_text (&txt,NIL,&rtxt,NIL);
	if (!dst->data) {	/* need to create buffer now? */
				/* allocate for worst case */
	  dst->data = (unsigned char *)
	    fs_get ((size_t) ((src->size / 4) + 1) * 9);
	  memcpy (dst->data,src->data,(size_t) (dst->size = s - src->data));
	}
	for (i=0; i < rtxt.size; i++) dst->data[dst->size++] = rtxt.data[i];
				/* all done with converted text */
	if (rtxt.data != txt.data) fs_give ((void **) &rtxt.data);
	if (ls) *ls = '*';	/* restore language tag delimiter */
	*ce = '?';		/* restore charset delimiter */
				/* all done with decoded text */
	fs_give ((void **) &txt.data);
	s = te+1;		/* continue scan after encoded word */

				/* skip leading whitespace */
	for (t = s + 1; (t < se) && ((*t == ' ') || (*t == '\t')); t++);
				/* see if likely continuation encoded word */
	if (t < (se - MINENCWORD)) switch (*t) {
	case '=':		/* possible encoded word? */
	  if (t[1] == '?') s = t - 1;
	  break;
	case '\015':		/* CR, eat a following LF */
	  if (t[1] == '\012') t++;
	case '\012':		/* possible end of logical line */
	  if ((t[1] == ' ') || (t[1] == '\t')) {
	    do t++;
	    while ((t < (se - MINENCWORD)) && ((t[1] == ' ')||(t[1] == '\t')));
	    if ((t < (se - MINENCWORD)) && (t[1] == '=') && (t[2] == '?'))
	      s = t;		/* definitely looks like continuation */
	  }
	}
      }
      else {			/* restore original text */
	if (dst->data) fs_give ((void **) &dst->data);
	dst->data = src->data;
	dst->size = src->size;
	return NIL;		/* syntax error: MIME-2 decoding failure */
      }
    }
				/* stash ordinary character */
    else if (dst->data) dst->data[dst->size++] = *s;
  }
  if (dst->data) dst->data[dst->size] = '\0';
  else {			/* nothing converted, return identity */
    dst->data = src->data;
    dst->size = src->size;
  }
  return T;			/* success */
}

/* Decode MIME-2 text
 * Accepts: Encoding
 *	    text
 *	    text end
 *	    destination sized text
 * Returns: T if successful, else NIL
 */

long mime2_decode (unsigned char *e,unsigned char *t,unsigned char *te,
		   SIZEDTEXT *txt)
{
  unsigned char *q;
  txt->data = NIL;		/* initially no returned data */
  switch (*e) {			/* dispatch based upon encoding */
  case 'Q': case 'q':		/* sort-of QUOTED-PRINTABLE */
    txt->data = (unsigned char *) fs_get ((size_t) (te - t) + 1);
    for (q = t,txt->size = 0; q < te; q++) switch (*q) {
    case '=':			/* quoted character */
				/* both must be hex */
      if (!isxdigit (q[1]) || !isxdigit (q[2])) {
	fs_give ((void **) &txt->data);
	return NIL;		/* syntax error: bad quoted character */
      }
      txt->data[txt->size++] =	/* assemble character */
	((q[1] - (isdigit (q[1]) ? '0' :
		  ((isupper (q[1]) ? 'A' : 'a') - 10))) << 4) +
		    (q[2] - (isdigit (q[2]) ? '0' :
			     ((isupper (q[2]) ? 'A' : 'a') - 10)));
      q += 2;			/* advance past quoted character */
      break;
    case '_':			/* convert to space */
      txt->data[txt->size++] = ' ';
      break;
    default:			/* ordinary character */
      txt->data[txt->size++] = *q;
      break;
    }
    txt->data[txt->size] = '\0';
    break;
  case 'B': case 'b':		/* BASE64 */
    if (txt->data = (unsigned char *) rfc822_base64 (t,te - t,&txt->size))
      break;
  default:			/* any other encoding is unknown */
    return NIL;			/* syntax error: unknown encoding */
  }
  return T;
}

/* Get MIME-2 token from encoded word
 * Accepts: current text pointer
 *	    text limit pointer
 *	    pointer to returned end pointer
 * Returns: current text pointer & end pointer if success, else NIL
 */

unsigned char *mime2_token (unsigned char *s,unsigned char *se,
			    unsigned char **t)
{
  for (*t = s; **t != '?'; ++*t) {
    if ((*t < se) && isgraph (**t)) switch (**t) {
    case '(': case ')': case '<': case '>': case '@': case ',': case ';':
    case ':': case '\\': case '"': case '/': case '[': case ']': case '.':
    case '=':
      return NIL;		/* none of these are valid in tokens */
    }
    else return NIL;		/* out of text or CTL or space */
  }
  return s;
}


/* Get MIME-2 text from encoded word
 * Accepts: current text pointer
 *	    text limit pointer
 *	    pointer to returned end pointer
 * Returns: current text pointer & end pointer if success, else NIL
 */

unsigned char *mime2_text (unsigned char *s,unsigned char *se,
			   unsigned char **t)
{
				/* make sure valid, search for closing ? */
  for (*t = s; **t != '?'; ++*t) if ((*t >= se) || !isgraph (**t)) return NIL;
				/* make sure terminated properly */
  if ((*t)[1] != '=') return NIL;
  return s;
}
