// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#pragma once
#include "stdhdrs.h"

#include <array>

class RulesListView
{
private:
    int index;
    void addItem(char* caption);
    void selectItem(int index);
    HWND hListView = nullptr;

    bool isValidNumber(const char* str);
    bool isValidIPAddress(const char* ip);
    bool isEdit;
    std::array<char, 256U> oldrule;
    int oldindex;

public:
    explicit RulesListView();
    ~RulesListView();
    void init(HWND hListView);
    void removeSelectedItem();
    void moveUp();
    void moveDown();
    void updateItem(int index, char* caption);
    void add();
    void edit();
    bool onInitDialog(HWND hwnd);
    bool onCommand(HWND hwnd, UINT cID, UINT nID);
    void onOkButtonClick(HWND hwnd);
    char* getAuthHost();
};
