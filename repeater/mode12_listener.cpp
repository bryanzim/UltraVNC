// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include <array>
#include <cstring>

#include "repeater.h"

void error(const char* fmt, ...);

int local_resolve(const char* const host, struct sockaddr_in* addr, struct sockaddr_in* const ns)
{
    struct hostent* ent;
    if (strspn(host, "0123456789.") == std::strlen(host))
    {
        /* given by IPv4 address */
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = inet_addr(host);
    }
    else
    {
        debug("resolving host by name: %s\n", host);
        ent = gethostbyname(host);
        if (ent)
        {
            std::memcpy(&addr->sin_addr, ent->h_addr, ent->h_length);
            addr->sin_family = ent->h_addrtype;
            debug("resolved: %s (%s)\n", host, inet_ntoa(addr->sin_addr));
        }
        else
        {
            debug("failed to resolve locally.\n");
            return -1; /* failed */
        }
    }
    return 0; /* good */
}

SOCKET
open_connection(const char* const host, u_short port)
{
    std::array<char, 1024U> remote_host;
    BOOL found;
    int i;
    SOCKET s;
    struct sockaddr_in saddr;

    if (local_resolve(host, &saddr, nullptr) < 0)
    {
        error("can't resolve hostname: %s\n", host);
        return SOCKET_ERROR;
    }
    saddr.sin_port = htons(port);

    debug("connecting to %s:%u\n", inet_ntoa(saddr.sin_addr), port);
    ::strcpy_s(remote_host.data(), remote_host.size(), inet_ntoa(saddr.sin_addr));
    found = FALSE;
    if (!saved_allow)
    {
        found = TRUE;
    }
    else
    {
        for (i = 0; i < rule1; ++i)
        {
            std::array<TCHAR, 256U> myhost{};
            int myport{ 0 };
            if (ParseDisplay(temp1[i], sizeof(temp1[i]), myhost.data(), 255, &myport))
            {
                struct sockaddr_in rule_addr;
                // Compare the resolved 32-bit addresses exactly: a string
                // prefix match would also allow e.g. 10.0.0.150 when only
                // 10.0.0.1 was listed.
                if ((local_resolve(myhost.data(), &rule_addr, nullptr) == 0) && (rule_addr.sin_addr.s_addr == saddr.sin_addr.s_addr) &&
                    (port == myport))
                {
                    found = TRUE;
                }
            }
        }
    }
    if (saved_refuse)
    {
        for (i = 0; i < rule2; ++i)
        {
            std::array<TCHAR, 256U> myhost{};
            int myport{ 0 };
            if (ParseDisplay(temp2[i], sizeof(temp2[i]), myhost.data(), 255, &myport))
            {
                struct sockaddr_in rule_addr;
                // Exact IP match (port-insensitive, as before): a refused
                // host is refused regardless of the requested port.
                if ((local_resolve(myhost.data(), &rule_addr, nullptr) == 0) && (rule_addr.sin_addr.s_addr == saddr.sin_addr.s_addr))
                {
                    found = FALSE;
                }
            }
        }
    }
    if (!found)
    {
        return SOCKET_ERROR;
    }
    s = socket(AF_INET, SOCK_STREAM, 0);
    // NAGLE
    const int one{ 1 };
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(one));

    if (connect(s, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR)
    {
        debug("connect() failed.\n");
        return SOCKET_ERROR;
    }
    return s;
}

HANDLE hmode12listener = nullptr;
int sock{ 0 };

DWORD WINAPI mode12listener(const LPVOID lpParam)
{
    std::array<TCHAR, 256U> dest_host{};
    u_short dest_port;
    struct sockaddr_in name;
    struct sockaddr_in client;
    int socklen;
    const int one{ 1 };
    int connection;
    rfbProtocolVersionMsg pv;
    std::array<TCHAR, 256U> proxyadress{};
    std::array<TCHAR, 256U> remotehost{};
    int remoteport;
    SOCKET local_in;  /* Local input */
    SOCKET local_out; /* Local output */
    SOCKET remote;
    mystruct teststruct{};
    DWORD dwThreadId;
    BOOL found2;
    char* ip_peer;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        fatal("socket() failed, errno=%d\n", socket_errno());
    }
    else
    {
        debug("socket() initialized\n");
    }

    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(one));

    name.sin_family = AF_INET;
    name.sin_port = htons(saved_portA);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr*)&name, sizeof(name)) < 0)
    {
        fatal("bind() failed, errno=%d\n", socket_errno());
    }
    else
    {
        debug("bind() succeded to port %i\n", saved_portA);
    }
    if (listen(sock, 1) < 0)
    {
        fatal("listen() failed, errno=%d\n", socket_errno());
    }
    else
    {
        debug("listen() succeded\n");
    }
    socklen = sizeof(client);

    while (notstopped)
    {
        std::memset(dest_host.data(), 0, 256);
        std::memset(proxyadress.data(), 0, 256);
        std::memset(remotehost.data(), 0, 256);
        std::memset(pv, 0, sizeof(pv));
        //		debug ("Waiting for Viewer connection...\n");
        connection = accept(sock, (struct sockaddr*)&client, &socklen);
        ::Sleep(1000);
        ip_peer = inet_ntoa(client.sin_addr);
        if (connection < 0)
        {
            debug("accept() failed, errno=%d\n", socket_errno());
            debug("Check if port is not already in use port=%i ", saved_portA);
            ::Sleep(500);
        }
        else
        {
            // NAGLE
            const int one_local{ 1 };
            setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (char*)&one_local, sizeof(one_local));
            DWORD ii{ 5000U }; // disable
            setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char*)&ii, sizeof(ii));
            setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, (char*)&ii, sizeof(ii));

            debug("accept() connection \n");
            ::sprintf_s(pv, rfbProtocolVersionFormat, rfbProtocolMajorVersion, rfbProtocolMinorVersion);
            if (WriteExact(connection, pv, sz_rfbProtocolVersionMsg) < 0)
            {
                debug("Writing protocol version error");
                closesocket(connection);
                goto end;
            }
            if (ReadExact(connection, proxyadress.data(), MAX_HOST_NAME_LEN) < 0)
            {
                debug("Reading Proxy settings error");
                closesocket(connection);
                goto end;
            }
            DWORD i{ 0U }; // disable
            setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (char*)&i, sizeof(i));
            setsockopt(connection, SOL_SOCKET, SO_SNDTIMEO, (char*)&i, sizeof(i));
            if (!ParseDisplay(proxyadress.data(), 255, remotehost.data(), 255, &remoteport))
            {
                debug("ParseDisplay failed");
                shutdown(sock, 2);
                closesocket(connection);
                continue;
            }
            ::strcpy_s(dest_host.data(), dest_host.size(), remotehost.data());
            dest_port = remoteport;
            //debug ("Server %s port %d \n", dest_host,remoteport);
            shutdown(sock, 2);
            local_in = local_out = connection;
            char* pch;
            char* comm;
            char* posn;
            pch = ::strtok_s(dest_host.data(), "+", &posn);
            comm = ::strtok_s(nullptr, ":", &posn);

            bool alsoID{ false };

            if (pch == nullptr)
            {
                closesocket(connection);
                debug("ID refused %i \n", remoteport);
                goto end;
            }

            if (saved_usecom && (std::strcmp(pch, "ID") == 0) && (comm != nullptr))
            {
                alsoID = true;
                if (comm)
                {
                    if (lookup_comment(remoteport) == nullptr)
                    {
                        closesocket(connection);
                        debug("ID-XXX refused %i , ID not in use.\n", remoteport, comm);
                        goto end;
                    }
                    if (std::strcmp(lookup_comment(remoteport), comm) != 0)
                    {
                        closesocket(connection);
                        debug("ID-XXX refused %i , incorrect %s\n", remoteport, comm);
                        goto end;
                    }
                }
                else
                {
                    closesocket(connection);
                    debug("ID- refused %i\n", remoteport);
                    goto end;
                }
            }

            //

            if ((std::strcmp(dest_host.data(), "ID") == 0) || alsoID)
            {
                found2 = TRUE;
                if (saved_refuse2)
                {
                    found2 = FALSE;
                    for (i = 0; static_cast<int>(i) < rule3; ++i)
                    {
                        if (atoi(temp3[i]) == remoteport)
                        {
                            found2 = 1;
                        }
                    }
                }
                if (!found2)
                {
                    closesocket(connection);
                    debug("ID refused %i \n", remoteport);
                }
                if (found2)
                {
                    teststruct.local_in = local_in;
                    teststruct.local_out = local_out;
                    teststruct.code = remoteport;
                    ::strcpy_s(teststruct.hostname, ip_peer);
                    Add_viewer_list(&teststruct);
                    debug("ID added %i \n", remoteport);


                    if (Find_server_list(&teststruct) != MAX_LIST + 1)
                    {
                        //found
                        //initiate connection
                        Find_viewer_list(&teststruct);
                        teststruct.local_in = local_in;
                        teststruct.local_out = local_out;
                        teststruct.remote = Servers[Find_server_list(&teststruct)].remote;
                        teststruct.server = 0;
                        teststruct.server_nummer = Find_server_list(&teststruct);
                        teststruct.viewer_nummer = Find_viewer_list(&teststruct);
                        ::CreateThread(nullptr, 0, do_repeater, (LPVOID)&teststruct, 0, &dwThreadId);
                    }
                    else
                    {
                        if (Find_viewer_list_id(&teststruct) != MAX_LIST + 1)
                        {
                            teststruct.server = 0;
                            teststruct.remote = local_in;
                            if (teststruct.waitingThread == 0)
                            {
                                ::CreateThread(nullptr, 0, do_repeater_wait, (LPVOID)&teststruct, 0, &dwThreadId);
                            }
                        }
                    }
                }
            }
            else if (saved_mode1)
            {
                remote = open_connection(dest_host.data(), dest_port);
                if (remote == SOCKET_ERROR)
                {
                    debug("Unable to connect to destination host, errno=%d\n", socket_errno());
                    closesocket(connection);
                    goto end;
                }
                debug("connected\n");
                debug("start relaying.\n");
                teststruct.local_in = local_in;
                teststruct.local_out = local_out;
                teststruct.remote = remote;
                ::CreateThread(nullptr, 0, do_repeater, (LPVOID)&teststruct, 0, &dwThreadId);
            }
            //do_repeater(local_in, local_out, remote);
        }
    end:;
    }
    notstopped = 0;
    debug("relaying done mode12listener.\n");
    return 0;
}

void Start_mode12listenerThread()
{
    notstopped = 1;
    DWORD iID;
    hmode12listener = ::CreateThread(nullptr, 0, mode12listener, nullptr, 0, &iID);
}

void Stop_mode12listenerThread()
{
    closesocket(sock);
    notstopped = 0;

    ::WaitForSingleObject(hmode12listener, 5000);
    ::CloseHandle(hmode12listener);
}
