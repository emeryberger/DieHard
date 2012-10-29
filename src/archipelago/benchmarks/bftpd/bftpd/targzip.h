#ifndef __BFTPD_TARGZIP_H
#define __BFTPD_TARGZIP_H

enum const_whattodo {
	DO_NORMAL, DO_TARGZ, DO_TARONLY, DO_GZONLY, DO_GZUNZIP
};

int pax_main(int argc, char *const *argv);

#endif
