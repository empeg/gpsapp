/*
 * Copyright (c) 2002 Jan Harkes <jaharkes(at)cs.cmu.edu>
 * This code is distributed "AS IS" without warranty of any kind under the
 * terms of the GNU General Public License Version 2.
 */

#ifndef _EMPEG_UI_H_
#define _EMPEG_UI_H_

#include "hijack.h"

int  empeg_init(void);
void empeg_free(void);
int  empeg_waitmenu(void);
int  empeg_getkey(unsigned long *key);
void empeg_updatedisplay(const unsigned char *screen);

#endif /* _EMPEG_UI_H_ */

