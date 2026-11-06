#include <array>
// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// For translating all messages
// find and translate all MessageBox                          -- done
// find and translate all MsgBox                              -- done
// find and translate all sendMessage with strings            -- done
// find and copy here and delete all messages string const    -- done
// find and translate all SetWindowText                       -- done
// find and translate all SetDlgItemText                      -- done

// All VNCLOG messages are NOT translated for now


#ifdef LOCALIZATION_MESSAGES

// LOCALIZATION_MESSAGES is only declare in winvnc.cpp

std::array<char, 64U> sz_ID_FAILED_INIT;    //    "Failed to initialise the socket system"
std::array<char, 64U> sz_ID_WINVNC_USAGE;   //   "UltraVNC Server Usage"
std::array<char, 64U> sz_ID_ANOTHER_INST;   //   "Another instance of UltraVNC Server is already running"
std::array<char, 64U> sz_ID_NO_EXIST_INST;  // "No existing instance of UltraVNC Server could be contacted"
std::array<char, 64U> sz_ID_UNABLE_INST;    //    "Unable to install UltraVNC Server service"
std::array<char, 128U> sz_ID_SCM_NOT_HERE;  // "The SCM could not be contacted - the UltraVNC Server service was not installed"
std::array<char, 64U> sz_ID_SERV_NOT_REG;   // "The UltraVNC Server service could not be registered"
std::array<char, 64U> sz_ID_SERV_FAIL_ST;   //   "The UltraVNC Server service failed to start"
std::array<char, 128U> sz_ID_SERV_CT_MISS;  // "The Service Control Manager could not be contacted - the UltraVNC Server service was not
                                // registered"
std::array<char, 64U> sz_ID_SERV_OLD_REG;   //   "The UltraVNC Server service is already registered"
std::array<char, 128U> sz_ID_SERVHELP_UNAB; // "WARNING:Unable to install the ServiceHelper hook\nGlobal user-specific registry settings will
                                // not be loaded"
std::array<char, 128U> sz_ID_SERV_CT_UNREG; // "The Service Control Manager could not be contacted - the UltraVNC Server service was not
                                // unregistered"
std::array<char, 64U> sz_ID_SERV_NOT_UNRG;  //  "The UltraVNC Server service could not be unregistered"
std::array<char, 64U> sz_ID_SERV_NCONTACT;  //  "The UltraVNC Server service could not be contacted"
std::array<char, 126U> sz_ID_SERVHELP_NREM; // "WARNING:The ServiceHelper hook entry could not be removed from the registry"
std::array<char, 64U> sz_ID_SERV_NOT_STOP;  //  "The UltraVNC Server service could not be stopped"
std::array<char, 64U> sz_ID_SERV_MK_UNREG;  // "The UltraVNC Server service is already marked to be unregistered"
std::array<char, 64U> sz_ID_SERV_NT_FOUND;
std::array<char, 64U> sz_ID_WINVNC_ERROR;
std::array<char, 64U> sz_ID_WINVNC_WARNIN; //  "UltraVNC Server - Warning"
std::array<char, 64U> sz_ID_PLUGIN_LOADIN; //  "Plugin Loading"
std::array<char, 200U> sz_ID_NO_PASSWD_NO_OVERRIDE_ERR;
std::array<char, 160U> sz_ID_NO_PASSWD_NO_OVERRIDE_WARN;
std::array<char, 140U> sz_ID_NO_PASSWD_NO_LOGON_WARN; // "WARNING : This machine has no default password set. UltraVNC Server will present the
                                          // Default Properties dialog now to allow one to be entered.";
std::array<char, 200U> sz_ID_NO_OVERRIDE_ERR;
std::array<char, 128U> sz_ID_NO_CURRENT_USER_ERR;       // = "The UltraVNC Server settings for the current user are unavailable at present.";
std::array<char, 128U> sz_ID_CANNOT_EDIT_DEFAULT_PREFS; // = "You do not have sufficient priviliges to edit the default local UltraVNC Server
                                            // settings.";
std::array<char, 200U> sz_ID_NO_PASSWORD_WARN;
std::array<char, 200U> sz_ID_PLUGIN_NOT_LOAD;     //  "The Plugin cannot be loaded.\n\rPlease check its integrity.";
std::array<char, 10U> sz_ID_MB1;                  //  "MB1";
std::array<char, 10U> sz_ID_WVNC;                 //  "WVNC";
std::array<char, 128U> sz_ID_AUTHAD_NOT_FO;       //  "You selected MS-Logon, but the authad.dll\nwas not found.Check you installation"
std::array<char, 64U> sz_ID_WARNING;              //  "WARNING";
std::array<char, 128U> sz_ID_AUTH_NOT_FO;         //    "You selected MS-Logon, but the auth.dll\nwas not found.Check you installation";
std::array<char, 128U> sz_ID_DESKTOP_BITBLT_ROOT; //   "vncDesktop : root device doesn't support BitBlt\n"       "UltraVNC Server cannot be used
                                      //   with this graphics device driver";
std::array<char, 128U> sz_ID_DESKTOP_BITBLT_MEM;  //   "vncDesktop : memory device doesn't support GetDIBits\n"  "UltraVNC Server cannot be used
                                      //   with this graphics device driver";
std::array<char, 128U> sz_ID_DESKTOP_PLANAR_NOTC; //   "vncDesktop : current display is PLANAR, not CHUNKY!\n"   "UltraVNC Server cannot be used
                                      //   with this graphics device driver";
std::array<char, 64U> sz_ID_FAILED_CONNECT_LISTING_VIEW; //  "Failed to connect to listening VNC Viewer";
std::array<char, 64U> sz_ID_OUTGOING_CONNECTION;         //  "Outgoing Connection";
std::array<char, 64U> sz_ID_UNABLE_PROC_MSLOGON;         //  "Unable to process MS-Logon";
std::array<char, 64U> sz_ID_RICHED32_UNLOAD;             //  "Unable to load the Rich Edit (RICHED32.DLL) control!";
std::array<char, 64U> sz_ID_RICHED32_DLL_LD;             //  "Rich Edit Dll Loading";
std::array<char, 200U> sz_ID_SERV_SUCCESS_INST;
std::array<char, 200U> sz_ID_SERV_SUCCESS_REG;
std::array<char, 64U> sz_ID_SERV_SUCCESS_UNREG; //    "The UltraVNC Server service has been unregistered";
std::array<char, 128U> sz_ID_ULTRAVNC_TEXTCHAT; // "The selected client is not an UltraVNC Viewer!\n" "It presumably does not support Text
                                    // Chat\n";
std::array<char, 64U> sz_ID_ULTRAVNC_WARNING;   //     "UltraVNC Warning";
std::array<char, 64U> sz_ID_NO_PLUGIN_DETECT;   // "No Plugin detected..."

std::array<char, 64U> sz_ID_CHAT_WITH_S_ULTRAVNC; //   "UltraVNC Server - Chat with <%s>"
std::array<char, 64U> sz_ID_CURRENT_USER_PROP;    //   "UltraVNC Server: Current User Properties"
std::array<char, 64U> sz_ID_DEFAULT_SYST_PROP;    //  "UltraVNC Server: Default Local System Properties"
std::array<char, 64U> sz_ID_AUTOREJECT_U;         //  "AutoReject:%u"
std::array<char, 64U> sz_ID_AUTOACCEPT_U;         //  "AutoAccept:%u"
std::array<char, 128U> sz_ID_CADERROR;
std::array<char, 128U> sz_ID_CADERRORFILE;
std::array<char, 128U> sz_ID_CADPERMISSION;

// New string definitions for hardcoded MessageBoxSecure strings
std::array<char, 64U> sz_ID_WINDOWS_XP_SPECIAL_BUILD;
std::array<char, 64U> sz_ID_WARNING_CAPTION;
std::array<char, 64U> sz_ID_ERROR_OS_NOT_SUPPORTED;
std::array<char, 64U> sz_ID_UNSUPPORTED_OS_CAPTION;
std::array<char, 64U> sz_ID_AUTHADMIN_DLL_NOT_FOUND;
std::array<char, 64U> sz_ID_WORKGRPDOMNT4_DLL_NOT_FOUND;
std::array<char, 64U> sz_ID_LDAPAUTH_DLL_NOT_FOUND;
std::array<char, 64U> sz_ID_LDAPAUTHNT4_DLL_NOT_FOUND;
std::array<char, 64U> sz_ID_LDAPAUTH9X_DLL_NOT_FOUND;
std::array<char, 128U> sz_ID_KILL_VIEWERS_QUESTION;
std::array<char, 128U> sz_ID_RESTART_SERVER_QUESTION;
std::array<char, 128U> sz_ID_CLOSE_SERVER_QUESTION;
std::array<char, 128U> sz_ID_REBOOT_SYSTEM_QUESTION;
std::array<char, 128U> sz_ID_FORCE_REBOOT_QUESTION;
std::array<char, 128U> sz_ID_UNINSTALL_SERVICE_QUESTION;
std::array<char, 128U> sz_ID_INSTALL_SERVICE_QUESTION;
std::array<char, 128U> sz_ID_STOP_SERVICE_QUESTION;
std::array<char, 128U> sz_ID_START_SERVICE_QUESTION;
std::array<char, 64U> sz_ID_SYSTEM_CAPTION;
std::array<char, 64U> sz_ID_SERVICE_CAPTION;
std::array<char, 256U> sz_ID_CURRENT_DRIVER_OLD;
std::array<char, 128U> sz_ID_DRIVER_NOT_FOUND_REBOOT;
std::array<char, 128U> sz_ID_DRIVER_INFO_REQUIRED_VERSION;
std::array<char, 128U> sz_ID_FAILED_OPEN_SVC_MGR;
std::array<char, 128U> sz_ID_FAILED_PERMISSION_DENIED;
std::array<char, 128U> sz_ID_FAILED_CREATE_SVC;
std::array<char, 128U> sz_ID_FAILED_SVC_NOT_INSTALLED;
std::array<char, 128U> sz_ID_FAILED_OPEN_SVC;
std::array<char, 128U> sz_ID_FAILED_QUERY_SVC_STATUS;
std::array<char, 128U> sz_ID_FAILED_DELETE_SVC;
std::array<char, 128U> sz_ID_WRONG_PASSWORD_RETRY;
std::array<char, 64U> sz_ID_ERROR_CAPTION;
std::array<char, 128U> sz_ID_UAC_DISABLED_REGISTRY;
std::array<char, 256U> sz_ID_VERIFY_SAME_FOLDER;

// Driver message string definitions
std::array<char, 64U> sz_ID_DRIVER_FOUND;
std::array<char, 64U> sz_ID_DRIVER_VERSION_OK;
std::array<char, 64U> sz_ID_DRIVER_VERSION_NOT_OK;
std::array<char, 128U> sz_ID_DRIVER_NOT_ACTIVATED;
std::array<char, 128U> sz_ID_DRIVER_ONLY_SERVICE_ADMIN;
std::array<char, 64U> sz_ID_DRIVER_ACTIVE;
std::array<char, 64U> sz_ID_DRIVER_ACCESS_OK;
std::array<char, 64U> sz_ID_DRIVER_ACCESS_DENIED;


int Load_Localization(const HINSTANCE hInstance)
{

    ::LoadString(hInstance, ID_FAILED_INIT, sz_ID_FAILED_INIT.data(), 64 - 1);
    ::LoadString(hInstance, ID_WINVNC_USAGE, sz_ID_WINVNC_USAGE.data(), 64 - 1);
    ::LoadString(hInstance, ID_ANOTHER_INST, sz_ID_ANOTHER_INST.data(), 64 - 1);
    ::LoadString(hInstance, ID_NO_EXIST_INST, sz_ID_NO_EXIST_INST.data(), 64 - 1);
    ::LoadString(hInstance, ID_UNABLE_INST, sz_ID_UNABLE_INST.data(), 64 - 1);
    ::LoadString(hInstance, ID_SCM_NOT_HERE, sz_ID_SCM_NOT_HERE.data(), 128 - 1);
    ::LoadString(hInstance, ID_SERV_NOT_REG, sz_ID_SERV_NOT_REG.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERV_FAIL_ST, sz_ID_SERV_FAIL_ST.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERV_CT_MISS, sz_ID_SERV_CT_MISS.data(), 128 - 1);
    ::LoadString(hInstance, ID_SERV_OLD_REG, sz_ID_SERV_OLD_REG.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERVHELP_UNAB, sz_ID_SERVHELP_UNAB.data(), 128 - 1);
    ::LoadString(hInstance, ID_SERV_CT_UNREG, sz_ID_SERV_CT_UNREG.data(), 128 - 1);
    ::LoadString(hInstance, ID_SERV_NOT_UNRG, sz_ID_SERV_NOT_UNRG.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERV_NCONTACT, sz_ID_SERV_NCONTACT.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERVHELP_NREM, sz_ID_SERVHELP_NREM.data(), 126 - 1);
    ::LoadString(hInstance, ID_SERV_NOT_STOP, sz_ID_SERV_NOT_STOP.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERV_MK_UNREG, sz_ID_SERV_MK_UNREG.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERV_NT_FOUND, sz_ID_SERV_NT_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, ID_WINVNC_ERROR, sz_ID_WINVNC_ERROR.data(), 64 - 1);
    ::LoadString(hInstance, ID_WINVNC_WARNIN, sz_ID_WINVNC_WARNIN.data(), 64 - 1);
    ::LoadString(hInstance, ID_PLUGIN_LOADIN, sz_ID_PLUGIN_LOADIN.data(), 64 - 1);
    ::LoadString(hInstance, ID_NO_PASSWD_NO_OVERRIDE_ERR, sz_ID_NO_PASSWD_NO_OVERRIDE_ERR.data(), 200 - 1);
    ::LoadString(hInstance, ID_NO_PASSWD_NO_OVERRIDE_WARN, sz_ID_NO_PASSWD_NO_OVERRIDE_WARN.data(), 160 - 1);
    ::LoadString(hInstance, ID_NO_PASSWD_NO_LOGON_WARN, sz_ID_NO_PASSWD_NO_LOGON_WARN.data(), 140 - 1);
    ::LoadString(hInstance, ID_NO_OVERRIDE_ERR, sz_ID_NO_OVERRIDE_ERR.data(), 200 - 1);
    ::LoadString(hInstance, ID_NO_CURRENT_USER_ERR, sz_ID_NO_CURRENT_USER_ERR.data(), 128 - 1);
    ::LoadString(hInstance, ID_CANNOT_EDIT_DEFAULT_PREFS, sz_ID_CANNOT_EDIT_DEFAULT_PREFS.data(), 128 - 1);
    ::LoadString(hInstance, ID_NO_PASSWORD_WARN, sz_ID_NO_PASSWORD_WARN.data(), 200 - 1);
    ::LoadString(hInstance, ID_PLUGIN_NOT_LOAD, sz_ID_PLUGIN_NOT_LOAD.data(), 200 - 1);
    ::LoadString(hInstance, ID_MB1, sz_ID_MB1.data(), 10 - 1);
    ::LoadString(hInstance, ID_WVNC, sz_ID_WVNC.data(), 10 - 1);
    ::LoadString(hInstance, ID_AUTHAD_NOT_FO, sz_ID_AUTHAD_NOT_FO.data(), 128 - 1);
    ::LoadString(hInstance, ID_WARNING, sz_ID_WARNING.data(), 64 - 1);
    ::LoadString(hInstance, ID_AUTH_NOT_FO, sz_ID_AUTH_NOT_FO.data(), 128 - 1);
    ::LoadString(hInstance, ID_DESKTOP_BITBLT_ROOT, sz_ID_DESKTOP_BITBLT_ROOT.data(), 128 - 1);
    ::LoadString(hInstance, ID_DESKTOP_BITBLT_MEM, sz_ID_DESKTOP_BITBLT_MEM.data(), 128 - 1);
    ::LoadString(hInstance, ID_DESKTOP_PLANAR_NOTC, sz_ID_DESKTOP_PLANAR_NOTC.data(), 128 - 1);
    ::LoadString(hInstance, ID_FAILED_CONNECT_LISTING_VIEW, sz_ID_FAILED_CONNECT_LISTING_VIEW.data(), 64 - 1);
    ::LoadString(hInstance, ID_OUTGOING_CONNECTION, sz_ID_OUTGOING_CONNECTION.data(), 64 - 1);
    ::LoadString(hInstance, ID_UNABLE_PROC_MSLOGON, sz_ID_UNABLE_PROC_MSLOGON.data(), 64 - 1);
    ::LoadString(hInstance, ID_RICHED32_UNLOAD, sz_ID_RICHED32_UNLOAD.data(), 64 - 1);
    ::LoadString(hInstance, ID_RICHED32_DLL_LD, sz_ID_RICHED32_DLL_LD.data(), 64 - 1);
    ::LoadString(hInstance, ID_SERV_SUCCESS_INST, sz_ID_SERV_SUCCESS_INST.data(), 200 - 1);
    ::LoadString(hInstance, ID_SERV_SUCCESS_REG, sz_ID_SERV_SUCCESS_REG.data(), 200 - 1);
    ::LoadString(hInstance, ID_SERV_SUCCESS_UNREG, sz_ID_SERV_SUCCESS_UNREG.data(), 64 - 1);
    ::LoadString(hInstance, ID_ULTRAVNC_TEXTCHAT, sz_ID_ULTRAVNC_TEXTCHAT.data(), 128 - 1);
    ::LoadString(hInstance, ID_ULTRAVNC_WARNING, sz_ID_ULTRAVNC_WARNING.data(), 64 - 1);
    ::LoadString(hInstance, ID_NO_PLUGIN_DETECT, sz_ID_NO_PLUGIN_DETECT.data(), 64 - 1);
    ::LoadString(hInstance, ID_CHAT_WITH_S_ULTRAVNC, sz_ID_CHAT_WITH_S_ULTRAVNC.data(), 64 - 1);
    ::LoadString(hInstance, ID_CURRENT_USER_PROP, sz_ID_CURRENT_USER_PROP.data(), 64 - 1);
    ::LoadString(hInstance, ID_DEFAULT_SYST_PROP, sz_ID_DEFAULT_SYST_PROP.data(), 64 - 1);
    ::LoadString(hInstance, ID_AUTOREJECT_U, sz_ID_AUTOREJECT_U.data(), 64 - 1);
    ::LoadString(hInstance, ID_AUTOACCEPT_U, sz_ID_AUTOACCEPT_U.data(), 64 - 1);
    ::LoadString(hInstance, ID_CADERROR, sz_ID_CADERROR.data(), 128 - 1);
    ::LoadString(hInstance, ID_CADERROR, sz_ID_CADERRORFILE.data(), 128 - 1);
    ::LoadString(hInstance, ID_CADERROR, sz_ID_CADPERMISSION.data(), 128 - 1);

    // LoadString calls for new hardcoded strings
    ::LoadString(hInstance, IDS_WINDOWS_XP_SPECIAL_BUILD, sz_ID_WINDOWS_XP_SPECIAL_BUILD.data(), 64 - 1);
    ::LoadString(hInstance, IDS_WARNING_CAPTION, sz_ID_WARNING_CAPTION.data(), 64 - 1);
    ::LoadString(hInstance, IDS_ERROR_OS_NOT_SUPPORTED, sz_ID_ERROR_OS_NOT_SUPPORTED.data(), 64 - 1);
    ::LoadString(hInstance, IDS_UNSUPPORTED_OS_CAPTION, sz_ID_UNSUPPORTED_OS_CAPTION.data(), 64 - 1);
    ::LoadString(hInstance, IDS_AUTHADMIN_DLL_NOT_FOUND, sz_ID_AUTHADMIN_DLL_NOT_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, IDS_WORKGRPDOMNT4_DLL_NOT_FOUND, sz_ID_WORKGRPDOMNT4_DLL_NOT_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, IDS_LDAPAUTH_DLL_NOT_FOUND, sz_ID_LDAPAUTH_DLL_NOT_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, IDS_LDAPAUTHNT4_DLL_NOT_FOUND, sz_ID_LDAPAUTHNT4_DLL_NOT_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, IDS_LDAPAUTH9X_DLL_NOT_FOUND, sz_ID_LDAPAUTH9X_DLL_NOT_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, IDS_KILL_VIEWERS_QUESTION, sz_ID_KILL_VIEWERS_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_RESTART_SERVER_QUESTION, sz_ID_RESTART_SERVER_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_CLOSE_SERVER_QUESTION, sz_ID_CLOSE_SERVER_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_REBOOT_SYSTEM_QUESTION, sz_ID_REBOOT_SYSTEM_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FORCE_REBOOT_QUESTION, sz_ID_FORCE_REBOOT_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_UNINSTALL_SERVICE_QUESTION, sz_ID_UNINSTALL_SERVICE_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_INSTALL_SERVICE_QUESTION, sz_ID_INSTALL_SERVICE_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_STOP_SERVICE_QUESTION, sz_ID_STOP_SERVICE_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_START_SERVICE_QUESTION, sz_ID_START_SERVICE_QUESTION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_SYSTEM_CAPTION, sz_ID_SYSTEM_CAPTION.data(), 64 - 1);
    ::LoadString(hInstance, IDS_SERVICE_CAPTION, sz_ID_SERVICE_CAPTION.data(), 64 - 1);
    ::LoadString(hInstance, IDS_CURRENT_DRIVER_OLD, sz_ID_CURRENT_DRIVER_OLD.data(), 256 - 1);
    ::LoadString(hInstance, IDS_DRIVER_NOT_FOUND_REBOOT, sz_ID_DRIVER_NOT_FOUND_REBOOT.data(), 128 - 1);
    ::LoadString(hInstance, IDS_DRIVER_INFO_REQUIRED_VERSION, sz_ID_DRIVER_INFO_REQUIRED_VERSION.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_OPEN_SVC_MGR, sz_ID_FAILED_OPEN_SVC_MGR.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_PERMISSION_DENIED, sz_ID_FAILED_PERMISSION_DENIED.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_CREATE_SVC, sz_ID_FAILED_CREATE_SVC.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_SVC_NOT_INSTALLED, sz_ID_FAILED_SVC_NOT_INSTALLED.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_OPEN_SVC, sz_ID_FAILED_OPEN_SVC.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_QUERY_SVC_STATUS, sz_ID_FAILED_QUERY_SVC_STATUS.data(), 128 - 1);
    ::LoadString(hInstance, IDS_FAILED_DELETE_SVC, sz_ID_FAILED_DELETE_SVC.data(), 128 - 1);
    ::LoadString(hInstance, IDS_WRONG_PASSWORD_RETRY, sz_ID_WRONG_PASSWORD_RETRY.data(), 128 - 1);
    ::LoadString(hInstance, IDS_ERROR_CAPTION, sz_ID_ERROR_CAPTION.data(), 64 - 1);
    ::LoadString(hInstance, IDS_UAC_DISABLED_REGISTRY, sz_ID_UAC_DISABLED_REGISTRY.data(), 128 - 1);
    ::LoadString(hInstance, IDS_VERIFY_SAME_FOLDER, sz_ID_VERIFY_SAME_FOLDER.data(), 256 - 1);

    // LoadString calls for driver messages
    ::LoadString(hInstance, IDS_DRIVER_FOUND, sz_ID_DRIVER_FOUND.data(), 64 - 1);
    ::LoadString(hInstance, IDS_DRIVER_VERSION_OK, sz_ID_DRIVER_VERSION_OK.data(), 64 - 1);
    ::LoadString(hInstance, IDS_DRIVER_VERSION_NOT_OK, sz_ID_DRIVER_VERSION_NOT_OK.data(), 64 - 1);
    ::LoadString(hInstance, IDS_DRIVER_NOT_ACTIVATED, sz_ID_DRIVER_NOT_ACTIVATED.data(), 128 - 1);
    ::LoadString(hInstance, IDS_DRIVER_ONLY_SERVICE_ADMIN, sz_ID_DRIVER_ONLY_SERVICE_ADMIN.data(), 128 - 1);
    ::LoadString(hInstance, IDS_DRIVER_ACTIVE, sz_ID_DRIVER_ACTIVE.data(), 64 - 1);
    ::LoadString(hInstance, IDS_DRIVER_ACCESS_OK, sz_ID_DRIVER_ACCESS_OK.data(), 64 - 1);
    ::LoadString(hInstance, IDS_DRIVER_ACCESS_DENIED, sz_ID_DRIVER_ACCESS_DENIED.data(), 64 - 1);


    return 0;
}

#else

extern std::array<char, 64U> sz_ID_FAILED_INIT;    //    "Failed to initialise the socket system"
extern std::array<char, 64U> sz_ID_WINVNC_USAGE;   //   "UltraVNC Server Usage"
extern std::array<char, 64U> sz_ID_ANOTHER_INST;   //   "Another instance of UltraVNC Server is already running"
extern std::array<char, 64U> sz_ID_NO_EXIST_INST;  // "No existing instance of UltraVNC Server could be contacted"
extern std::array<char, 64U> sz_ID_UNABLE_INST;    //    "Unable to install UltraVNC Server service"
extern std::array<char, 128U> sz_ID_SCM_NOT_HERE;  // "The SCM could not be contacted - the UltraVNC Server service was not installed"
extern std::array<char, 64U> sz_ID_SERV_NOT_REG;   // "The UltraVNC Server service could not be registered"
extern std::array<char, 64U> sz_ID_SERV_FAIL_ST;   //   "The UltraVNC Server service failed to start"
extern std::array<char, 128U> sz_ID_SERV_CT_MISS;  // "The Service Control Manager could not be contacted - the UltraVNC Server service was not
                                       // registered"
extern std::array<char, 64U> sz_ID_SERV_OLD_REG;   //   "The UltraVNC Server service is already registered"
extern std::array<char, 128U> sz_ID_SERVHELP_UNAB; // "WARNING:Unable to install the ServiceHelper hook\nGlobal user-specific registry settings
                                       // will not be loaded"
extern std::array<char, 128U> sz_ID_SERV_CT_UNREG; // "The Service Control Manager could not be contacted - the UltraVNC Server service was not
                                       // unregistered"
extern std::array<char, 64U> sz_ID_SERV_NOT_UNRG;  //  "The UltraVNC Server service could not be unregistered"
extern std::array<char, 64U> sz_ID_SERV_NCONTACT;  //  "The UltraVNC Server service could not be contacted"
extern std::array<char, 126U> sz_ID_SERVHELP_NREM; // "WARNING:The ServiceHelper hook entry could not be removed from the registry"
extern std::array<char, 64U> sz_ID_SERV_NOT_STOP;  //  "The UltraVNC Server service could not be stopped"
extern std::array<char, 64U> sz_ID_SERV_MK_UNREG;  // "The UltraVNC Server service is already marked to be unregistered"
extern std::array<char, 64U> sz_ID_SERV_NT_FOUND;
extern std::array<char, 64U> sz_ID_WINVNC_ERROR;
extern std::array<char, 64U> sz_ID_WINVNC_WARNIN; //  "UltraVNC Server - Warning"
extern std::array<char, 64U> sz_ID_PLUGIN_LOADIN; //  "Plugin Loading"
extern std::array<char, 200U> sz_ID_NO_PASSWD_NO_OVERRIDE_ERR;
extern std::array<char, 160U> sz_ID_NO_PASSWD_NO_OVERRIDE_WARN;
extern std::array<char, 140U> sz_ID_NO_PASSWD_NO_LOGON_WARN; // "WARNING : This machine has no default password set. UltraVNC Server will
                                                 // present the Default Properties dialog now to allow one to be entered.";
extern std::array<char, 200U> sz_ID_NO_OVERRIDE_ERR;
extern std::array<char, 128U> sz_ID_NO_CURRENT_USER_ERR; // = "The UltraVNC Server settings for the current user are unavailable at present.";
extern std::array<char, 128U> sz_ID_CANNOT_EDIT_DEFAULT_PREFS; // = "You do not have sufficient priviliges to edit the default local UltraVNC
                                                   // Server settings.";
extern std::array<char, 200U> sz_ID_NO_PASSWORD_WARN;
extern std::array<char, 200U> sz_ID_PLUGIN_NOT_LOAD;     //  "The Plugin cannot be loaded.\n\rPlease check its integrity.";
extern std::array<char, 10U> sz_ID_MB1;                  //  "MB1";
extern std::array<char, 10U> sz_ID_WVNC;                 //  "WVNC";
extern std::array<char, 128U> sz_ID_AUTHAD_NOT_FO;       //  "You selected MS-Logon, but the authad.dll\nwas not found.Check you installation"
extern std::array<char, 64U> sz_ID_WARNING;              //  "WARNING";
extern std::array<char, 128U> sz_ID_AUTH_NOT_FO;         //    "You selected MS-Logon, but the auth.dll\nwas not found.Check you installation";
extern std::array<char, 128U> sz_ID_DESKTOP_BITBLT_ROOT; //   "vncDesktop : root device doesn't support BitBlt\n"       "UltraVNC Server cannot
                                             //   be used with this graphics device driver";
extern std::array<char, 128U> sz_ID_DESKTOP_BITBLT_MEM;  //   "vncDesktop : memory device doesn't support GetDIBits\n"  "UltraVNC Server cannot
                                            //   be used with this graphics device driver";
extern std::array<char, 128U> sz_ID_DESKTOP_PLANAR_NOTC; //   "vncDesktop : current display is PLANAR, not CHUNKY!\n"   "UltraVNC Server cannot
                                             //   be used with this graphics device driver";
extern std::array<char, 64U> sz_ID_FAILED_CONNECT_LISTING_VIEW; //  "Failed to connect to listening VNC Viewer";
extern std::array<char, 64U> sz_ID_OUTGOING_CONNECTION;         //  "Outgoing Connection";
extern std::array<char, 64U> sz_ID_UNABLE_PROC_MSLOGON;         //  "Unable to process MS-Logon";
extern std::array<char, 64U> sz_ID_RICHED32_UNLOAD;             //  "Unable to load the Rich Edit (RICHED32.DLL) control!";
extern std::array<char, 64U> sz_ID_RICHED32_DLL_LD;             //  "Rich Edit Dll Loading";
extern std::array<char, 200U> sz_ID_SERV_SUCCESS_INST;
extern std::array<char, 200U> sz_ID_SERV_SUCCESS_REG;
extern std::array<char, 64U> sz_ID_SERV_SUCCESS_UNREG; //    "The UltraVNC Server service has been unregistered";
extern std::array<char, 128U> sz_ID_ULTRAVNC_TEXTCHAT; // "The selected client is not an UltraVNC Viewer!\n" "It presumably does not support
                                           // Text Chat\n";
extern std::array<char, 64U> sz_ID_ULTRAVNC_WARNING; //     "UltraVNC Warning";

extern std::array<char, 64U> sz_ID_NO_PLUGIN_DETECT; // "No Plugin detected..."

extern std::array<char, 64U> sz_ID_CHAT_WITH_S_ULTRAVNC; //   "UltraVNC Server - Chat with <%s>"
extern std::array<char, 64U> sz_ID_CURRENT_USER_PROP;    //   "UltraVNC Server: Current User Properties"
extern std::array<char, 64U> sz_ID_DEFAULT_SYST_PROP;    //  "UltraVNC Server: Default Local System Properties"
extern std::array<char, 64U> sz_ID_AUTOREJECT_U;         //  "AutoReject:%u"
extern std::array<char, 64U> sz_ID_AUTOACCEPT_U;         //  "AutoReject:%u"
extern std::array<char, 128U> sz_ID_CADERROR;
extern std::array<char, 128U> sz_ID_CADERRORFILE;
extern std::array<char, 128U> sz_ID_CADPERMISSION;

// New string declarations for hardcoded MessageBoxSecure strings
extern std::array<char, 64U> sz_ID_WINDOWS_XP_SPECIAL_BUILD;
extern std::array<char, 64U> sz_ID_WARNING_CAPTION;
extern std::array<char, 64U> sz_ID_ERROR_OS_NOT_SUPPORTED;
extern std::array<char, 64U> sz_ID_UNSUPPORTED_OS_CAPTION;
extern std::array<char, 64U> sz_ID_AUTHADMIN_DLL_NOT_FOUND;
extern std::array<char, 64U> sz_ID_WORKGRPDOMNT4_DLL_NOT_FOUND;
extern std::array<char, 64U> sz_ID_LDAPAUTH_DLL_NOT_FOUND;
extern std::array<char, 64U> sz_ID_LDAPAUTHNT4_DLL_NOT_FOUND;
extern std::array<char, 64U> sz_ID_LDAPAUTH9X_DLL_NOT_FOUND;
extern std::array<char, 128U> sz_ID_KILL_VIEWERS_QUESTION;
extern std::array<char, 128U> sz_ID_RESTART_SERVER_QUESTION;
extern std::array<char, 128U> sz_ID_CLOSE_SERVER_QUESTION;
extern std::array<char, 128U> sz_ID_REBOOT_SYSTEM_QUESTION;
extern std::array<char, 128U> sz_ID_FORCE_REBOOT_QUESTION;
extern std::array<char, 128U> sz_ID_UNINSTALL_SERVICE_QUESTION;
extern std::array<char, 128U> sz_ID_INSTALL_SERVICE_QUESTION;
extern std::array<char, 128U> sz_ID_STOP_SERVICE_QUESTION;
extern std::array<char, 128U> sz_ID_START_SERVICE_QUESTION;
extern std::array<char, 64U> sz_ID_SYSTEM_CAPTION;
extern std::array<char, 64U> sz_ID_SERVICE_CAPTION;
extern std::array<char, 256U> sz_ID_CURRENT_DRIVER_OLD;
extern std::array<char, 128U> sz_ID_DRIVER_NOT_FOUND_REBOOT;
extern std::array<char, 128U> sz_ID_DRIVER_INFO_REQUIRED_VERSION;
extern std::array<char, 128U> sz_ID_FAILED_OPEN_SVC_MGR;
extern std::array<char, 128U> sz_ID_FAILED_PERMISSION_DENIED;
extern std::array<char, 128U> sz_ID_FAILED_CREATE_SVC;
extern std::array<char, 128U> sz_ID_FAILED_SVC_NOT_INSTALLED;
extern std::array<char, 128U> sz_ID_FAILED_OPEN_SVC;
extern std::array<char, 128U> sz_ID_FAILED_QUERY_SVC_STATUS;
extern std::array<char, 128U> sz_ID_FAILED_DELETE_SVC;
extern std::array<char, 128U> sz_ID_WRONG_PASSWORD_RETRY;
extern std::array<char, 64U> sz_ID_ERROR_CAPTION;
extern std::array<char, 128U> sz_ID_UAC_DISABLED_REGISTRY;
extern std::array<char, 256U> sz_ID_VERIFY_SAME_FOLDER;

// Driver message string declarations
extern std::array<char, 64U> sz_ID_DRIVER_FOUND;
extern std::array<char, 64U> sz_ID_DRIVER_VERSION_OK;
extern std::array<char, 64U> sz_ID_DRIVER_VERSION_NOT_OK;
extern std::array<char, 128U> sz_ID_DRIVER_NOT_ACTIVATED;
extern std::array<char, 128U> sz_ID_DRIVER_ONLY_SERVICE_ADMIN;
extern std::array<char, 64U> sz_ID_DRIVER_ACTIVE;
extern std::array<char, 64U> sz_ID_DRIVER_ACCESS_OK;
extern std::array<char, 64U> sz_ID_DRIVER_ACCESS_DENIED;

// Function to reload localization strings
extern int Load_Localization(HINSTANCE hInstance);

#endif
