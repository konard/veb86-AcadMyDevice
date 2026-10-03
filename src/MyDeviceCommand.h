// MyDeviceCommand.h — команда AutoCAD MYDEVICE.
#pragma once

// Имя группы команд приложения.
#define MYDEVICE_COMMAND_GROUP _T("MYDEVICE_COMMANDS")

// Команда MYDEVICE: запрашивает точку вставки и создаёт MyDevice в пространстве модели.
void MyDeviceCommand();
