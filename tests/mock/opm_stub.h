// opm_stub.h — состояние заглушки регистрации свойств палитры (opm_stub.cpp).
//
// MyDeviceOPM.cpp реализует COM-интерфейсы AutoCAD и собирается только MSVC;
// в тестах его заменяет заглушка, которая проверяет, когда точка входа
// регистрирует и удаляет свойства.
#pragma once

namespace opmStub
{
    extern int registerCalls;
    extern int unregisterCalls;
    // Свойства сейчас зарегистрированы.
    extern bool registered;
    // Был ли класс MyDevice зарегистрирован в момент вызова.
    extern bool classReadyAtRegister;
    extern bool classReadyAtUnregister;
}
