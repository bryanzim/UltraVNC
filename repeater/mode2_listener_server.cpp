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

HANDLE hserver_listen = nullptr;
int sock_server_listen{ 0 };

DWORD WINAPI server_listen([[maybe_unused]] const LPVOID lpParam)
{
    std::array<TCHAR, 256U> dest_host;
    u_short dest_port;
    SOCKET local_in = INVALID_SOCKET;  /* Local input */
    SOCKET local_out = INVALID_SOCKET; /* Local output */

    int connection;
    struct sockaddr_in name;
    struct sockaddr_in client;
    int socklen;
    BOOL found2;
    DWORD dwThreadId;
    std::array<TCHAR, 256U> proxyadress;
    std::array<TCHAR, 256U> remotehost;
    int remoteport;
    mystruct teststruct;
    char* ip_peer;



    ////////////
    sock_server_listen = static_cast<int>(socket(PF_INET, SOCK_STREAM, 0));
    if (sock_server_listen < 0)
    {
        fatal("socket() failed, errno=%d\n", socket_errno());
    }
    else
    {
        debug("socket() initialized\n");
    }
    // NAGLE
    const int one{ 1 };
    setsockopt(static_cast<SOCKET>(sock_server_listen), IPPROTO_TCP, TCP_NODELAY, const_cast<char*>(reinterpret_cast<const char *>(&one)), sizeof(one));

    name.sin_family = AF_INET;
    name.sin_port = htons(static_cast<u_short>(saved_portB));
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(static_cast<SOCKET>(sock_server_listen), reinterpret_cast<struct sockaddr*>(&name), sizeof(name)) < 0)
    {
        fatal("bind() failed, errno=%d\n", socket_errno());
    }
    else
    {
        debug("bind() succeded to port %i\n", saved_portB);
    }
    if (listen(static_cast<SOCKET>(sock_server_listen), 1) < 0)
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

        connection = static_cast<int>(accept(static_cast<SOCKET>(sock_server_listen), reinterpret_cast<struct sockaddr*>(&client), &socklen));
        ::Sleep(1000);
        ip_peer = inet_ntoa(client.sin_addr);

        if (connection < 0)
        {
            debug("accept() failed, errno=%d\n", socket_errno());
            debug("Check if port is not already in use port=%i ", saved_portB);
            ::Sleep(500);
        }
        else
        {
            // NAGLE
            const int one_local{ 1 };
            setsockopt(static_cast<SOCKET>(connection), IPPROTO_TCP, TCP_NODELAY, const_cast<char*>(reinterpret_cast<const char *>(&one_local)), sizeof(one_local));
            setsockopt(static_cast<SOCKET>(connection), SOL_SOCKET, SO_KEEPALIVE, const_cast<char*>(reinterpret_cast<const char *>(&one_local)), sizeof(one_local));
            int ii{ 5000 }; // disable
            setsockopt(static_cast<SOCKET>(connection), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&ii), sizeof(ii));
            setsockopt(static_cast<SOCKET>(connection), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&ii), sizeof(ii));

            if (ReadExact(connection, proxyadress.data(), MAX_HOST_NAME_LEN) < 0)
            {
                debug("Reading Proxy settings error");
                closesocket(static_cast<SOCKET>(connection));
                goto end;
            }
            int i{ 0 }; // disable
            setsockopt(static_cast<SOCKET>(connection), SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&i), sizeof(i));
            setsockopt(static_cast<SOCKET>(connection), SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&i), sizeof(i));
            if (!ParseDisplay(proxyadress.data(), 255, remotehost.data(), 255, &remoteport))
            {
                shutdown(static_cast<SOCKET>(sock_server_listen), 2);
                closesocket(static_cast<SOCKET>(connection));
                continue;
            }

            ::strcpy_s(dest_host.data(), dest_host.size(), remotehost.data());
            dest_port = static_cast<u_short>(remoteport);
            shutdown(static_cast<SOCKET>(sock_server_listen), 2);
            local_in = local_out = static_cast<SOCKET>(connection);
            if (std::strcmp(dest_host.data(), "ID") == 0)
            {
                found2 = TRUE;
                if (saved_refuse2)
                {
                    found2 = FALSE;
                    for (i = 0; i < rule3; ++i)
                    {
                        if (atoi(temp3[i]) == remoteport)
                        {
                            found2 = true;
                        }
                    }
                }
                if (saved_usecom)
                {
                    if (lookup_comment(static_cast<ULONG>(remoteport)) == nullptr)
                    {
                        found2 = FALSE;
                    }
                }

                if (!found2)
                {
                    debug("ID refused %i \n", remoteport);
                    closesocket(static_cast<SOCKET>(connection));
                }
                if (found2)
                {
                    teststruct.remote = static_cast<SOCKET>(connection);
                    teststruct.code = static_cast<ULONG>(remoteport);
                    ::strcpy_s(teststruct.hostname, ip_peer);

                    Add_server_list(&teststruct);

                    if (Find_viewer_list(&teststruct) != MAX_LIST + 1)
                    {
                        debug("ID viewer found %i \n", remoteport);
                        //initiate connection
                        Find_server_list(&teststruct);
                        teststruct.local_in = Viewers[Find_viewer_list(&teststruct)].local_in;
                        teststruct.local_out = Viewers[Find_viewer_list(&teststruct)].local_out;
                        teststruct.remote = static_cast<SOCKET>(connection);
                        teststruct.viewer_nummer = static_cast<int>(Find_viewer_list(&teststruct));
                        teststruct.server_nummer = static_cast<int>(Find_server_list(&teststruct));
                        teststruct.server = 1;
                        ::CreateThread(nullptr, 0, do_repeater, reinterpret_cast<LPVOID>(&teststruct), 0, &dwThreadId);
                    }
                    else
                    {
                        if (Find_server_list_id(&teststruct) != MAX_LIST + 1)
                        {
                            teststruct.server = 1;
                            if (teststruct.waitingThread == 0)
                            {
                                ::CreateThread(nullptr, 0, do_repeater_wait, reinterpret_cast<LPVOID>(&teststruct), 0, &dwThreadId);
                            }
                        }
                    }
                }
            }
        }
    end:;
    }
    notstopped = false;
    debug("relaying done. server_listen \n");
    closesocket(local_out);

    closesocket(local_in);
    return 0;
}

void Start_server_listenThread()
{
    notstopped = 1;
    DWORD iID;
    hserver_listen = ::CreateThread(nullptr, 0, server_listen, nullptr, 0, &iID);
}

void Stop_server_listenThread()
{
    closesocket(static_cast<SOCKET>(sock_server_listen));
    notstopped = 0;
    if (hserver_listen)
    {
        ::WaitForSingleObject(hserver_listen, 5000);
        ::CloseHandle(hserver_listen);
        hserver_listen = nullptr;
    }
}
