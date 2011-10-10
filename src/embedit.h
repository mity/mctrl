/*
 * Copyright (c) 2011 Martin Mitas
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef MC_EMBEDIT_H
#define MC_EMBEDIT_H

#include "misc.h"


/* Embedded edit control: Simple subclass of standard EDIT control, 
 * used in more complex controls (e.g. MC_WC_PROPVIEW or MC_WC_GRID)
 * which need edit capability. */


/* Window class */
extern const TCHAR embedit_wc[];

/* Default style */
#define EMBEDIT_STYLE   (WS_CHILD | WS_CLIPSIBLINGS | ES_LEFT | ES_AUTOHSCROLL)

/* Notifications (sent via WM_COMMAND) when the control text should be
 * applied/canceled. The embedded edit control is destroyed after sending
 * any of these.
 *
 * NOTE: There is no equivalent of WM_USER for notification codes. Hence we
 * use arbitrary codes here chosen so that it is unlikely to clash with any
 * notifications sent be standard EDIT controls.
 */
#define EEN_APPLY      (0xe01e)
#define EEN_CANCEL     (0xe02e)


int embedit_init(void);
void embedit_fini(void);


#endif  /* MC_EMBEDIT_H */
