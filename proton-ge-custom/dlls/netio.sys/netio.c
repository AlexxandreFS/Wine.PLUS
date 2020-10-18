/*
 * NETIO driver stub.
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winioctl.h"
#include "winternl.h"
#include "winsock2.h"
#include "ddk/wdm.h"
#include "wine/debug.h"
#include "winsock2.h"
#include "ws2tcpip.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(netio);

typedef NTSTATUS (* WINAPI PFN_WSK_CLIENT_EVENT)(PVOID ClientContext, ULONG EventType, PVOID Information, SIZE_T InformationLength);

typedef PVOID PWSK_CLIENT;

typedef struct _WSK_CLIENT_DISPATCH {
  USHORT               Version;
  USHORT               Reserved;
  PFN_WSK_CLIENT_EVENT WskClientEvent;
} WSK_CLIENT_DISPATCH, *PWSK_CLIENT_DISPATCH;

typedef struct _WSK_CLIENT_NPI {
  PVOID                     ClientContext;
  const WSK_CLIENT_DISPATCH *Dispatch;
} WSK_CLIENT_NPI, *PWSK_CLIENT_NPI;

typedef struct _WSK_REGISTRATION {
  ULONGLONG  ReservedRegistrationState;
  PVOID      ReservedRegistrationContext;
  KSPIN_LOCK ReservedRegistrationLock;
} WSK_REGISTRATION, *PWSK_REGISTRATION;

typedef struct _WSK_BUF {
  PMDL   Mdl;
  ULONG  Offset;
  SIZE_T Length;
} WSK_BUF, *PWSK_BUF;

typedef struct _WSK_DATA_INDICATION {
  struct _WSK_DATA_INDICATION *Next;
  WSK_BUF                     Buffer;
} WSK_DATA_INDICATION, *PWSK_DATA_INDICATION;

typedef NTSTATUS (* WINAPI PFN_WSK_RECEIVE_EVENT)(
  PVOID SocketContext,
  ULONG Flags,
  PWSK_DATA_INDICATION DataIndication,
  SIZE_T BytesIndicated,
  SIZE_T *BytesAccepted
);

typedef NTSTATUS (* WINAPI PFN_WSK_DISCONNECT_EVENT)(
  PVOID SocketContext,
  ULONG Flags
);

typedef NTSTATUS (* WINAPI PFN_WSK_SEND_BACKLOG_EVENT)(
  PVOID SocketContext,
  SIZE_T IdealBacklogSize
);

typedef struct _WSK_CLIENT_CONNECTION_DISPATCH {
  PFN_WSK_RECEIVE_EVENT      WskReceiveEvent;
  PFN_WSK_DISCONNECT_EVENT   WskDisconnectEvent;
  PFN_WSK_SEND_BACKLOG_EVENT WskSendBacklogEvent;
} WSK_CLIENT_CONNECTION_DISPATCH, *PWSK_CLIENT_CONNECTION_DISPATCH;

typedef NTSTATUS (* WINAPI PFN_WSK_SOCKET)(
  PWSK_CLIENT Client,
  ADDRESS_FAMILY AddressFamily,
  USHORT SocketType,
  ULONG Protocol,
  ULONG Flags,
  PVOID SocketContext,
  const VOID *Dispatch,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PSECURITY_DESCRIPTOR SecurityDescriptor,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_SOCKET_CONNECT)(
  PWSK_CLIENT Client,
  USHORT SocketType,
  ULONG Protocol,
  PSOCKADDR LocalAddress,
  PSOCKADDR RemoteAddress,
  ULONG Flags,
  PVOID SocketContext,
  const WSK_CLIENT_CONNECTION_DISPATCH *Dispatch,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PSECURITY_DESCRIPTOR SecurityDescriptor,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_CONTROL_CLIENT)(
  PWSK_CLIENT Client,
  ULONG ControlCode,
  SIZE_T InputSize,
  PVOID InputBuffer,
  SIZE_T OutputSize,
  PVOID OutputBuffer,
  SIZE_T *OutputSizeReturned,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_GET_ADDRESS_INFO)(
  PWSK_CLIENT Client,
  PUNICODE_STRING NodeName,
  PUNICODE_STRING ServiceName,
  ULONG NameSpace,
  GUID *Provider,
  PADDRINFOEXW Hints,
  PADDRINFOEXW *Result,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PIRP Irp
);

typedef void (* WINAPI PFN_WSK_FREE_ADDRESS_INFO)(
  PWSK_CLIENT Client,
  PADDRINFOEXW AddrInfo
);

typedef NTSTATUS (* WINAPI PFN_WSK_GET_NAME_INFO)(
  PWSK_CLIENT Client,
  PSOCKADDR SockAddr,
  ULONG SockAddrLength,
  PUNICODE_STRING NodeName,
  PUNICODE_STRING ServiceName,
  ULONG Flags,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PIRP Irp
);

typedef struct _WSK_SOCKET {
    const VOID *Dispatch;
    SOCKET s;
    void *socket_client_context;
    int socket_type;
    ULONG flags;
} WSK_SOCKET, *PWSK_SOCKET;


typedef enum _WSK_CONTROL_SOCKET_TYPE
{
     WskSetOption,
     WskGetOption,
     WskIoctl,
} WSK_CONTROL_SOCKET_TYPE;

typedef NTSTATUS (* WINAPI PFN_WSK_CONTROL_SOCKET)(
  PWSK_SOCKET Socket,
  WSK_CONTROL_SOCKET_TYPE RequestType,
  ULONG ControlCode,
  ULONG Level,
  SIZE_T InputSize,
  PVOID InputBuffer,
  SIZE_T OutputSize,
  PVOID OutputBuffer,
  SIZE_T *OutputSizeReturned,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_CLOSE_SOCKET)(
  PWSK_SOCKET Socket,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_BIND)(
  PWSK_SOCKET Socket,
  PSOCKADDR LocalAddress,
  ULONG Flags,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_ACCEPT)(
  PWSK_SOCKET ListenSocket,
  ULONG Flags,
  PVOID AcceptSocketContext,
  const WSK_CLIENT_CONNECTION_DISPATCH *AcceptSocketDispatch,
  PSOCKADDR LocalAddress,
  PSOCKADDR RemoteAddress,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_CONNECT)(
  PWSK_SOCKET Socket,
  PSOCKADDR RemoteAddress,
  ULONG Flags,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_LISTEN)(
  PWSK_SOCKET Socket,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_SEND)(
  PWSK_SOCKET Socket,
  PWSK_BUF Buffer,
  ULONG Flags,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_RECEIVE)(
  PWSK_SOCKET Socket,
  PWSK_BUF Buffer,
  ULONG Flags,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_DISCONNECT)(
  PWSK_SOCKET Socket,
  PWSK_BUF Buffer,
  ULONG Flags,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_GET_LOCAL_ADDRESS)(
  PWSK_SOCKET Socket,
  PSOCKADDR LocalAddress,
  PIRP Irp
);


typedef NTSTATUS (* WINAPI PFN_WSK_GET_REMOTE_ADDRESS)(
  PWSK_SOCKET Socket,
  PSOCKADDR RemoteAddress,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_CONNECT_EX)(
  PWSK_SOCKET Socket,
  PSOCKADDR RemoteAddress,
  PWSK_BUF Buffer,
  ULONG Flags,
  PIRP Irp
);

typedef void *PFN_WSK_SEND_EX;
typedef void *PFN_WSK_RECEIVE_EX;

typedef NTSTATUS (* WINAPI PFN_WSK_RELEASE_DATA_INDICATION_LIST)(
  PWSK_SOCKET          Socket,
  PWSK_DATA_INDICATION DataIndication
);

typedef struct _WSACMSGHDR {
  SIZE_T cmsg_len;
  INT    cmsg_level;
  INT    cmsg_type;
} WSACMSGHDR, *PWSACMSGHDR, *LPWSACMSGHDR;

typedef WSACMSGHDR PCMSGHDR;

typedef NTSTATUS (* WINAPI PFN_WSK_SEND_TO)(
  PWSK_SOCKET Socket,
  PWSK_BUF Buffer,
  ULONG Flags,
  PSOCKADDR RemoteAddress,
  ULONG ControlInfoLength,
  PCMSGHDR ControlInfo,
  PIRP Irp
);

typedef NTSTATUS (* WINAPI PFN_WSK_RECEIVE_FROM)(
  PWSK_SOCKET Socket,
  PWSK_BUF Buffer,
  ULONG Flags,
  PSOCKADDR RemoteAddress,
  PULONG ControlLength,
  PCMSGHDR ControlInfo,
  PULONG ControlFlags,
  PIRP Irp
);

typedef struct _WSK_DATAGRAM_INDICATION {
  struct _WSK_DATAGRAM_INDICATION *Next;
  WSK_BUF                         Buffer;
  PCMSGHDR                        ControlInfo;
  ULONG                           ControlInfoLength;
  PSOCKADDR                       RemoteAddress;
} WSK_DATAGRAM_INDICATION, *PWSK_DATAGRAM_INDICATION;

typedef NTSTATUS (* WINAPI PFN_WSK_RELEASE_DATAGRAM_INDICATION_LIST)(
  PWSK_SOCKET Socket,
  PWSK_DATAGRAM_INDICATION DatagramIndication
);

typedef void *PFN_WSK_SEND_MESSAGES;

typedef struct _WSK_INSPECT_ID {
  ULONG_PTR Key;
  ULONG     SerialNumber;
} WSK_INSPECT_ID, *PWSK_INSPECT_ID;

typedef enum _WSK_INSPECT_ACTION
{
    WskInspectReject,
    WskInspectAccept,
} WSK_INSPECT_ACTION;

typedef NTSTATUS (* WINAPI PFN_WSK_INSPECT_COMPLETE)(
  PWSK_SOCKET ListenSocket,
  PWSK_INSPECT_ID InspectID,
  WSK_INSPECT_ACTION Action,
  PIRP Irp
);

typedef struct _WSK_PROVIDER_BASIC_DISPATCH {
  PFN_WSK_CONTROL_SOCKET WskControlSocket;
  PFN_WSK_CLOSE_SOCKET   WskCloseSocket;
} WSK_PROVIDER_BASIC_DISPATCH, *PWSK_PROVIDER_BASIC_DISPATCH;

typedef struct _WSK_PROVIDER_STREAM_DISPATCH {
  WSK_PROVIDER_BASIC_DISPATCH          Basic;
  PFN_WSK_BIND                         WskBind;
  PFN_WSK_ACCEPT                       WskAccept;
  PFN_WSK_CONNECT                      WskConnect;
  PFN_WSK_LISTEN                       WskListen;
  PFN_WSK_SEND                         WskSend;
  PFN_WSK_RECEIVE                      WskReceive;
  PFN_WSK_DISCONNECT                   WskDisconnect;
  PFN_WSK_RELEASE_DATA_INDICATION_LIST WskRelease;
  PFN_WSK_GET_LOCAL_ADDRESS            WskGetLocalAddress;
  PFN_WSK_GET_REMOTE_ADDRESS           WskGetRemoteAddress;
  PFN_WSK_CONNECT_EX                   WskConnectEx;
  PFN_WSK_SEND_EX                      WskSendEx;
  PFN_WSK_RECEIVE_EX                   WskReceiveEx;
} WSK_PROVIDER_STREAM_DISPATCH, *PWSK_PROVIDER_STREAM_DISPATCH;

typedef struct _WSK_PROVIDER_DISPATCH {
  USHORT                    Version;
  USHORT                    Reserved;
  PFN_WSK_SOCKET            WskSocket;
  PFN_WSK_SOCKET_CONNECT    WskSocketConnect;
  PFN_WSK_CONTROL_CLIENT    WskControlClient;
  PFN_WSK_GET_ADDRESS_INFO  WskGetAddressInfo;
  PFN_WSK_FREE_ADDRESS_INFO WskFreeAddressInfo;
  PFN_WSK_GET_NAME_INFO     WskGetNameInfo;
} WSK_PROVIDER_DISPATCH, *PWSK_PROVIDER_DISPATCH;

typedef struct _WSK_PROVIDER_CONNECTION_DISPATCH {
  WSK_PROVIDER_BASIC_DISPATCH          Basic;
  PFN_WSK_BIND                         WskBind;
  PFN_WSK_CONNECT                      WskConnect;
  PFN_WSK_GET_LOCAL_ADDRESS            WskGetLocalAddress;
  PFN_WSK_GET_REMOTE_ADDRESS           WskGetRemoteAddress;
  PFN_WSK_SEND                         WskSend;
  PFN_WSK_RECEIVE                      WskReceive;
  PFN_WSK_DISCONNECT                   WskDisconnect;
  PFN_WSK_RELEASE_DATA_INDICATION_LIST WskRelease;
  PFN_WSK_CONNECT_EX                   WskConnectEx;
  PFN_WSK_SEND_EX                      WskSendEx;
  PFN_WSK_RECEIVE_EX                   WskReceiveEx;
} WSK_PROVIDER_CONNECTION_DISPATCH, *PWSK_PROVIDER_CONNECTION_DISPATCH;

typedef struct _WSK_PROVIDER_NPI {
  PWSK_CLIENT                 Client;
  const WSK_PROVIDER_DISPATCH *Dispatch;
} WSK_PROVIDER_NPI, *PWSK_PROVIDER_NPI;


static void WINAPI driver_unload(DRIVER_OBJECT *driver)
{
    TRACE("driver %p.\n", driver);
}

static NTSTATUS sock_error_to_ntstatus( DWORD err )
{
    switch (err)
    {
    case 0:                    return STATUS_SUCCESS;
    case WSAEBADF:             return STATUS_INVALID_HANDLE;
    case WSAEACCES:            return STATUS_ACCESS_DENIED;
    case WSAEFAULT:            return STATUS_NO_MEMORY;
    case WSAEINVAL:            return STATUS_INVALID_PARAMETER;
    case WSAEMFILE:            return STATUS_TOO_MANY_OPENED_FILES;
    case WSAEWOULDBLOCK:       return STATUS_CANT_WAIT;
    case WSAEINPROGRESS:       return STATUS_PENDING;
    case WSAEALREADY:          return STATUS_NETWORK_BUSY;
    case WSAENOTSOCK:          return STATUS_OBJECT_TYPE_MISMATCH;
    case WSAEDESTADDRREQ:      return STATUS_INVALID_PARAMETER;
    case WSAEMSGSIZE:          return STATUS_BUFFER_OVERFLOW;
    case WSAEPROTONOSUPPORT:
    case WSAESOCKTNOSUPPORT:
    case WSAEPFNOSUPPORT:
    case WSAEAFNOSUPPORT:
    case WSAEPROTOTYPE:        return STATUS_NOT_SUPPORTED;
    case WSAENOPROTOOPT:       return STATUS_INVALID_PARAMETER;
    case WSAEOPNOTSUPP:        return STATUS_NOT_SUPPORTED;
    case WSAEADDRINUSE:        return STATUS_ADDRESS_ALREADY_ASSOCIATED;
    case WSAEADDRNOTAVAIL:     return STATUS_INVALID_PARAMETER;
    case WSAECONNREFUSED:      return STATUS_CONNECTION_REFUSED;
    case WSAESHUTDOWN:         return STATUS_PIPE_DISCONNECTED;
    case WSAENOTCONN:          return STATUS_CONNECTION_DISCONNECTED;
    case WSAETIMEDOUT:         return STATUS_IO_TIMEOUT;
    case WSAENETUNREACH:       return STATUS_NETWORK_UNREACHABLE;
    case WSAENETDOWN:          return STATUS_NETWORK_BUSY;
    case WSAECONNRESET:        return STATUS_CONNECTION_RESET;
    case WSAECONNABORTED:      return STATUS_CONNECTION_ABORTED;
    default:
        FIXME("unmapped error %u\n", err);
        return STATUS_UNSUCCESSFUL;
    }
}

typedef struct _WSK_PROVIDER_DATAGRAM_DISPATCH {
  WSK_PROVIDER_BASIC_DISPATCH              Basic;
  PFN_WSK_BIND                             WskBind;
  PFN_WSK_SEND_TO                          WskSendTo;
  PFN_WSK_RECEIVE_FROM                     WskReceiveFrom;
  PFN_WSK_RELEASE_DATAGRAM_INDICATION_LIST WskRelease;
  PFN_WSK_GET_LOCAL_ADDRESS                WskGetLocalAddress;
  PFN_WSK_SEND_MESSAGES                    WskSendMessages;
} WSK_PROVIDER_DATAGRAM_DISPATCH, *PWSK_PROVIDER_DATAGRAM_DISPATCH;

typedef struct _WSK_PROVIDER_LISTEN_DISPATCH {
  WSK_PROVIDER_BASIC_DISPATCH Basic;
  PFN_WSK_BIND                WskBind;
  PFN_WSK_ACCEPT              WskAccept;
  PFN_WSK_INSPECT_COMPLETE    WskInspectComplete;
  PFN_WSK_GET_LOCAL_ADDRESS   WskGetLocalAddress;
} WSK_PROVIDER_LISTEN_DISPATCH, *PWSK_PROVIDER_LISTEN_DISPATCH;

#define WSK_FLAG_BASIC_SOCKET      0x00000000
#define WSK_FLAG_LISTEN_SOCKET     0x00000001
#define WSK_FLAG_CONNECTION_SOCKET 0x00000002
#define WSK_FLAG_DATAGRAM_SOCKET   0x00000004
#define WSK_FLAG_STREAM_SOCKET     0x00000008

static const WSK_PROVIDER_CONNECTION_DISPATCH wsk_provider_connection_dispatch;

static void dispatch_irp(IRP *irp, NTSTATUS status)
{
    irp->IoStatus.u.Status = status;
    --irp->CurrentLocation;
    --irp->Tail.Overlay.s.u2.CurrentStackLocation;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
}

static NTSTATUS WINAPI wsk_control_socket(PWSK_SOCKET Socket, WSK_CONTROL_SOCKET_TYPE RequestType,
        ULONG ControlCode, ULONG Level, SIZE_T InputSize, PVOID InputBuffer, SIZE_T OutputSize,
        PVOID OutputBuffer, SIZE_T *OutputSizeReturned, PIRP Irp)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI wsk_close_socket(PWSK_SOCKET socket, PIRP irp)
{
    NTSTATUS status;

    TRACE("socket %p, irp %p.\n", socket, irp);

    if (closesocket(socket->s))
        status = sock_error_to_ntstatus(WSAGetLastError());
    else
        status = STATUS_SUCCESS;

    heap_free(socket);

    dispatch_irp(irp, status);
    return status;
}

static NTSTATUS WINAPI wsk_bind(PWSK_SOCKET socket, PSOCKADDR local_address, ULONG flags, PIRP irp)
{
    NTSTATUS status;

    TRACE("socket %p, local_address %p, flags %#x, irp %p, ret %p.\n",
            socket, local_address, flags, irp, __builtin_return_address(0));

    if (bind(socket->s, local_address, sizeof(*local_address)))
        status = sock_error_to_ntstatus(WSAGetLastError());
    else if (socket->flags & WSK_FLAG_LISTEN_SOCKET && listen(socket->s, 10))
        status = sock_error_to_ntstatus(WSAGetLastError());
    else
        status = STATUS_SUCCESS;

    TRACE("status %#x.\n", status);
    irp->IoStatus.Information = status;
    dispatch_irp(irp, status);
    return status;
}

struct socket_callback_context
{
    WSK_SOCKET *socket;
    IRP *irp;
    PSOCKADDR remote_address;
    void *accept_socket_client_context;
};

static void WINAPI accept_callback(TP_CALLBACK_INSTANCE *instance, void *context_)
{
    struct socket_callback_context *context = context_;
    WSK_SOCKET *accept_socket;
    NTSTATUS status;
    SOCKET s;

    TRACE("instance %p, context %p.\n", instance, context);

    if ((s = accept(context->socket->s, context->remote_address, NULL)) == INVALID_SOCKET)
    {
        status = sock_error_to_ntstatus(WSAGetLastError());
    }
    else if (!(accept_socket = heap_alloc_zero(sizeof(*accept_socket))))
    {
        ERR("No memory.\n");
        status = STATUS_NO_MEMORY;
    }
    else
    {
        TRACE("accept_socket %p.\n", accept_socket);
        accept_socket->s = s;
        accept_socket->socket_client_context = context->accept_socket_client_context;
        accept_socket->Dispatch = &wsk_provider_connection_dispatch;
        accept_socket->socket_type = context->socket->socket_type;
        accept_socket->flags = WSK_FLAG_CONNECTION_SOCKET;

        context->irp->IoStatus.Information = (ULONG_PTR)accept_socket;
        status = STATUS_SUCCESS;
    }

    TRACE("status %#x.\n", status);
    dispatch_irp(context->irp, status);
    heap_free(context);
}

static NTSTATUS WINAPI wsk_accept(PWSK_SOCKET listen_socket, ULONG flags, PVOID accept_socket_context,
        const WSK_CLIENT_CONNECTION_DISPATCH *accept_socker_dispatch,
        PSOCKADDR local_address, PSOCKADDR remote_address, PIRP irp)
{
    struct socket_callback_context *context;
    NTSTATUS status;

    TRACE("listen_socket %p, flags %#x, accept_socket_context %p, accept_socker_dispatch %p,"
            " local_address %p, remote_address %p, irp %p.\n",
            listen_socket, flags, accept_socket_context, accept_socker_dispatch, local_address, remote_address, irp);

    if (!(context = heap_alloc(sizeof(*context))))
    {
        ERR("No memory.\n");
        status = STATUS_NO_MEMORY;
        dispatch_irp(irp, status);
        return status;
    }

    context->socket = listen_socket;
    context->irp = irp;
    context->remote_address = remote_address;
    context->accept_socket_client_context = accept_socket_context;

    if (!TrySubmitThreadpoolCallback(accept_callback, context, NULL))
    {
        ERR("Could not submit thread pool callback.\n");
        status = STATUS_UNSUCCESSFUL;
        dispatch_irp(irp, status);
        heap_free(context);
        return status;
    }

    TRACE("Submitted threadpool callback, context %p.\n", context);

    return STATUS_PENDING;
}

static NTSTATUS WINAPI wsk_inspect_complete(PWSK_SOCKET ListenSocket, PWSK_INSPECT_ID InspectID,
        WSK_INSPECT_ACTION Action, PIRP Irp)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI wsk_get_local_address(PWSK_SOCKET Socket, PSOCKADDR LocalAddress, PIRP Irp)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static const WSK_PROVIDER_LISTEN_DISPATCH wsk_provider_listen_dispatch =
{
    {
        wsk_control_socket,
        wsk_close_socket,
    },
    wsk_bind,
    wsk_accept,
    wsk_inspect_complete,
    wsk_get_local_address,
};

static void WINAPI connect_callback(TP_CALLBACK_INSTANCE *instance, void *context_)
{
    struct socket_callback_context *context = context_;
    NTSTATUS status;

    TRACE("instance %p, context %p.\n", instance, context);

    if (connect(context->socket->s, context->remote_address, sizeof(*context->remote_address)))
        status = sock_error_to_ntstatus(WSAGetLastError());
    else
        status = STATUS_SUCCESS;

    TRACE("status %#x.\n", status);
    dispatch_irp(context->irp, status);
    heap_free(context);
}

static NTSTATUS WINAPI wsk_connect(PWSK_SOCKET socket, PSOCKADDR remote_address, ULONG flags, PIRP irp)
{
    struct socket_callback_context *context;
    NTSTATUS status;

    TRACE("socket %p, remote_address %p, flags %#x, irp %p.\n",
            socket, remote_address, flags, irp);

    if (!(context = heap_alloc_zero(sizeof(*context))))
    {
        ERR("No memory.\n");
        status = STATUS_NO_MEMORY;
        dispatch_irp(irp, status);
        return status;
    }

    context->socket = socket;
    context->irp = irp;
    context->remote_address = remote_address;

    if (!TrySubmitThreadpoolCallback(connect_callback, context, NULL))
    {
        ERR("Could not submit thread pool callback.\n");
        status = STATUS_UNSUCCESSFUL;
        dispatch_irp(irp, status);
        heap_free(context);
        return status;
    }

    TRACE("Submitted threadpool callback, context %p.\n", context);

    return STATUS_PENDING;
}

static NTSTATUS WINAPI wsk_get_remote_address(PWSK_SOCKET socket, PSOCKADDR remote_address, PIRP irp)
{
    FIXME("Stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

struct send_receive_context
{
    BOOL is_send;
    WSK_SOCKET *socket;
    WSK_BUF *buffer;
    ULONG flags;
    IRP *irp;
};

static void WINAPI send_receive_callback(TP_CALLBACK_INSTANCE *instance, void *context_)
{
    struct send_receive_context *context = context_;
    int buffer_length;
    NTSTATUS status;
    int length = 0;
    void *buffer;

    TRACE("instance %p, context %p, mdl %p, offset %u, length %lu.\n",
            instance, context, context->buffer->Mdl, context->buffer->Offset, context->buffer->Length);

    buffer_length = (int)context->buffer->Length;

    if (!context->buffer->Mdl && buffer_length)
    {
        status = STATUS_INVALID_PARAMETER;
        goto done;
    }

    if (context->buffer->Mdl)
    {
        /* FIXME */
        buffer = (BYTE *)context->buffer->Mdl->StartVa + context->buffer->Mdl->ByteOffset + context->buffer->Offset;
    }
    else
    {
        buffer = NULL;
    }

    if ((context->is_send && (length = send(context->socket->s, buffer, buffer_length, 0)) == SOCKET_ERROR)
            || (!context->is_send && (length = recv(context->socket->s, buffer, buffer_length, 0)) == SOCKET_ERROR))
    {
        status = sock_error_to_ntstatus(WSAGetLastError());
    }
    else
    {
        status = STATUS_SUCCESS;
        context->irp->IoStatus.Information = length;
    }

done:
    TRACE("status %#x, length %d.\n", status, length);
    dispatch_irp(context->irp, status);
    heap_free(context);
}

static NTSTATUS WINAPI do_send_receive(PWSK_SOCKET socket, PWSK_BUF buffer, ULONG flags, PIRP irp, BOOL is_send)
{
    struct send_receive_context *context;
    NTSTATUS status;

    TRACE("socket %p, buffer %p, flags %#x, irp %p, is_send %#x.\n",
            socket, buffer, flags, irp, is_send);

    if (flags)
        FIXME("flags %#x not implemented.\n", flags);

    if (!(context = heap_alloc_zero(sizeof(*context))))
    {
        ERR("No memory.\n");
        status = STATUS_NO_MEMORY;
        dispatch_irp(irp, status);
        return status;
    }

    context->is_send = is_send;
    context->socket = socket;
    context->buffer = buffer;
    context->flags = flags;
    context->irp = irp;

    if (!TrySubmitThreadpoolCallback(send_receive_callback, context, NULL))
    {
        ERR("Could not submit thread pool callback.\n");
        status = STATUS_UNSUCCESSFUL;
        dispatch_irp(irp, status);
        heap_free(context);
        return status;
    }

    TRACE("Submitted threadpool callback, context %p.\n", context);

    return STATUS_PENDING;
}

static NTSTATUS WINAPI wsk_send(PWSK_SOCKET socket, PWSK_BUF buffer, ULONG flags, PIRP irp)
{
    TRACE(".\n");
    return do_send_receive(socket, buffer, flags, irp, TRUE);
}

static NTSTATUS WINAPI wsk_receive(PWSK_SOCKET socket, PWSK_BUF buffer, ULONG flags, PIRP irp)
{

    return do_send_receive(socket, buffer, flags, irp, FALSE);
}

static NTSTATUS WINAPI wsk_disconnect(PWSK_SOCKET Socket, PWSK_BUF Buffer, ULONG Flags, PIRP Irp)
{
    FIXME("Stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI wsk_release(PWSK_SOCKET socket, PWSK_DATA_INDICATION data_indication)
{
    FIXME("Stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI wsk_connext_ex(PWSK_SOCKET Socket, PSOCKADDR remote_address, PWSK_BUF buffer,
        ULONG flags, PIRP irp)
{
    FIXME("Stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI wsk_send_ex(void)
{
    FIXME("Stub (no prototype, will crash).\n");

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI wsk_receive_ex(void)
{
    FIXME("Stub (no prototype, will crash).\n");

    return STATUS_NOT_IMPLEMENTED;
}

static const WSK_PROVIDER_CONNECTION_DISPATCH wsk_provider_connection_dispatch =
{
    {
        wsk_control_socket,
        wsk_close_socket,
    },
    wsk_bind,
    wsk_connect,
    wsk_get_local_address,
    wsk_get_remote_address,
    wsk_send,
    wsk_receive,
    wsk_disconnect,
    wsk_release,
    wsk_connext_ex,
    wsk_send_ex,
    wsk_receive_ex,
};

NTSTATUS WINAPI wsk_socket(
  PWSK_CLIENT Client,
  ADDRESS_FAMILY AddressFamily,
  USHORT SocketType,
  ULONG Protocol,
  ULONG Flags,
  PVOID SocketContext,
  const VOID *Dispatch,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PSECURITY_DESCRIPTOR SecurityDescriptor,
  PIRP irp
)
{
    WSK_SOCKET *socket;
    NTSTATUS status;
    SOCKET s;

    FIXME("Client %p, AddressFamily %#x, SocketType %#x, Protocol %#x, Flags %#x, SocketContext %p, Dispatch %p,"
            "OwningProcess %p, OwningThread %p, SecurityDescriptor %p, Irp %p, ret %p.\n",
            Client, AddressFamily, SocketType, Protocol, Flags, SocketContext, Dispatch, OwningProcess,
            OwningThread, SecurityDescriptor, irp, __builtin_return_address(0));

    if ((s = WSASocketW(AddressFamily, SocketType, Protocol, NULL, 0, 0)) == INVALID_SOCKET)
    {
        status = sock_error_to_ntstatus(WSAGetLastError());
        goto done;
    }

    socket = heap_alloc(sizeof(*socket));
    if (!socket)
    {
        status = STATUS_NO_MEMORY;
        closesocket(s);
        goto done;
    }
    socket->s = s;
    socket->socket_client_context = SocketContext;
    socket->socket_type = SocketType;
    socket->flags = Flags;

    switch (Flags)
    {
        case WSK_FLAG_LISTEN_SOCKET:
            socket->Dispatch = &wsk_provider_listen_dispatch;
            break;

        case WSK_FLAG_CONNECTION_SOCKET:
            socket->Dispatch = &wsk_provider_connection_dispatch;
            break;

        default:
            FIXME("Flags %#x not implemented.\n", Flags);
            closesocket(s);
            heap_free(socket);
            status = STATUS_NOT_IMPLEMENTED;
            goto done;
    }

    irp->IoStatus.Information = (ULONG_PTR)socket;
    status = STATUS_SUCCESS;

done:
    dispatch_irp(irp, status);
    return status;
}

NTSTATUS WINAPI wsk_socket_connect(
  PWSK_CLIENT Client,
  USHORT SocketType,
  ULONG Protocol,
  PSOCKADDR LocalAddress,
  PSOCKADDR RemoteAddress,
  ULONG Flags,
  PVOID SocketContext,
  const WSK_CLIENT_CONNECTION_DISPATCH *Dispatch,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PSECURITY_DESCRIPTOR SecurityDescriptor,
  PIRP Irp
)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI wsk_control_client(
  PWSK_CLIENT Client,
  ULONG ControlCode,
  SIZE_T InputSize,
  PVOID InputBuffer,
  SIZE_T OutputSize,
  PVOID OutputBuffer,
  SIZE_T *OutputSizeReturned,
  PIRP Irp
)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

struct wsk_get_address_info_context
{
    UNICODE_STRING *node_name;
    UNICODE_STRING *service_name;
    ULONG namespace;
    GUID *provider;
    ADDRINFOEXW *hints;
    ADDRINFOEXW **result;
    IRP *irp;
};

static void WINAPI get_address_info_callback(TP_CALLBACK_INSTANCE *instance, void *context_)
{
    struct wsk_get_address_info_context *context = context_;
    NTSTATUS status;
    INT ret;

    TRACE("instance %p, context %p.\n", instance, context);

    ret = GetAddrInfoExW( context->node_name ? context->node_name->Buffer : NULL,
            context->service_name ? context->service_name->Buffer : NULL, context->namespace,
            context->provider, context->hints, context->result, NULL, NULL, NULL, NULL);

    status = sock_error_to_ntstatus(ret);

    TRACE("status %#x.\n", status);
    dispatch_irp(context->irp, status);
    heap_free(context);
}

NTSTATUS WINAPI wsk_get_address_info(
  PWSK_CLIENT Client,
  PUNICODE_STRING NodeName,
  PUNICODE_STRING ServiceName,
  ULONG NameSpace,
  GUID *Provider,
  PADDRINFOEXW Hints,
  PADDRINFOEXW *Result,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PIRP Irp
)
{
    struct wsk_get_address_info_context *context;
    WSK_CLIENT_NPI *client = Client;
    NTSTATUS status;
    INT ret;

    FIXME("Client %p, NodeName %s, ServiceName %s, Result %p, Irp %p, ret %p stub.\n", Client,
            debugstr_w(NodeName ? NodeName->Buffer : NULL), debugstr_w(ServiceName ? ServiceName->Buffer : NULL),
            Result, Irp, __builtin_return_address (0));

    if (client && client->Dispatch && client->Dispatch->WskClientEvent)
        FIXME("WskClientEvent %p.\n", client->Dispatch->WskClientEvent);

    if (Irp)
    {
        if (!(context = heap_alloc(sizeof(*context))))
        {
            ERR("No memory.\n");
            status = STATUS_NO_MEMORY;
            dispatch_irp(Irp, status);
            return status;
        }

        context->node_name = NodeName;
        context->service_name = ServiceName;
        context->namespace = NameSpace;
        context->provider = Provider;
        context->hints = Hints;
        context->result = Result;
        context->irp = Irp;

        if (!TrySubmitThreadpoolCallback(get_address_info_callback, context, NULL))
        {
            ERR("Could not submit thread pool callback.\n");
            status = STATUS_UNSUCCESSFUL;
            dispatch_irp(Irp, status);
            heap_free(context);
            return status;
        }
        TRACE("Submitted threadpool callback, context %p.\n", context);
        return STATUS_PENDING;
    }

    ret = GetAddrInfoExW(NodeName ? NodeName->Buffer : NULL, ServiceName ? ServiceName->Buffer : NULL, NameSpace,
            Provider, Hints, Result, NULL, NULL, NULL, NULL);
    status = sock_error_to_ntstatus(ret);

    return status;
}

void WINAPI wsk_free_address_info(
  PWSK_CLIENT client_,
  PADDRINFOEXW addr_info
)
{
    WSK_CLIENT_NPI *client = client_;

    TRACE("client %p, addr_info %p.\n", client, addr_info);

    FreeAddrInfoExW(addr_info);

    if (client && client->Dispatch && client->Dispatch->WskClientEvent)
        FIXME("WskClientEvent %p.\n", client->Dispatch->WskClientEvent);
}

NTSTATUS WINAPI wsk_get_name_info(
  PWSK_CLIENT Client,
  PSOCKADDR SockAddr,
  ULONG SockAddrLength,
  PUNICODE_STRING NodeName,
  PUNICODE_STRING ServiceName,
  ULONG Flags,
  PEPROCESS OwningProcess,
  PETHREAD OwningThread,
  PIRP Irp
)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

static const WSK_PROVIDER_DISPATCH wsk_dispatch =
{
    0x100, 0,
    wsk_socket,
    wsk_socket_connect,
    wsk_control_client,
    wsk_get_address_info,
    wsk_free_address_info,
    wsk_get_name_info,
};

NTSTATUS WINAPI WskCaptureProviderNPI(PWSK_REGISTRATION WskRegistration, ULONG WaitTimeout,
        PWSK_PROVIDER_NPI WskProviderNpi)
{
    FIXME("WskRegistration %p, WaitTimeout %u, WskProviderNpi %p stub.\n",
            WskRegistration, WaitTimeout, WskProviderNpi);

    WskProviderNpi->Dispatch = &wsk_dispatch;
    WskProviderNpi->Client = WskRegistration->ReservedRegistrationContext;
    return STATUS_SUCCESS;
}

void WINAPI WskReleaseProviderNPI(PWSK_REGISTRATION WskRegistration)
{
    FIXME("WskRegistration %p stub.\n", WskRegistration);
}

NTSTATUS WINAPI WskRegister(PWSK_CLIENT_NPI WskClientNpi, PWSK_REGISTRATION WskRegistration)
{
    WORD version = MAKEWORD( 2, 2 );
    WSADATA data;

    FIXME("WskClientNpi %p, WskRegistration %p stub.\n", WskClientNpi, WskRegistration);
    TRACE("WskClientNpi->Dispatch %p, version %#x, WskClientNpi->Dispatch->WskClientEvent %p.\n",
            WskClientNpi->Dispatch, WskClientNpi->Dispatch->Version, WskClientNpi->Dispatch->WskClientEvent);

    if (WSAStartup(version, &data))
        return STATUS_INTERNAL_ERROR;

    WskRegistration->ReservedRegistrationContext = WskClientNpi;

    return STATUS_SUCCESS;
}

void WINAPI WskDeregister(PWSK_REGISTRATION WskRegistration)
{
    FIXME("WskRegistration %p stub.\n", WskRegistration);
}

typedef struct _WSK_PROVIDER_CHARACTERISTICS {
  USHORT HighestVersion;
  USHORT LowestVersion;
} WSK_PROVIDER_CHARACTERISTICS, *PWSK_PROVIDER_CHARACTERISTICS;

NTSTATUS WskQueryProviderCharacteristics(
  PWSK_REGISTRATION             WskRegistration,
  PWSK_PROVIDER_CHARACTERISTICS WskProviderCharacteristics
)
{
    FIXME("stub.\n");

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    TRACE("driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    driver->DriverUnload = driver_unload;

    return STATUS_SUCCESS;
}

