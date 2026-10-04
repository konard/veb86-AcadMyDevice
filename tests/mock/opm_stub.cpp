// opm_stub.cpp — заглушка MyDeviceOPM.cpp для тестов.
#include "StdAfx.h"
#include "MyDevice.h"
#include "MyDeviceOPM.h"
#include "opm_stub.h"

namespace opmStub
{
    int registerCalls = 0;
    int unregisterCalls = 0;
    bool registered = false;
    bool classReadyAtRegister = false;
    bool classReadyAtUnregister = false;
}

bool registerMyDeviceProperties()
{
    opmStub::registerCalls++;
    opmStub::registered = true;
    // Класс должен быть в словаре ACRX (указатель desc() после удаления класса висячий).
    opmStub::classReadyAtRegister = mockFindClass(L"MyDevice") != nullptr;
    return true;
}

void unregisterMyDeviceProperties()
{
    opmStub::unregisterCalls++;
    opmStub::registered = false;
    opmStub::classReadyAtUnregister = mockFindClass(L"MyDevice") != nullptr;
}
