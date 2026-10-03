// MyDeviceCommand.cpp — реализация команды MYDEVICE.
#include "StdAfx.h"
#include "MyDevice.h"
#include "MyDeviceCommand.h"

namespace
{
    // Добавляет объект в пространство модели базы данных.
    // При успехе объект закрывается, иначе — удаляется.
    Acad::ErrorStatus appendToModelSpace(AcDbDatabase* pDb, AcDbEntity* pEntity,
                                         AcDbObjectId& entityId)
    {
        AcDbBlockTable* pBlockTable = nullptr;
        Acad::ErrorStatus es = pDb->getBlockTable(pBlockTable, AcDb::kForRead);
        if (es != Acad::eOk)
        {
            delete pEntity;
            return es;
        }

        AcDbBlockTableRecord* pModelSpace = nullptr;
        es = pBlockTable->getAt(ACDB_MODEL_SPACE, pModelSpace, AcDb::kForWrite);
        pBlockTable->close();
        if (es != Acad::eOk)
        {
            delete pEntity;
            return es;
        }

        es = pModelSpace->appendAcDbEntity(entityId, pEntity);
        pModelSpace->close();

        if (es == Acad::eOk)
            pEntity->close();
        else
            delete pEntity;
        return es;
    }
}

void MyDeviceCommand()
{
    ads_point pickedPoint;
    if (acedGetPoint(nullptr, _T("\nУкажите точку вставки MyDevice: "), pickedPoint) != RTNORM)
        return;

    // acedGetPoint() возвращает точку в ПСК — переводим её и оси ПСК в МСК.
    AcGeMatrix3d ucsToWcs;
    acedGetCurrentUCS(ucsToWcs);

    AcGePoint3d position(pickedPoint[X], pickedPoint[Y], pickedPoint[Z]);
    position.transformBy(ucsToWcs);

    AcGeVector3d xDirection = AcGeVector3d::kXAxis;
    xDirection.transformBy(ucsToWcs);
    AcGeVector3d normal = AcGeVector3d::kZAxis;
    normal.transformBy(ucsToWcs);

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();

    MyDevice* pDevice = new MyDevice(position);
    pDevice->setDatabaseDefaults(pDb);
    pDevice->setOrientation(xDirection, normal);

    AcDbObjectId deviceId;
    const Acad::ErrorStatus es = appendToModelSpace(pDb, pDevice, deviceId);
    if (es != Acad::eOk)
        acutPrintf(_T("\nНе удалось создать MyDevice: %s"), acadErrorStatusText(es));
}
