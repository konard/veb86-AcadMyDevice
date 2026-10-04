// acrxEntryPoint.cpp — точка входа ObjectARX-приложения MyDevice.
#include "StdAfx.h"
#include "MyDevice.h"
#include "MyDeviceCommand.h"
#include "MyDeviceOPM.h"

// acrxGetApiVersion() реализована в rxapi.lib; экспортируем её без .def-файла.
#if defined(_MSC_VER)
#if defined(_WIN64)
#pragma comment(linker, "/export:acrxGetApiVersion,PRIVATE")
#else
#pragma comment(linker, "/export:_acrxGetApiVersion,PRIVATE")
#endif
#endif

extern "C" AcRx::AppRetCode __declspec(dllexport)
acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt)
{
    switch (msg)
    {
    case AcRx::kInitAppMsg:
        // Приложение не разблокируется (acrxUnlockApplication не вызывается):
        // выгрузка класса при наличии объектов MyDevice в открытых чертежах небезопасна.
        acrxRegisterAppMDIAware(pkt);

        MyDevice::rxInit();
        acrxBuildClassHierarchy();

        // Категории «Text 1» и «Text 2» в палитре свойств.
        registerMyDeviceProperties();

        acedRegCmds->addCommand(MYDEVICE_COMMAND_GROUP,
                                _T("MYDEVICE"), _T("MYDEVICE"),
                                ACRX_CMD_MODAL, MyDeviceCommand);

        acutPrintf(_T("\nMyDevice загружен. Команда: MYDEVICE."));
        break;

    case AcRx::kUnloadAppMsg:
        acedRegCmds->removeGroup(MYDEVICE_COMMAND_GROUP);
        unregisterMyDeviceProperties();
        deleteAcRxClass(MyDevice::desc());
        acrxBuildClassHierarchy();
        break;

    default:
        break;
    }
    return AcRx::kRetOK;
}
