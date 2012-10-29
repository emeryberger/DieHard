#ifndef __BFTPD_LIST_H
#define __BFTPD_LIST_H

#include <stdio.h>

struct bftpd_list_element {
	void *data;
	struct bftpd_list_element *next;
};

void bftpd_list_add(struct bftpd_list_element **list, void *data);
void bftpd_list_del(struct bftpd_list_element **list, int index);
int bftpd_list_count(struct bftpd_list_element *list);
void *bftpd_list_get(struct bftpd_list_element *list, int index);

#endif
