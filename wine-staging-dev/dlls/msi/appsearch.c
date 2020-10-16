/*
 * Implementation of the AppSearch action of the Microsoft Installer (msi.dll)
 *
 * Copyright 2005 Juan Lang
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
#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "msi.h"
#include "msiquery.h"
#include "msidefs.h"
#include "winver.h"
#include "shlwapi.h"
#include "wine/debug.h"
#include "msipriv.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

typedef struct tagMSISIGNATURE
{
    LPCWSTR  Name;     /* NOT owned by this structure */
    LPWSTR   File;
    DWORD    MinVersionMS;
    DWORD    MinVersionLS;
    DWORD    MaxVersionMS;
    DWORD    MaxVersionLS;
    DWORD    MinSize;
    DWORD    MaxSize;
    FILETIME MinTime;
    FILETIME MaxTime;
    LPWSTR   Languages;
}MSISIGNATURE;

void msi_parse_version_string(LPCWSTR verStr, PDWORD ms, PDWORD ls)
{
    const WCHAR *ptr;
    int x1 = 0, x2 = 0, x3 = 0, x4 = 0;

    x1 = wcstol(verStr, NULL, 10);
    ptr = wcschr(verStr, '.');
    if (ptr)
    {
        x2 = wcstol(ptr + 1, NULL, 10);
        ptr = wcschr(ptr + 1, '.');
    }
    if (ptr)
    {
        x3 = wcstol(ptr + 1, NULL, 10);
        ptr = wcschr(ptr + 1, '.');
    }
    if (ptr)
        x4 = wcstol(ptr + 1, NULL, 10);
    /* FIXME: byte-order dependent? */
    *ms = x1 << 16 | x2;
    if (ls) *ls = x3 << 16 | x4;
}

/* Fills in sig with the values from the Signature table, where name is the
 * signature to find.  Upon return, sig->File will be NULL if the record is not
 * found, and not NULL if it is found.
 * Warning: clears all fields in sig!
 * Returns ERROR_SUCCESS upon success (where not finding the record counts as
 * success), something else on error.
 */
static UINT get_signature( MSIPACKAGE *package, MSISIGNATURE *sig, const WCHAR *name )
{
    static const WCHAR query[] = {
        's','e','l','e','c','t',' ','*',' ',
        'f','r','o','m',' ',
        'S','i','g','n','a','t','u','r','e',' ',
        'w','h','e','r','e',' ','S','i','g','n','a','t','u','r','e',' ','=',' ',
        '\'','%','s','\'',0};
    LPWSTR minVersion, maxVersion, p;
    MSIRECORD *row;
    DWORD time;

    TRACE("package %p, sig %p\n", package, sig);

    memset(sig, 0, sizeof(*sig));
    sig->Name = name;
    row = MSI_QueryGetRecord( package->db, query, name );
    if (!row)
    {
        TRACE("failed to query signature for %s\n", debugstr_w(name));
        return ERROR_SUCCESS;
    }

    /* get properties */
    sig->File = msi_dup_record_field(row,2);
    if ((p = wcschr(sig->File, '|')))
    {
        p++;
        memmove(sig->File, p, (lstrlenW(p) + 1) * sizeof(WCHAR));
    }

    minVersion = msi_dup_record_field(row,3);
    if (minVersion)
    {
        msi_parse_version_string( minVersion, &sig->MinVersionMS, &sig->MinVersionLS );
        msi_free( minVersion );
    }
    maxVersion = msi_dup_record_field(row,4);
    if (maxVersion)
    {
        msi_parse_version_string( maxVersion, &sig->MaxVersionMS, &sig->MaxVersionLS );
        msi_free( maxVersion );
    }
    sig->MinSize = MSI_RecordGetInteger(row,5);
    if (sig->MinSize == MSI_NULL_INTEGER)
        sig->MinSize = 0;
    sig->MaxSize = MSI_RecordGetInteger(row,6);
    if (sig->MaxSize == MSI_NULL_INTEGER)
        sig->MaxSize = 0;
    sig->Languages = msi_dup_record_field(row,9);
    time = MSI_RecordGetInteger(row,7);
    if (time != MSI_NULL_INTEGER)
        DosDateTimeToFileTime(HIWORD(time), LOWORD(time), &sig->MinTime);
    time = MSI_RecordGetInteger(row,8);
    if (time != MSI_NULL_INTEGER)
        DosDateTimeToFileTime(HIWORD(time), LOWORD(time), &sig->MaxTime);

    TRACE("Found file name %s for Signature_ %s;\n",
          debugstr_w(sig->File), debugstr_w(name));
    TRACE("MinVersion is %d.%d.%d.%d\n", HIWORD(sig->MinVersionMS),
          LOWORD(sig->MinVersionMS), HIWORD(sig->MinVersionLS),
          LOWORD(sig->MinVersionLS));
    TRACE("MaxVersion is %d.%d.%d.%d\n", HIWORD(sig->MaxVersionMS),
          LOWORD(sig->MaxVersionMS), HIWORD(sig->MaxVersionLS),
          LOWORD(sig->MaxVersionLS));
    TRACE("MinSize is %d, MaxSize is %d;\n", sig->MinSize, sig->MaxSize);
    TRACE("Languages is %s\n", debugstr_w(sig->Languages));

    msiobj_release( &row->hdr );

    return ERROR_SUCCESS;
}

/* Frees any memory allocated in sig */
static void free_signature( MSISIGNATURE *sig )
{
    msi_free(sig->File);
    msi_free(sig->Languages);
}

static WCHAR *search_file( MSIPACKAGE *package, WCHAR *path, MSISIGNATURE *sig )
{
    VS_FIXEDFILEINFO *info;
    DWORD attr, size;
    LPWSTR val = NULL;
    LPBYTE buffer;

    if (!sig->File)
    {
        PathRemoveFileSpecW(path);
        PathAddBackslashW(path);

        attr = msi_get_file_attributes( package, path );
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            return strdupW(path);

        return NULL;
    }

    attr = msi_get_file_attributes( package, path );
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        return NULL;

    size = msi_get_file_version_info( package, path, 0, NULL );
    if (!size)
        return strdupW(path);

    buffer = msi_alloc(size);
    if (!buffer)
        return NULL;

    size = msi_get_file_version_info( package, path, size, buffer );
    if (!size)
        goto done;

    if (!VerQueryValueW(buffer, szBackSlash, (LPVOID)&info, &size) || !info)
        goto done;

    if (sig->MinVersionLS || sig->MinVersionMS)
    {
        if (info->dwFileVersionMS < sig->MinVersionMS)
            goto done;

        if (info->dwFileVersionMS == sig->MinVersionMS &&
            info->dwFileVersionLS < sig->MinVersionLS)
            goto done;
    }

    if (sig->MaxVersionLS || sig->MaxVersionMS)
    {
        if (info->dwFileVersionMS > sig->MaxVersionMS)
            goto done;

        if (info->dwFileVersionMS == sig->MaxVersionMS &&
            info->dwFileVersionLS > sig->MaxVersionLS)
            goto done;
    }

    val = strdupW(path);

done:
    msi_free(buffer);
    return val;
}

static UINT search_components( MSIPACKAGE *package, WCHAR **appValue, MSISIGNATURE *sig )
{
    static const WCHAR query[] =  {
        'S','E','L','E','C','T',' ','*',' ',
        'F','R','O','M',' ',
        '`','C','o','m','p','L','o','c','a','t','o','r','`',' ',
        'W','H','E','R','E',' ','`','S','i','g','n','a','t','u','r','e','_','`',' ','=',' ',
        '\'','%','s','\'',0};
    static const WCHAR sigquery[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        '`','S','i','g','n','a','t','u','r','e','`',' ',
        'W','H','E','R','E',' ','`','S','i','g','n','a','t','u','r','e','`',' ','=',' ',
        '\'','%','s','\'',0};
    MSIRECORD *row, *rec;
    LPCWSTR signature, guid;
    BOOL sigpresent = TRUE;
    BOOL isdir;
    UINT type;
    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    LPWSTR ptr;
    DWORD attr;

    TRACE("%s\n", debugstr_w(sig->Name));

    *appValue = NULL;

    row = MSI_QueryGetRecord(package->db, query, sig->Name);
    if (!row)
    {
        TRACE("failed to query CompLocator for %s\n", debugstr_w(sig->Name));
        return ERROR_SUCCESS;
    }

    signature = MSI_RecordGetString(row, 1);
    guid = MSI_RecordGetString(row, 2);
    type = MSI_RecordGetInteger(row, 3);

    rec = MSI_QueryGetRecord(package->db, sigquery, signature);
    if (!rec)
        sigpresent = FALSE;

    *path = '\0';
    MsiLocateComponentW(guid, path, &size);
    if (!*path)
        goto done;

    attr = msi_get_file_attributes( package, path );
    if (attr == INVALID_FILE_ATTRIBUTES)
        goto done;

    isdir = (attr & FILE_ATTRIBUTE_DIRECTORY);

    if (type != msidbLocatorTypeDirectory && sigpresent && !isdir)
    {
        *appValue = search_file( package, path, sig );
    }
    else if (!sigpresent && (type != msidbLocatorTypeDirectory || isdir))
    {
        if (type == msidbLocatorTypeFileName)
        {
            ptr = wcsrchr(path, '\\');
            *(ptr + 1) = '\0';
        }
        else
            PathAddBackslashW(path);

        *appValue = strdupW(path);
    }
    else if (sigpresent)
    {
        PathAddBackslashW(path);
        lstrcatW(path, MSI_RecordGetString(rec, 2));

        attr = msi_get_file_attributes( package, path );
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            *appValue = strdupW(path);
    }

done:
    if (rec) msiobj_release(&rec->hdr);
    msiobj_release(&row->hdr);
    return ERROR_SUCCESS;
}

static void convert_reg_value( DWORD regType, const BYTE *value, DWORD sz, WCHAR **appValue )
{
    static const WCHAR dwordFmt[] = { '#','%','d','\0' };
    static const WCHAR binPre[] = { '#','x','\0' };
    static const WCHAR binFmt[] = { '%','0','2','X','\0' };
    LPWSTR ptr;
    DWORD i;

    switch (regType)
    {
        case REG_SZ:
            if (*(LPCWSTR)value == '#')
            {
                /* escape leading pound with another */
                *appValue = msi_alloc(sz + sizeof(WCHAR));
                (*appValue)[0] = '#';
                lstrcpyW(*appValue + 1, (LPCWSTR)value);
            }
            else
            {
                *appValue = msi_alloc(sz);
                lstrcpyW(*appValue, (LPCWSTR)value);
            }
            break;
        case REG_DWORD:
            /* 7 chars for digits, 1 for NULL, 1 for #, and 1 for sign
             * char if needed
             */
            *appValue = msi_alloc(10 * sizeof(WCHAR));
            swprintf(*appValue, 10, dwordFmt, *(const DWORD *)value);
            break;
        case REG_EXPAND_SZ:
            sz = ExpandEnvironmentStringsW((LPCWSTR)value, NULL, 0);
            *appValue = msi_alloc(sz * sizeof(WCHAR));
            ExpandEnvironmentStringsW((LPCWSTR)value, *appValue, sz);
            break;
        case REG_BINARY:
            /* #x<nibbles>\0 */
            *appValue = msi_alloc((sz * 2 + 3) * sizeof(WCHAR));
            lstrcpyW(*appValue, binPre);
            ptr = *appValue + lstrlenW(binPre);
            for (i = 0; i < sz; i++, ptr += 2)
                swprintf(ptr, 3, binFmt, value[i]);
            break;
        default:
            WARN("unimplemented for values of type %d\n", regType);
            *appValue = NULL;
    }
}

static UINT search_directory( MSIPACKAGE *, MSISIGNATURE *, const WCHAR *, int, WCHAR ** );

static UINT search_reg( MSIPACKAGE *package, WCHAR **appValue, MSISIGNATURE *sig )
{
    static const WCHAR query[] =  {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'R','e','g','L','o','c','a','t','o','r',' ','W','H','E','R','E',' ',
        'S','i','g','n','a','t','u','r','e','_',' ','=',' ', '\'','%','s','\'',0};
    const WCHAR *keyPath, *valueName;
    WCHAR *deformatted = NULL, *ptr = NULL, *end;
    int root, type;
    REGSAM access = KEY_READ;
    HKEY rootKey, key = NULL;
    DWORD sz = 0, regType;
    LPBYTE value = NULL;
    MSIRECORD *row;
    UINT rc;

    TRACE("%s\n", debugstr_w(sig->Name));

    *appValue = NULL;

    row = MSI_QueryGetRecord( package->db, query, sig->Name );
    if (!row)
    {
        TRACE("failed to query RegLocator for %s\n", debugstr_w(sig->Name));
        return ERROR_SUCCESS;
    }

    root = MSI_RecordGetInteger(row, 2);
    keyPath = MSI_RecordGetString(row, 3);
    valueName = MSI_RecordGetString(row, 4);
    type = MSI_RecordGetInteger(row, 5);

    deformat_string(package, keyPath, &deformatted);

    switch (root)
    {
    case msidbRegistryRootClassesRoot:
        rootKey = HKEY_CLASSES_ROOT;
        break;
    case msidbRegistryRootCurrentUser:
        rootKey = HKEY_CURRENT_USER;
        break;
    case msidbRegistryRootLocalMachine:
        rootKey = HKEY_LOCAL_MACHINE;
        if (type & msidbLocatorType64bit) access |= KEY_WOW64_64KEY;
        else access |= KEY_WOW64_32KEY;
        break;
    case msidbRegistryRootUsers:
        rootKey = HKEY_USERS;
        break;
    default:
        WARN("Unknown root key %d\n", root);
        goto end;
    }

    rc = RegOpenKeyExW( rootKey, deformatted, 0, access, &key );
    if (rc)
    {
        TRACE("RegOpenKeyExW returned %d\n", rc);
        goto end;
    }

    msi_free(deformatted);
    deformat_string(package, valueName, &deformatted);

    rc = RegQueryValueExW(key, deformatted, NULL, NULL, NULL, &sz);
    if (rc)
    {
        TRACE("RegQueryValueExW returned %d\n", rc);
        goto end;
    }
    /* FIXME: sanity-check sz before allocating (is there an upper-limit
     * on the value of a property?)
     */
    value = msi_alloc( sz );
    rc = RegQueryValueExW(key, deformatted, NULL, &regType, value, &sz);
    if (rc)
    {
        TRACE("RegQueryValueExW returned %d\n", rc);
        goto end;
    }

    /* bail out if the registry key is empty */
    if (sz == 0)
        goto end;

    /* expand if needed */
    if (regType == REG_EXPAND_SZ)
    {
        sz = ExpandEnvironmentStringsW((LPCWSTR)value, NULL, 0);
        if (sz)
        {
            LPWSTR buf = msi_alloc(sz * sizeof(WCHAR));
            ExpandEnvironmentStringsW((LPCWSTR)value, buf, sz);
            msi_free(value);
            value = (LPBYTE)buf;
        }
    }

    if ((regType == REG_SZ || regType == REG_EXPAND_SZ) &&
        (ptr = wcschr((LPWSTR)value, '"')) && (end = wcschr(++ptr, '"')))
        *end = '\0';
    else
        ptr = (LPWSTR)value;

    switch (type & 0x0f)
    {
    case msidbLocatorTypeDirectory:
        search_directory( package, sig, ptr, 0, appValue );
        break;
    case msidbLocatorTypeFileName:
        *appValue = search_file( package, ptr, sig );
        break;
    case msidbLocatorTypeRawValue:
        convert_reg_value( regType, value, sz, appValue );
        break;
    default:
        FIXME("unimplemented for type %d (key path %s, value %s)\n",
              type, debugstr_w(keyPath), debugstr_w(valueName));
    }
end:
    msi_free( value );
    RegCloseKey( key );
    msi_free( deformatted );

    msiobj_release(&row->hdr);
    return ERROR_SUCCESS;
}

static LPWSTR get_ini_field(LPWSTR buf, int field)
{
    LPWSTR beg, end;
    int i = 1;

    if (field == 0)
        return strdupW(buf);

    beg = buf;
    while ((end = wcschr(beg, ',')) && i < field)
    {
        beg = end + 1;
        while (*beg == ' ')
            beg++;

        i++;
    }

    end = wcschr(beg, ',');
    if (!end)
        end = beg + lstrlenW(beg);

    *end = '\0';
    return strdupW(beg);
}

static UINT search_ini( MSIPACKAGE *package, WCHAR **appValue, MSISIGNATURE *sig )
{
    static const WCHAR query[] =  {
        's','e','l','e','c','t',' ','*',' ',
        'f','r','o','m',' ',
        'I','n','i','L','o','c','a','t','o','r',' ',
        'w','h','e','r','e',' ',
        'S','i','g','n','a','t','u','r','e','_',' ','=',' ','\'','%','s','\'',0};
    MSIRECORD *row;
    LPWSTR fileName, section, key;
    int field, type;
    WCHAR buf[MAX_PATH];

    TRACE("%s\n", debugstr_w(sig->Name));

    *appValue = NULL;

    row = MSI_QueryGetRecord( package->db, query, sig->Name );
    if (!row)
    {
        TRACE("failed to query IniLocator for %s\n", debugstr_w(sig->Name));
        return ERROR_SUCCESS;
    }

    fileName = msi_dup_record_field(row, 2);
    section = msi_dup_record_field(row, 3);
    key = msi_dup_record_field(row, 4);
    field = MSI_RecordGetInteger(row, 5);
    type = MSI_RecordGetInteger(row, 6);
    if (field == MSI_NULL_INTEGER)
        field = 0;
    if (type == MSI_NULL_INTEGER)
        type = 0;

    GetPrivateProfileStringW(section, key, NULL, buf, MAX_PATH, fileName);
    if (buf[0])
    {
        switch (type & 0x0f)
        {
        case msidbLocatorTypeDirectory:
            search_directory( package, sig, buf, 0, appValue );
            break;
        case msidbLocatorTypeFileName:
            *appValue = search_file( package, buf, sig );
            break;
        case msidbLocatorTypeRawValue:
            *appValue = get_ini_field(buf, field);
            break;
        }
    }

    msi_free(fileName);
    msi_free(section);
    msi_free(key);

    msiobj_release(&row->hdr);

    return ERROR_SUCCESS;
}

/* Expands the value in src into a path without property names and only
 * containing long path names into dst.  Replaces at most len characters of dst,
 * and always NULL-terminates dst if dst is not NULL and len >= 1.
 * May modify src.
 * Assumes src and dst are non-overlapping.
 * FIXME: return code probably needed:
 * - what does AppSearch return if the table values are invalid?
 * - what if dst is too small?
 */
static void expand_any_path( MSIPACKAGE *package, WCHAR *src, WCHAR *dst, size_t len )
{
    WCHAR *ptr, *deformatted;

    if (!src || !dst || !len)
    {
        if (dst) *dst = '\0';
        return;
    }

    dst[0] = '\0';

    /* Ignore the short portion of the path */
    if ((ptr = wcschr(src, '|')))
        ptr++;
    else
        ptr = src;

    deformat_string(package, ptr, &deformatted);
    if (!deformatted || lstrlenW(deformatted) > len - 1)
    {
        msi_free(deformatted);
        return;
    }

    lstrcpyW(dst, deformatted);
    dst[lstrlenW(deformatted)] = '\0';
    msi_free(deformatted);
}

static LANGID *parse_languages( const WCHAR *languages, DWORD *num_ids )
{
    UINT i, count = 1;
    WCHAR *str = strdupW( languages ), *p, *q;
    LANGID *ret;

    if (!str) return NULL;
    for (p = q = str; (q = wcschr( q, ',' )); q++) count++;

    if (!(ret = msi_alloc( count * sizeof(LANGID) )))
    {
        msi_free( str );
        return NULL;
    }
    i = 0;
    while (*p)
    {
        q = wcschr( p, ',' );
        if (q) *q = 0;
        ret[i] = wcstol( p, NULL, 10 );
        if (!q) break;
        p = q + 1;
        i++;
    }
    msi_free( str );
    *num_ids = count;
    return ret;
}

static BOOL match_languages( const void *version, const WCHAR *languages )
{
    struct lang
    {
        USHORT id;
        USHORT codepage;
    } *lang;
    DWORD len, num_ids, i, j;
    BOOL found = FALSE;
    LANGID *ids;

    if (!languages || !languages[0]) return TRUE;
    if (!VerQueryValueW( version, szLangResource, (void **)&lang, &len )) return FALSE;
    if (!(ids = parse_languages( languages, &num_ids ))) return FALSE;

    for (i = 0; i < num_ids; i++)
    {
        found = FALSE;
        for (j = 0; j < len / sizeof(struct lang); j++)
        {
            if (!ids[i] || ids[i] == lang[j].id) found = TRUE;
        }
        if (!found) goto done;
    }

done:
    msi_free( ids );
    return found;
}

/* Sets *matches to whether the file (whose path is filePath) matches the
 * versions set in sig.
 * Return ERROR_SUCCESS in case of success (whether or not the file matches),
 * something else if an install-halting error occurs.
 */
static UINT file_version_matches( MSIPACKAGE *package, const MSISIGNATURE *sig, const WCHAR *filePath,
                                  BOOL *matches )
{
    UINT len;
    void *version;
    VS_FIXEDFILEINFO *info = NULL;
    DWORD size = msi_get_file_version_info( package, filePath, 0, NULL );

    *matches = FALSE;

    if (!size) return ERROR_SUCCESS;
    if (!(version = msi_alloc( size ))) return ERROR_OUTOFMEMORY;

    if (msi_get_file_version_info( package, filePath, size, version ))
        VerQueryValueW( version, szBackSlash, (void **)&info, &len );

    if (info)
    {
        TRACE("comparing file version %d.%d.%d.%d:\n",
              HIWORD(info->dwFileVersionMS),
              LOWORD(info->dwFileVersionMS),
              HIWORD(info->dwFileVersionLS),
              LOWORD(info->dwFileVersionLS));
        if (info->dwFileVersionMS < sig->MinVersionMS
            || (info->dwFileVersionMS == sig->MinVersionMS &&
                info->dwFileVersionLS < sig->MinVersionLS))
        {
            TRACE("less than minimum version %d.%d.%d.%d\n",
                   HIWORD(sig->MinVersionMS),
                   LOWORD(sig->MinVersionMS),
                   HIWORD(sig->MinVersionLS),
                   LOWORD(sig->MinVersionLS));
        }
        else if ((sig->MaxVersionMS || sig->MaxVersionLS) &&
                 (info->dwFileVersionMS > sig->MaxVersionMS ||
                  (info->dwFileVersionMS == sig->MaxVersionMS &&
                   info->dwFileVersionLS > sig->MaxVersionLS)))
        {
            TRACE("greater than maximum version %d.%d.%d.%d\n",
                   HIWORD(sig->MaxVersionMS),
                   LOWORD(sig->MaxVersionMS),
                   HIWORD(sig->MaxVersionLS),
                   LOWORD(sig->MaxVersionLS));
        }
        else if (!match_languages( version, sig->Languages ))
        {
            TRACE("languages %s not supported\n", debugstr_w( sig->Languages ));
        }
        else *matches = TRUE;
    }
    msi_free( version );
    return ERROR_SUCCESS;
}

/* Sets *matches to whether the file in findData matches that in sig.
 * fullFilePath is assumed to be the full path of the file specified in
 * findData, which may be necessary to compare the version.
 * Return ERROR_SUCCESS in case of success (whether or not the file matches),
 * something else if an install-halting error occurs.
 */
static UINT file_matches_sig( MSIPACKAGE *package, const MSISIGNATURE *sig, const WIN32_FIND_DATAW *findData,
                              const WCHAR *fullFilePath, BOOL *matches )
{
    UINT rc = ERROR_SUCCESS;

    *matches = TRUE;
    /* assumes the caller has already ensured the filenames match, so check
     * the other fields..
     */
    if (sig->MinTime.dwLowDateTime || sig->MinTime.dwHighDateTime)
    {
        if (findData->ftCreationTime.dwHighDateTime <
         sig->MinTime.dwHighDateTime ||
         (findData->ftCreationTime.dwHighDateTime == sig->MinTime.dwHighDateTime
         && findData->ftCreationTime.dwLowDateTime <
         sig->MinTime.dwLowDateTime))
            *matches = FALSE;
    }
    if (*matches && (sig->MaxTime.dwLowDateTime || sig->MaxTime.dwHighDateTime))
    {
        if (findData->ftCreationTime.dwHighDateTime >
         sig->MaxTime.dwHighDateTime ||
         (findData->ftCreationTime.dwHighDateTime == sig->MaxTime.dwHighDateTime
         && findData->ftCreationTime.dwLowDateTime >
         sig->MaxTime.dwLowDateTime))
            *matches = FALSE;
    }
    if (*matches && sig->MinSize && findData->nFileSizeLow < sig->MinSize)
        *matches = FALSE;
    if (*matches && sig->MaxSize && findData->nFileSizeLow > sig->MaxSize)
        *matches = FALSE;
    if (*matches && (sig->MinVersionMS || sig->MinVersionLS ||
     sig->MaxVersionMS || sig->MaxVersionLS))
        rc = file_version_matches( package, sig, fullFilePath, matches );
    return rc;
}

/* Recursively searches the directory dir for files that match the signature
 * sig, up to (depth + 1) levels deep.  That is, if depth is 0, it searches dir
 * (and only dir).  If depth is 1, searches dir and its immediate
 * subdirectories.
 * Assumes sig->File is not NULL.
 * Returns ERROR_SUCCESS on success (which may include non-critical errors),
 * something else on failures which should halt the install.
 */
static UINT recurse_search_directory( MSIPACKAGE *package, WCHAR **appValue, MSISIGNATURE *sig, const WCHAR *dir,
                                      int depth )
{
    static const WCHAR starDotStarW[] = { '*','.','*',0 };
    HANDLE hFind;
    WIN32_FIND_DATAW findData;
    UINT rc = ERROR_SUCCESS;
    size_t dirLen = lstrlenW(dir), fileLen = lstrlenW(sig->File);
    WCHAR subpath[MAX_PATH];
    WCHAR *buf;
    DWORD len;

    TRACE("Searching directory %s for file %s, depth %d\n", debugstr_w(dir), debugstr_w(sig->File), depth);

    if (depth < 0)
        return ERROR_SUCCESS;

    *appValue = NULL;
    /* We need the buffer in both paths below, so go ahead and allocate it
     * here.  Add two because we might need to add a backslash if the dir name
     * isn't backslash-terminated.
     */
    len = dirLen + max(fileLen, lstrlenW(starDotStarW)) + 2;
    buf = msi_alloc(len * sizeof(WCHAR));
    if (!buf)
        return ERROR_OUTOFMEMORY;

    lstrcpyW(buf, dir);
    PathAddBackslashW(buf);
    lstrcatW(buf, sig->File);

    hFind = msi_find_first_file( package, buf, &findData );
    if (hFind != INVALID_HANDLE_VALUE)
    {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            BOOL matches;

            rc = file_matches_sig( package, sig, &findData, buf, &matches );
            if (rc == ERROR_SUCCESS && matches)
            {
                TRACE("found file, returning %s\n", debugstr_w(buf));
                *appValue = buf;
            }
        }
        FindClose(hFind);
    }

    if (rc == ERROR_SUCCESS && !*appValue)
    {
        lstrcpyW(buf, dir);
        PathAddBackslashW(buf);
        lstrcatW(buf, starDotStarW);

        hFind = msi_find_first_file( package, buf, &findData );
        if (hFind != INVALID_HANDLE_VALUE)
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
                wcscmp( findData.cFileName, szDot ) &&
                wcscmp( findData.cFileName, szDotDot ))
            {
                lstrcpyW(subpath, dir);
                PathAppendW(subpath, findData.cFileName);
                rc = recurse_search_directory( package, appValue, sig, subpath, depth - 1 );
            }

            while (rc == ERROR_SUCCESS && !*appValue && msi_find_next_file( package, hFind, &findData ))
            {
                if (!wcscmp( findData.cFileName, szDot ) ||
                    !wcscmp( findData.cFileName, szDotDot ))
                    continue;

                lstrcpyW(subpath, dir);
                PathAppendW(subpath, findData.cFileName);
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    rc = recurse_search_directory( package, appValue, sig, subpath, depth - 1 );
            }

            FindClose(hFind);
        }
    }

    if (*appValue != buf)
        msi_free(buf);

    return rc;
}

static UINT check_directory( MSIPACKAGE *package, const WCHAR *dir, WCHAR **appValue )
{
    DWORD attr = msi_get_file_attributes( package, dir );

    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        TRACE("directory exists, returning %s\n", debugstr_w(dir));
        *appValue = strdupW(dir);
    }

    return ERROR_SUCCESS;
}

static BOOL is_full_path( const WCHAR *path )
{
    WCHAR first = towupper(path[0]);
    BOOL ret;

    if (first >= 'A' && first <= 'Z' && path[1] == ':')
        ret = TRUE;
    else if (path[0] == '\\' && path[1] == '\\')
        ret = TRUE;
    else
        ret = FALSE;
    return ret;
}

static UINT search_directory( MSIPACKAGE *package, MSISIGNATURE *sig, const WCHAR *path, int depth, WCHAR **appValue )
{
    UINT rc;
    DWORD attr;
    LPWSTR val = NULL;

    TRACE("%p, %p, %s, %d, %p\n", package, sig, debugstr_w(path), depth, appValue);

    if (is_full_path( path ))
    {
        if (sig->File)
            rc = recurse_search_directory( package, &val, sig, path, depth );
        else
        {
            /* Recursively searching a directory makes no sense when the
             * directory to search is the thing you're trying to find.
             */
            rc = check_directory( package, path, &val );
        }
    }
    else
    {
        WCHAR pathWithDrive[MAX_PATH] = { 'C',':','\\',0 };
        DWORD drives = GetLogicalDrives();
        int i;

        rc = ERROR_SUCCESS;
        for (i = 0; rc == ERROR_SUCCESS && !val && i < 26; i++)
        {
            if (!(drives & (1 << i)))
                continue;

            pathWithDrive[0] = 'A' + i;
            if (GetDriveTypeW(pathWithDrive) != DRIVE_FIXED)
                continue;

            lstrcpynW(pathWithDrive + 3, path, ARRAY_SIZE(pathWithDrive) - 3);

            if (sig->File)
                rc = recurse_search_directory( package, &val, sig, pathWithDrive, depth );
            else
                rc = check_directory( package, pathWithDrive, &val );
        }
    }

    attr = msi_get_file_attributes( package, val );
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) &&
        val && val[lstrlenW(val) - 1] != '\\')
    {
        val = msi_realloc(val, (lstrlenW(val) + 2) * sizeof(WCHAR));
        if (!val)
            rc = ERROR_OUTOFMEMORY;
        else
            PathAddBackslashW(val);
    }

    *appValue = val;

    TRACE("returning %d\n", rc);
    return rc;
}

static UINT search_sig_name( MSIPACKAGE *, const WCHAR *, MSISIGNATURE *, WCHAR ** );

static UINT search_dr( MSIPACKAGE *package, WCHAR **appValue, MSISIGNATURE *sig )
{
    static const WCHAR query[] =  {
        's','e','l','e','c','t',' ','*',' ',
        'f','r','o','m',' ',
        'D','r','L','o','c','a','t','o','r',' ',
        'w','h','e','r','e',' ',
        'S','i','g','n','a','t','u','r','e','_',' ','=',' ', '\'','%','s','\'',0};
    LPWSTR parent = NULL;
    LPCWSTR parentName;
    WCHAR path[MAX_PATH];
    WCHAR expanded[MAX_PATH];
    MSIRECORD *row;
    int depth;
    DWORD sz, attr;
    UINT rc;

    TRACE("%s\n", debugstr_w(sig->Name));

    *appValue = NULL;

    row = MSI_QueryGetRecord( package->db, query, sig->Name );
    if (!row)
    {
        TRACE("failed to query DrLocator for %s\n", debugstr_w(sig->Name));
        return ERROR_SUCCESS;
    }

    /* check whether parent is set */
    parentName = MSI_RecordGetString(row, 2);
    if (parentName)
    {
        MSISIGNATURE parentSig;

        search_sig_name( package, parentName, &parentSig, &parent );
        free_signature( &parentSig );
        if (!parent)
        {
            msiobj_release(&row->hdr);
            return ERROR_SUCCESS;
        }
    }

    sz = MAX_PATH;
    MSI_RecordGetStringW(row, 3, path, &sz);

    if (MSI_RecordIsNull(row,4))
        depth = 0;
    else
        depth = MSI_RecordGetInteger(row,4);

    if (sz)
        expand_any_path( package, path, expanded, MAX_PATH );
    else
        lstrcpyW(expanded, path);

    if (parent)
    {
        attr = msi_get_file_attributes( package, parent );
        if (attr != INVALID_FILE_ATTRIBUTES &&
            !(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            PathRemoveFileSpecW(parent);
            PathAddBackslashW(parent);
        }

        lstrcpyW(path, parent);
        lstrcatW(path, expanded);
    }
    else if (sz) lstrcpyW(path, expanded);

    PathAddBackslashW(path);

    rc = search_directory( package, sig, path, depth, appValue );

    msi_free(parent);
    msiobj_release(&row->hdr);
    TRACE("returning %d\n", rc);
    return rc;
}

static UINT search_sig_name( MSIPACKAGE *package, const WCHAR *sigName, MSISIGNATURE *sig, WCHAR **appValue )
{
    UINT rc;

    *appValue = NULL;
    rc = get_signature( package, sig, sigName );
    if (rc == ERROR_SUCCESS)
    {
        rc = search_components( package, appValue, sig );
        if (rc == ERROR_SUCCESS && !*appValue)
        {
            rc = search_reg( package, appValue, sig );
            if (rc == ERROR_SUCCESS && !*appValue)
            {
                rc = search_ini( package, appValue, sig );
                if (rc == ERROR_SUCCESS && !*appValue)
                    rc = search_dr( package, appValue, sig );
            }
        }
    }
    return rc;
}

static UINT ITERATE_AppSearch(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = param;
    LPCWSTR propName, sigName;
    LPWSTR value = NULL;
    MSISIGNATURE sig;
    MSIRECORD *uirow;
    UINT r;

    /* get property and signature */
    propName = MSI_RecordGetString(row, 1);
    sigName = MSI_RecordGetString(row, 2);

    TRACE("%s %s\n", debugstr_w(propName), debugstr_w(sigName));

    r = search_sig_name( package, sigName, &sig, &value );
    if (value)
    {
        r = msi_set_property( package->db, propName, value, -1 );
        if (r == ERROR_SUCCESS && !wcscmp( propName, szSourceDir ))
            msi_reset_source_folders( package );

        msi_free(value);
    }
    free_signature( &sig );

    uirow = MSI_CreateRecord( 2 );
    MSI_RecordSetStringW( uirow, 1, propName );
    MSI_RecordSetStringW( uirow, 2, sigName );
    MSI_ProcessMessage(package, INSTALLMESSAGE_ACTIONDATA, uirow);
    msiobj_release( &uirow->hdr );

    return r;
}

UINT ACTION_AppSearch(MSIPACKAGE *package)
{
    static const WCHAR query[] =  {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'A','p','p','S','e','a','r','c','h',0};
    MSIQUERY *view;
    UINT r;

    if (msi_action_is_unique(package, szAppSearch))
    {
        TRACE("Skipping AppSearch action: already done in UI sequence\n");
        return ERROR_SUCCESS;
    }
    else
        msi_register_unique_action(package, szAppSearch);

    r = MSI_OpenQuery( package->db, &view, query );
    if (r != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    r = MSI_IterateRecords( view, NULL, ITERATE_AppSearch, package );
    msiobj_release( &view->hdr );
    return r;
}

static UINT ITERATE_CCPSearch(MSIRECORD *row, LPVOID param)
{
    MSIPACKAGE *package = param;
    LPCWSTR signature;
    LPWSTR value = NULL;
    MSISIGNATURE sig;
    UINT r = ERROR_SUCCESS;

    static const WCHAR success[] = {'C','C','P','_','S','u','c','c','e','s','s',0};

    signature = MSI_RecordGetString(row, 1);

    TRACE("%s\n", debugstr_w(signature));

    search_sig_name( package, signature, &sig, &value );
    if (value)
    {
        TRACE("Found signature %s\n", debugstr_w(signature));
        msi_set_property( package->db, success, szOne, -1 );
        msi_free(value);
        r = ERROR_NO_MORE_ITEMS;
    }

    free_signature(&sig);
    return r;
}

UINT ACTION_CCPSearch(MSIPACKAGE *package)
{
    static const WCHAR query[] =  {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        'C','C','P','S','e','a','r','c','h',0};
    MSIQUERY *view;
    UINT r;

    if (msi_action_is_unique(package, szCCPSearch))
    {
        TRACE("Skipping AppSearch action: already done in UI sequence\n");
        return ERROR_SUCCESS;
    }
    else
        msi_register_unique_action(package, szCCPSearch);

    r = MSI_OpenQuery(package->db, &view, query);
    if (r != ERROR_SUCCESS)
        return ERROR_SUCCESS;

    r = MSI_IterateRecords(view, NULL, ITERATE_CCPSearch, package);
    msiobj_release(&view->hdr);
    return r;
}