#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include "gpsapp.h"

static char *
findchars (char *s, char *chars)
{
        char c, *k;

        while ((c = *s)) {
                for (k = chars; *k; ++k) {
                        if (c == *k)
                                return s;
                }
                ++s;
        }
        return s;
}

static char *
skipchars (char *s, char *chars)
{
        char c, *k;

        skip: while ((c = *s)) {
                for (k = chars; *k; ++k) {
                        if (c == *k) {
                                ++s;
                                goto skip;
                        }
                }
                break;
        }
        return s;
}

static char *
find_header (char *s, const char *header)
{
  if (!s || !*s || !(s = strstr(s, header)) || !*s || !*(s += strlen(header)))
    s = NULL;
  return s;
}

const char *val0[] = { "off", "no", "0", "false", "sats", "ddd", NULL };
const char *val1[] = { "on", "yes", "1", "true", "map", "dmm", NULL };
const char *val2[] = { "permanent", "route", "dms", NULL };

int
config_ini_option (char *s, char *match, int *inside)
{
  if (!(*inside) && !(s = find_header(s, CONFIG_HEADER))) {
    return -1;
  }

  while (*(s = skipchars(s, " \n\t\r")) && *s != '[') {
    char *f;

    if (*s == ';') {
      /* ignore comment */
    } else {
      if ((f = findchars(s, "="))) {
	if (strncmp(s,match,f-s) == 0) {
          int i, len;
	  char *eof = findchars(s, "\n");
	  if (!eof)
	    eof = findchars(s, "\0");
	  if (!eof)
	    return -1;

	  /* Special handling for "protocol" */
	  if (!strcmp(match, "protocol")) {
	    char proto_name[12]; /* Be stingy, but we may need more later? */
	    int sz = ((eof-(f+1))>11)?11:(eof-(f+1)); /* reserve \0 */

	    strncpy(proto_name, (char *)f+1, sz); /* maybe no \0 */
	    proto_name[sz] = '\0'; 
	    serial_protocol(proto_name); /* safe due to nmea default later */
	    return 0;
	  }

	  /* Special handling for "serialport" */
	  if (!strcmp(match, "serialport")) {
	    int sz = (eof-(f+1)); /* reserve \0 */
	    serport=(char *)malloc(sz);
	    strncpy(serport, (char *)f+1, sz); /* maybe no \0 */
	    serport[sz] = '\0'; 
	    return 0;
	  }

	  /* Special handling for "serialport" */
	  if (!strcmp(match, "routedir")) {
	    int sz = (eof-(f+1)); /* reserve \0 */
	    serport=(char *)malloc(sz);
	    strncpy(routedir, (char *)f+1, sz); /* maybe no \0 */
	    routedir[sz] = '\0'; 
	    return 0;
	  }

	  len = eof - (f+1);
	  for (i = 0; val2[i] != NULL; i++)
	      if (!strncasecmp((char *)f+1, val2[i], len))
		  return 2;

	  for (i = 0; val1[i] != NULL; i++)
	      if (!strncasecmp((char *)f+1, val1[i], len))
		  return 1;

	  for (i = 0; val0[i] != NULL; i++)
	      if (!strncasecmp((char *)f+1, val0[i], len))
		  return 0;
	}
      } 
    }
    s = findchars(s, "\n");
  }

  if (*s != '[') *inside = 0;

  return -1;
}
