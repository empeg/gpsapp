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

	  if (!strncasecmp((char *)f+1, "permanent", (eof-(f+1)))) {
	    return 2;
	  }	    
	  if (!strncasecmp((char *)f+1, "sats", (eof-(f+1)))) {
	    return 0;
	  }	    
	  if (!strncasecmp((char *)f+1, "map", (eof-(f+1)))) {
	    return 1;
	  }	    
	  if (!strncasecmp((char *)f+1, "route", (eof-(f+1)))) {
	    return 2;
	  }	    
	  if (!strncasecmp((char *)f+1, "true", (eof-(f+1)))) {
	    return 1;
	  }	    
	  if (!strncasecmp((char *)f+1, "false", (eof-(f+1)))) {
	    return 0;
	  }	    
	  if (!strncmp((char *)f+1, "1", (eof-(f+1)))) {
	    return 1;
	  }	    
	  if (!strncmp((char *)f+1, "0", (eof-(f+1)))) {
	    return 0;
	  }	    
	  if (!strncasecmp((char *)f+1, "yes", (eof-(f+1)))) {
	    return 1;
	  }	    
	  if (!strncasecmp((char *)f+1, "no", (eof-(f+1)))) {
	    return 0;
	  }	    
	}
      } 
    }
    s = findchars(s, "\n");
  }

  if (*s != '[') *inside = 0;

  return -1;
}
