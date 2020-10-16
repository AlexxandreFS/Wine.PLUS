/******************************************************************************
 * winspool internal include file
 *
 *
 * Copyright 2005  Huw Davies
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <windef.h>
#include <winuser.h>

extern HINSTANCE WINSPOOL_hInstance DECLSPEC_HIDDEN;

extern PRINTPROVIDOR * backend DECLSPEC_HIDDEN;
extern BOOL load_backend(void) DECLSPEC_HIDDEN;

extern void WINSPOOL_LoadSystemPrinters(void) DECLSPEC_HIDDEN;

#define IDS_CAPTION       10
#define IDS_FILE_EXISTS   11
#define IDS_CANNOT_OPEN   12

#define IDR_SRGB_ICM       2

#define FILENAME_DIALOG  100
#define EDITBOX 201