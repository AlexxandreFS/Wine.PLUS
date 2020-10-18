/*
 * WDFLDR driver stub.
 *
 * Copyright 2020 Paul Gofman <pgofman@codeweavers.com> for Codeweavers
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

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winioctl.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wdfldr);

static void WINAPI driver_unload(DRIVER_OBJECT *driver)
{
    TRACE("driver %p.\n", driver);
}

typedef void *WDF_COMPONENT_GLOBALS;
typedef WDF_COMPONENT_GLOBALS *PWDF_COMPONENT_GLOBALS;

typedef VOID (* WINAPI WDFFUNC)(VOID);

typedef ULONG WDF_MAJOR_VERSION;
typedef ULONG WDF_MINOR_VERSION;
typedef ULONG WDF_BUILD_NUMBER;

typedef struct _WDF_VERSION
{
    WDF_MAJOR_VERSION  Major;
    WDF_MINOR_VERSION  Minor;
    WDF_BUILD_NUMBER   Build;
} WDF_VERSION;

typedef struct _WDF_BIND_INFO
{
    ULONG              Size;
    PWCHAR             Component;
    WDF_VERSION        Version;
    ULONG              FuncCount;
    WDFFUNC *FuncTable;

    PVOID              Module;
} WDF_BIND_INFO, * PWDF_BIND_INFO;

NTSTATUS WINAPI WdfVersionBind(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath,
    PWDF_BIND_INFO BindInfo,
    PWDF_COMPONENT_GLOBALS *ComponentGlobals
    )
{
    static UINT64 wdf_ldr_data[256];
    static PWDF_COMPONENT_GLOBALS pwdf_ldr_data = (void *)wdf_ldr_data;

    FIXME("DriverObject %p, RegistryPath %s, BindInfo %p, ComponentGlobals %p.\n",
            DriverObject, debugstr_w(RegistryPath->Buffer), BindInfo, ComponentGlobals);
    FIXME("BindInfo Size %#lx, Component %s, Version %u.%u.%u, FuncCount %u, f[0] %p.\n", BindInfo->Size, debugstr_w(BindInfo->Component),
            BindInfo->Version.Major, BindInfo->Version.Minor, BindInfo->Version.Build, BindInfo->FuncCount, BindInfo->FuncTable[0]);

    /*{
        unsigned int i;

        for (i = 0; i < BindInfo->FuncCount; ++i)
            BindInfo->FuncTable[i] = (void *)0xfeeddeadbeef;
    }*/

    *ComponentGlobals = pwdf_ldr_data;


    return STATUS_SUCCESS;
}

NTSTATUS WINAPI WdfVersionUnbind(
    PUNICODE_STRING RegistryPath,
    PWDF_BIND_INFO BindInfo,
    PWDF_COMPONENT_GLOBALS ComponentGlobals
    )
{
    FIXME("RegistryPath %s, BindInfo %p, ComponentGlobals %p.\n",
            debugstr_w(RegistryPath->Buffer), BindInfo, ComponentGlobals);

    return STATUS_SUCCESS;
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    TRACE("driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    driver->DriverUnload = driver_unload;

    return STATUS_SUCCESS;
}
