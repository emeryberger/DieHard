#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int pos(char *haystack, char *needle)
{
	if (strstr(haystack, needle)) {
		return (int) strstr(haystack, needle) - (int) haystack;
	} else {
		return -1;
	}
}

void cutto(char *str, int len)
{
	memmove(str, str + len, strlen(str) - len + 1);
}

void mystrncpy(char *dest, char *src, int len)
{
	strncpy(dest, src, len);
	*(dest + len) = 0;
}

int replace(char *str, char *what, char *by)
{
	char *foo, *bar = str;
    int i = 0;
	while ((foo = strstr(bar, what))) {
        bar = foo + strlen(by);
		memmove(bar,
				foo + strlen(what), strlen(foo + strlen(what)) + 1);
		memcpy(foo, by, strlen(by));
        i++;
	}
    return i;
}

/* int_from_list(char *list, int n) 
 * returns the n'th integer element from a string like '2,5,12-15,20-23'
 * if n is out of range or *list is out of range, -1 is returned.
 */

int int_from_list(char *list, int n)
{
    char *str, *tok;
    int count = -1, firstrun = 1;
    
    str = (char *) malloc(sizeof(char) * (strlen(list) + 2));
    if (!str)
        return -1;
    
    memset(str, 0, strlen(list) + 2);
    strncpy(str, list, strlen(list));
    
    /* apppend ',' to the string so we can always use strtok() */
	str[strlen(list)] = ',';
    
    for (;;) {
        if (firstrun)
            tok = strtok(str, ",");
        else
            tok = strtok(NULL, ",");
        
        if (!tok || *tok == '\0') {
			free(str);
            return -1;
		}
        
        if (strchr(tok, '-')) {
            char *start, *end;
            int s, e;
            start = tok;
            end = strchr(tok, '-');
            *end = '\0';
            end++;
            if (!*start || !*end) {
				free(str);
                return -1;
			}
            s = atoi(start);
            e = atoi(end);
            if (s < 1 || e < 1 || e < s) {
				free(str);
                return -1;
			}
            count += e - s + 1;
            if (count >= n) {
				free(str);
                return (e - (count - n));
			}
        } else {
            count++;
            if (count == n) {
				int val = atoi(tok);
				free(str);
                return val;
			}
        }
        firstrun = 0;
    }
}
