#include <stdio.h>
#include <stdlib.h>

#include "list.h"

void bftpd_list_add(struct bftpd_list_element **list, void *data)
{
	struct bftpd_list_element *new = malloc(sizeof(struct bftpd_list_element));
	struct bftpd_list_element *tmp = *list;
	new->data = data;
	new->next = NULL;
	if (tmp) {
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new;
	} else
		*list = new;
}

void bftpd_list_del(struct bftpd_list_element **list, int index)
{
	struct bftpd_list_element *tmp = *list;
	struct bftpd_list_element *tmp2;
	int i;
	if (!index) {
		tmp = tmp->next;
		free(*list);
		*list = tmp;
	} else {
		for (i = 0; i < index - 1; i++) {
			if (!(tmp->next))
				return;
			tmp = tmp->next;
		}
		tmp2 = tmp->next;
		tmp->next = tmp->next->next;
		free(tmp2);
	}
}

int bftpd_list_count(struct bftpd_list_element *list)
{
	int i = 1;
	struct bftpd_list_element *tmp = list;
	if (!tmp)
		return 0;
	while ((tmp = tmp->next))
		i++;
	return i;
}

void *bftpd_list_get(struct bftpd_list_element *list, int index)
{
	struct bftpd_list_element *tmp = list;
	int i;
	for (i = 0; i < index; i++) {
		if (!(tmp->next))
			return NULL;
		tmp = tmp->next;
	}
	return tmp->data;
}
