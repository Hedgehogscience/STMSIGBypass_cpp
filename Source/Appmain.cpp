/*
    Initial author: Convery (tcn@hedgehogscience.com)
    Started: 09-01-2018
    License: MIT
    Notes:
        Provides the entrypoint for Windows and Nix.
*/

#include "Stdinclude.hpp"

namespace STMSIGBypass
{
    void Initialize()
    {
        SteamIPC IPC{};
        SteamDRM DRM{};
        SteamStart Start{};

        // Initialize the IPC and wait for data.
        InitializeIPC(IPC);
        WaitForSingleObject(IPC.Consumesemaphore, 1500);

        // Initialize the DRM struct from the data.
        InitializeDRM(DRM, (char *)IPC.Sharedfilemapping);

        // Read the startup file, even though steam just copies this to a "./STF" temp-file.
        InitializeSteamstart(Start, DRM.Startupmodule.c_str());

        // Remove the startupfile.
        std::remove(DRM.Startupmodule.c_str());

        // Acknowledge that the game has started.
        auto Event = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, DRM.Startevent.c_str());
        SetEvent(Event);
        CloseHandle(Event);

        // Notify the game that we are done.
        ReleaseSemaphore(IPC.Producesemaphore, 1, NULL);

        // Clean up the IPC.
        UnmapViewOfFile(IPC.Sharedfilemapping);
        CloseHandle(IPC.Sharedfilehandle);
        CloseHandle(IPC.Consumesemaphore);
        CloseHandle(IPC.Producesemaphore);

        // Hook the modulehandle to return the wrong modulename.
        HookModulehandle();
    }
}

// Callbacks from the bootstrapper (Bootstrapmodule_cpp).
extern "C" EXPORT_ATTR void onInitializationStart(bool Reserved)
{
    /*
        ----------------------------------------------------------------------
        This callback is called when the game is initialized, which means that
        all other libraries are loaded; but maybe not all other plugins.
        Your plugins should take this time to modify the games .text segment
        as well as initializing all your own systems.
        ----------------------------------------------------------------------
    */

    // Initialize in a different thread.
    std::thread(STMSIGBypass::Initialize).detach();
}
extern "C" EXPORT_ATTR void onInitializationDone(bool Reserved)
{
    /*
        ----------------------------------------------------------------------
        This callback is called when the platform notifies the bootstrapper,
        or at most 3 seconds after startup. This is the perfect time to start
        communicating with other plugins and modify the games .data segment.
        ----------------------------------------------------------------------
    */
}
extern "C" EXPORT_ATTR void onMessage(uint32_t MessageID, uint32_t Messagesize, const void *Messagedata)
{
    /*
        ----------------------------------------------------------------------
        This callback is called when another plugin broadcasts a message. They
        can safely be ignored, but if you want to make use of the system you
        should create a unique name for your messages. We recommend that you
        base it on your pluginname as shown below, we also recommend that you
        use the bytebuffer format for data.
        ----------------------------------------------------------------------
    */

    Bytebuffer Message{ Messagesize, Messagedata};

    // MessageID is a FNV1a_32 hash of a string.
    switch (MessageID)
    {
        case Hash::FNV1a_32(MODULENAME "::Default"):
        default: break;
    }
}

#if defined _WIN32
BOOLEAN WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved)
{
    switch (nReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            // Opt-out of further thread notifications.
            DisableThreadLibraryCalls(hDllHandle);

            // Clear the previous sessions logfile.
            Clearlog();
        }
    }

    return TRUE;
}
#else
__attribute__((constructor)) void DllMain()
{
    // Clear the previous sessions logfile.
    Clearlog();
}
#endif
