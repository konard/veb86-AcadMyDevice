// MyDeviceTests.cpp — модульные тесты MyDevice на имитации ObjectARX.
//
// Проверяются требования этапа 1:
//  * регистрация класса MyDevice (наследник AcDbEntity) и команды MYDEVICE;
//  * значения по умолчанию Text1 = "TEXT1", Text2 = "TEXT2";
//  * отрисовка прямоугольника 100x50 и двух текстов самим объектом
//    (без создания AcDbText/AcDbMText);
//  * сохранение и восстановление DWG и DXF;
//  * команда MYDEVICE создаёт объект в пространстве модели в указанной точке.
//
// и этапа 2:
//  * у каждого текста есть содержимое, высота, локальное положение X/Y, слой и стиль;
//  * тексты рисуются своим стилем, на своём слое и вместе с объектом преобразуются;
//  * все свойства сохраняются в DWG/DXF (версия 2), данные версии 1 читаются;
//  * палитра свойств: категории «Text 1»/«Text 2» по шесть свойств, чтение и запись
//    значений, создание отсутствующего слоя, выбор только существующего стиля.
#include "StdAfx.h"
#include "MyDevice.h"
#include "MyDeviceCommand.h"
#include "MyDeviceProperties.h"
#include "mock_filers.h"
#include "opm_stub.h"
#include "test_framework.h"

#include <limits>

extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg, void* pkt);

namespace
{
    const double kTol = 1.0e-9;
    const double kPi = 3.14159265358979323846;

    void checkPoint(const char* file, int line, const AcGePoint3d& actual, const AcGePoint3d& expected)
    {
        if (!actual.isEqualTo(expected, 1.0e-7))
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "point (%g, %g, %g) != (%g, %g, %g)",
                          actual.x, actual.y, actual.z, expected.x, expected.y, expected.z);
            testing::fail(file, line, buf);
        }
    }

    void checkVector(const char* file, int line, const AcGeVector3d& actual, const AcGeVector3d& expected)
    {
        if (!actual.isEqualTo(expected, 1.0e-7))
        {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "vector (%g, %g, %g) != (%g, %g, %g)",
                          actual.x, actual.y, actual.z, expected.x, expected.y, expected.z);
            testing::fail(file, line, buf);
        }
    }

    // Объект с непустыми значениями всех полей — для проверок сохранения.
    MyDevice* makeCustomDevice()
    {
        MyDevice* pDevice = new MyDevice(AcGePoint3d(12.5, -7.25, 3.0));
        pDevice->setOrientation(AcGeVector3d(1.0, 1.0, 0.0), AcGeVector3d::kZAxis);
        pDevice->setScale(2.5);
        pDevice->setText1(L"Насос №1");
        pDevice->setText2(L"P = 10 кВт; \"кавычки\"");
        pDevice->setLayer(L"DEVICES");
        return pDevice;
    }

    void checkSameDevice(const char* file, int line, const MyDevice& actual, const MyDevice& expected)
    {
        checkPoint(file, line, actual.position(), expected.position());
        checkVector(file, line, actual.xDirection(), expected.xDirection());
        checkVector(file, line, actual.normal(), expected.normal());
        if (std::fabs(actual.scale() - expected.scale()) > kTol)
            testing::fail(file, line, "scale differs");
        if (!(actual.text1() == expected.text1()))
            testing::fail(file, line, "Text1 differs: " + testing::narrow(actual.text1().kACharPtr()));
        if (!(actual.text2() == expected.text2()))
            testing::fail(file, line, "Text2 differs: " + testing::narrow(actual.text2().kACharPtr()));
        AcString actualLayer, expectedLayer;
        actual.layer(actualLayer);
        expected.layer(expectedLayer);
        if (actualLayer != expectedLayer)
            testing::fail(file, line, "layer differs");
    }

    AcDbBlockTableRecord& modelSpace()
    {
        return acdbHostApplicationServices()->workingDatabase()->modelSpace;
    }
}

#define CHECK_POINT(a, e) checkPoint(__FILE__, __LINE__, (a), (e))
#define CHECK_VECTOR(a, e) checkVector(__FILE__, __LINE__, (a), (e))
#define CHECK_SAME_DEVICE(a, e) checkSameDevice(__FILE__, __LINE__, (a), (e))

// ---------------------------------------------------------------------------
// Регистрация приложения
// ---------------------------------------------------------------------------

TEST(EntryPoint_RegistersClassAndCommand_AndUnloadRemovesThem)
{
    // main() уже вызвал kInitAppMsg.
    AcRxClass* pClass = mockFindClass(L"MyDevice");
    CHECK(pClass != nullptr);
    if (!pClass)
        return;
    CHECK(pClass == MyDevice::desc());
    CHECK(pClass->myParent() == AcDbEntity::desc());
    CHECK_WSTR(pClass->dxfName(), L"MYDEVICE");
    CHECK(std::wstring(pClass->appName()).find(L"MyDeviceApp") == 0);
    CHECK_EQ(pClass->proxyFlags(), static_cast<int>(AcDbProxyEntity::kNoOperation));
    int dwgVer = 0, maintVer = 0;
    pClass->getClassVersion(dwgVer, maintVer);
    CHECK_EQ(dwgVer, static_cast<int>(AcDb::kDHL_CURRENT));
    CHECK_EQ(maintVer, static_cast<int>(AcDb::kMReleaseCurrent));

    const AcEdCommandStack::Command* pCmd = acedRegCmds->lookupGlobalCmd(L"MYDEVICE");
    CHECK(pCmd != nullptr);
    if (pCmd)
    {
        CHECK_WSTR(pCmd->group, MYDEVICE_COMMAND_GROUP);
        CHECK_WSTR(pCmd->localName, L"MYDEVICE");
        CHECK(pCmd->function == &MyDeviceCommand);
    }
    // Приложение не разблокируется: выгрузка при живых объектах небезопасна.
    CHECK_EQ(mock::unlockCalls, 0);
    CHECK(mock::mdiAwareCalls >= 1);
    // Свойства палитры регистрируются после регистрации класса MyDevice.
    CHECK(opmStub::registered);
    CHECK(opmStub::classReadyAtRegister);
    const int registerCalls = opmStub::registerCalls;
    const int unregisterCalls = opmStub::unregisterCalls;

    CHECK_EQ(acrxEntryPoint(AcRx::kUnloadAppMsg, nullptr), AcRx::kRetOK);
    CHECK(mockFindClass(L"MyDevice") == nullptr);
    CHECK(acedRegCmds->lookupGlobalCmd(L"MYDEVICE") == nullptr);
    // ...и удаляются до удаления класса.
    CHECK_EQ(opmStub::unregisterCalls, unregisterCalls + 1);
    CHECK(!opmStub::registered);
    CHECK(opmStub::classReadyAtUnregister);

    // Возвращаем приложение в загруженное состояние для остальных тестов.
    CHECK_EQ(acrxEntryPoint(AcRx::kInitAppMsg, nullptr), AcRx::kRetOK);
    CHECK(MyDevice::desc() != nullptr);
    CHECK(acedRegCmds->lookupGlobalCmd(L"MYDEVICE") != nullptr);
    CHECK_EQ(opmStub::registerCalls, registerCalls + 1);
    CHECK(opmStub::registered);
}

TEST(RuntimeClass_CreatesMyDeviceInstances)
{
    // Так AutoCAD создаёт объекты при открытии чертежа.
    AcRxObject* pObj = MyDevice::desc()->create();
    MyDevice* pDevice = MyDevice::cast(pObj);
    CHECK(pDevice != nullptr);
    CHECK(pObj->isKindOf(AcDbEntity::desc()));
    CHECK(pObj->isA() == MyDevice::desc());
    delete pObj;
}

// ---------------------------------------------------------------------------
// Поля и значения по умолчанию
// ---------------------------------------------------------------------------

TEST(Defaults_TextsAreTEXT1AndTEXT2)
{
    MyDevice device;
    CHECK_WSTR(device.text1().kACharPtr(), L"TEXT1");
    CHECK_WSTR(device.text2().kACharPtr(), L"TEXT2");
    CHECK_POINT(device.position(), AcGePoint3d::kOrigin);
    CHECK_VECTOR(device.xDirection(), AcGeVector3d::kXAxis);
    CHECK_VECTOR(device.normal(), AcGeVector3d::kZAxis);
    CHECK_NEAR(device.scale(), 1.0, kTol);

    MyDevice placed(AcGePoint3d(1.0, 2.0, 3.0));
    CHECK_POINT(placed.position(), AcGePoint3d(1.0, 2.0, 3.0));
    CHECK_WSTR(placed.text1().kACharPtr(), L"TEXT1");
    CHECK_WSTR(placed.text2().kACharPtr(), L"TEXT2");
}

TEST(Setters_ChangeFields_AndRejectInvalidValues)
{
    MyDevice device;
    CHECK_EQ(device.setText1(L"A"), Acad::eOk);
    CHECK_EQ(device.setText2(L"B"), Acad::eOk);
    CHECK_WSTR(device.text1().kACharPtr(), L"A");
    CHECK_WSTR(device.text2().kACharPtr(), L"B");

    CHECK_EQ(device.setPosition(AcGePoint3d(5.0, 6.0, 7.0)), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(5.0, 6.0, 7.0));

    // Ориентация нормализуется и делается ортогональной.
    CHECK_EQ(device.setOrientation(AcGeVector3d(2.0, 0.0, 1.0), AcGeVector3d(0.0, 0.0, 3.0)), Acad::eOk);
    CHECK_VECTOR(device.xDirection(), AcGeVector3d::kXAxis);
    CHECK_VECTOR(device.normal(), AcGeVector3d::kZAxis);

    CHECK_EQ(device.setOrientation(AcGeVector3d::kZAxis, AcGeVector3d::kZAxis), Acad::eInvalidInput);
    CHECK_EQ(device.setOrientation(AcGeVector3d::kXAxis, AcGeVector3d()), Acad::eInvalidInput);
    CHECK_EQ(device.setScale(0.0), Acad::eInvalidInput);
    CHECK_EQ(device.setScale(-1.0), Acad::eInvalidInput);
    CHECK_NEAR(device.scale(), 1.0, kTol);
}

// ---------------------------------------------------------------------------
// Отрисовка
// ---------------------------------------------------------------------------

TEST(WorldDraw_DrawsClosedRectangle100x50)
{
    MyDevice device(AcGePoint3d(10.0, 20.0, 0.0));
    RecordingWorldDraw wd;
    CHECK(device.worldDraw(&wd));

    CHECK_EQ(wd.polylines.size(), static_cast<size_t>(1));
    if (wd.polylines.size() != 1)
        return;
    const RecordingWorldDraw::Polyline& pl = wd.polylines[0];
    CHECK_EQ(pl.points.size(), static_cast<size_t>(5));
    if (pl.points.size() != 5)
        return;
    CHECK_POINT(pl.points[0], AcGePoint3d(10.0, 20.0, 0.0));
    CHECK_POINT(pl.points[1], AcGePoint3d(110.0, 20.0, 0.0));
    CHECK_POINT(pl.points[2], AcGePoint3d(110.0, 70.0, 0.0));
    CHECK_POINT(pl.points[3], AcGePoint3d(10.0, 70.0, 0.0));
    CHECK_POINT(pl.points[4], pl.points[0]);  // контур замкнут
    CHECK(pl.hasNormal);
    CHECK_VECTOR(pl.normal, AcGeVector3d::kZAxis);
}

TEST(WorldDraw_DrawsText1AndText2InsideRectangle)
{
    MyDevice device(AcGePoint3d(10.0, 20.0, 0.0));
    device.setText1(L"Насос");
    device.setText2(L"N1");
    RecordingWorldDraw wd;
    device.worldDraw(&wd);

    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() != 2)
        return;
    CHECK_WSTR(wd.texts[0].message, L"Насос");
    CHECK_WSTR(wd.texts[1].message, L"N1");
    for (const RecordingWorldDraw::Text& t : wd.texts)
    {
        CHECK_NEAR(t.height, MyDevice::kTextHeight, kTol);
        CHECK_NEAR(t.width, 1.0, kTol);
        CHECK_NEAR(t.oblique, 0.0, kTol);
        CHECK_VECTOR(t.direction, AcGeVector3d::kXAxis);
        CHECK_VECTOR(t.normal, AcGeVector3d::kZAxis);
        // Базовая точка и верх текста лежат внутри прямоугольника.
        CHECK(t.position.x > 10.0 && t.position.x < 110.0);
        CHECK(t.position.y > 20.0 && t.position.y + t.height < 70.0);
    }
    // Text1 — над Text2.
    CHECK(wd.texts[0].position.y > wd.texts[1].position.y + MyDevice::kTextHeight);
}

TEST(WorldDraw_DoesNotCreateDatabaseEntities)
{
    mock::reset();
    MyDevice device;
    RecordingWorldDraw wd;
    device.worldDraw(&wd);
    // Тексты — примитивы AcGi, а не AcDbText/AcDbMText в базе.
    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(0));
    CHECK_EQ(wd.polylines.size() + wd.texts.size(), static_cast<size_t>(3));
}

TEST(WorldDraw_RespectsRegenAbort)
{
    MyDevice device;
    RecordingWorldDraw wd;
    wd.abort = true;
    CHECK(device.worldDraw(&wd));
    CHECK(wd.polylines.empty());
    CHECK(wd.texts.empty());
}

TEST(WorldDraw_FollowsOrientationAndScale)
{
    MyDevice device(AcGePoint3d(0.0, 0.0, 0.0));
    device.setOrientation(AcGeVector3d::kYAxis, AcGeVector3d::kZAxis);  // повёрнут на 90°
    device.setScale(2.0);
    RecordingWorldDraw wd;
    device.worldDraw(&wd);
    CHECK_EQ(wd.polylines.size(), static_cast<size_t>(1));
    if (wd.polylines.empty())
        return;
    CHECK_POINT(wd.polylines[0].points[1], AcGePoint3d(0.0, 200.0, 0.0));
    CHECK_POINT(wd.polylines[0].points[2], AcGePoint3d(-100.0, 200.0, 0.0));
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() != 2)
        return;
    CHECK_NEAR(wd.texts[0].height, 2.0 * MyDevice::kTextHeight, kTol);
    CHECK_VECTOR(wd.texts[0].direction, AcGeVector3d::kYAxis);
}

TEST(GeomExtents_AreRectangle)
{
    MyDevice device(AcGePoint3d(-5.0, 3.0, 1.0));
    AcDbExtents ext;
    CHECK_EQ(device.getGeomExtents(ext), Acad::eOk);
    CHECK_POINT(ext.minPoint(), AcGePoint3d(-5.0, 3.0, 1.0));
    CHECK_POINT(ext.maxPoint(), AcGePoint3d(95.0, 53.0, 1.0));
}

// ---------------------------------------------------------------------------
// DWG
// ---------------------------------------------------------------------------

TEST(Dwg_RoundTripRestoresAllFields)
{
    MyDevice* pOriginal = makeCustomDevice();
    MemoryDwgFiler filer;
    CHECK_EQ(pOriginal->dwgOutFields(&filer), Acad::eOk);

    // Первое поле MyDevice после данных AcDbEntity — номер версии.
    CHECK(filer.items.size() >= 2);
    CHECK(filer.items[1].type == MemoryDwgFiler::Type::Int16);
    CHECK_EQ(filer.items[1].i, static_cast<Adesk::Int32>(MyDevice::kCurrentVersion));

    // Как при открытии чертежа: объект создаётся по описанию класса и читается.
    filer.rewind();
    AcRxObject* pObj = MyDevice::desc()->create();
    MyDevice* pRestored = MyDevice::cast(pObj);
    CHECK_EQ(pRestored->dwgInFields(&filer), Acad::eOk);
    CHECK_EQ(filer.cursor, filer.items.size());  // всё записанное прочитано
    CHECK_SAME_DEVICE(*pRestored, *pOriginal);

    delete pObj;
    delete pOriginal;
}

TEST(Dwg_RoundTripOfDefaultDevice)
{
    MyDevice original(AcGePoint3d(100.0, 200.0, 0.0));
    MemoryDwgFiler filer;
    original.dwgOutFields(&filer);
    filer.rewind();
    MyDevice restored;
    restored.setText1(L"other");
    restored.setText2(L"other");
    CHECK_EQ(restored.dwgInFields(&filer), Acad::eOk);
    CHECK_SAME_DEVICE(restored, original);
    CHECK_WSTR(restored.text1().kACharPtr(), L"TEXT1");
    CHECK_WSTR(restored.text2().kACharPtr(), L"TEXT2");
}

TEST(Dwg_NewerVersionBecomesProxy)
{
    MyDevice original;
    MemoryDwgFiler filer;
    original.dwgOutFields(&filer);
    filer.items[1].i = MyDevice::kCurrentVersion + 1;
    filer.rewind();
    MyDevice restored;
    CHECK_EQ(restored.dwgInFields(&filer), Acad::eMakeMeProxy);

    filer.items[1].i = 0;
    filer.rewind();
    CHECK_EQ(restored.dwgInFields(&filer), Acad::eMakeMeProxy);
}

TEST(Dwg_TruncatedDataReportsError)
{
    MyDevice original;
    MemoryDwgFiler filer;
    original.dwgOutFields(&filer);
    filer.items.pop_back();  // потеряна последняя строка (Text2)
    filer.rewind();
    MyDevice restored;
    CHECK(restored.dwgInFields(&filer) != Acad::eOk);
}

// ---------------------------------------------------------------------------
// DXF
// ---------------------------------------------------------------------------

TEST(Dxf_WritesSubclassMarkerAndGroupCodes)
{
    MyDevice* pDevice = makeCustomDevice();
    MemoryDxfFiler filer;
    CHECK_EQ(pDevice->dxfOutFields(&filer), Acad::eOk);

    // Маркер подкласса MyDevice идёт после данных AcDbEntity.
    bool foundMarker = false;
    size_t markerIndex = 0;
    for (size_t i = 0; i < filer.items.size(); ++i)
        if (filer.items[i].code == AcDb::kDxfSubclass && filer.items[i].str == L"MyDevice")
        {
            foundMarker = true;
            markerIndex = i;
        }
    CHECK(foundMarker);
    CHECK(markerIndex + 1 < filer.items.size() && filer.items[markerIndex + 1].code == 70);

    const MemoryDxfFiler::Item* pVersion = filer.find(70);
    const MemoryDxfFiler::Item* pPosition = filer.find(10);
    const MemoryDxfFiler::Item* pXDir = filer.find(11);
    const MemoryDxfFiler::Item* pScale = filer.find(40);
    const MemoryDxfFiler::Item* pNormal = filer.find(210);
    const MemoryDxfFiler::Item* pText1 = filer.find(300);
    const MemoryDxfFiler::Item* pText2 = filer.find(301);
    CHECK(pVersion && pPosition && pXDir && pScale && pNormal && pText1 && pText2);
    if (pVersion && pPosition && pXDir && pScale && pNormal && pText1 && pText2)
    {
        CHECK_EQ(pVersion->i, MyDevice::kCurrentVersion);
        CHECK_NEAR(pPosition->d[0], 12.5, kTol);
        CHECK_NEAR(pPosition->d[1], -7.25, kTol);
        CHECK_NEAR(pPosition->d[2], 3.0, kTol);
        CHECK_NEAR(pXDir->d[0], std::sqrt(0.5), kTol);
        CHECK_NEAR(pXDir->d[1], std::sqrt(0.5), kTol);
        CHECK_NEAR(pScale->d[0], 2.5, kTol);
        CHECK_NEAR(pNormal->d[2], 1.0, kTol);
        CHECK_WSTR(pText1->str, L"Насос №1");
        CHECK_WSTR(pText2->str, L"P = 10 кВт; \"кавычки\"");
    }
    delete pDevice;
}

TEST(Dxf_RoundTripRestoresAllFields)
{
    MyDevice* pOriginal = makeCustomDevice();
    MemoryDxfFiler filer;
    pOriginal->dxfOutFields(&filer);
    filer.rewind();

    AcRxObject* pObj = MyDevice::desc()->create();
    MyDevice* pRestored = MyDevice::cast(pObj);
    CHECK_EQ(pRestored->dxfInFields(&filer), Acad::eOk);
    CHECK_EQ(filer.cursor, filer.items.size());
    CHECK_SAME_DEVICE(*pRestored, *pOriginal);

    delete pObj;
    delete pOriginal;
}

TEST(Dxf_ReadsGroupsInAnyOrder_AndPushesBackForeignGroup)
{
    MemoryDxfFiler filer;
    filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
    filer.addString(AcDb::kDxfLayerName, L"0");
    filer.addString(AcDb::kDxfSubclass, L"MyDevice");
    filer.addString(301, L"второй");
    filer.addPoint(210, 0.0, 0.0, 1.0);
    filer.addString(300, L"первый");
    filer.addDouble(40, 1.5);
    filer.addPoint(11, 0.0, 1.0, 0.0);
    filer.addPoint(10, 1.0, 2.0, 3.0);
    filer.addInt16(70, 1);
    // Расширенные данные (XDATA) — не относятся к MyDevice.
    filer.addString(AcDb::kDxfRegAppName, L"SOMEAPP");
    filer.addString(AcDb::kDxfXdAsciiString, L"value");
    const size_t foreignIndex = filer.items.size() - 2;

    MyDevice device;
    CHECK_EQ(device.dxfInFields(&filer), Acad::eOk);
    CHECK_EQ(filer.cursor, foreignIndex);  // чужая группа возвращена филеру
    CHECK_POINT(device.position(), AcGePoint3d(1.0, 2.0, 3.0));
    CHECK_VECTOR(device.xDirection(), AcGeVector3d::kYAxis);
    CHECK_VECTOR(device.normal(), AcGeVector3d::kZAxis);
    CHECK_NEAR(device.scale(), 1.5, kTol);
    CHECK_WSTR(device.text1().kACharPtr(), L"первый");
    CHECK_WSTR(device.text2().kACharPtr(), L"второй");
}

TEST(Dxf_MissingOptionalGroupsUseDefaults)
{
    MemoryDxfFiler filer;
    filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
    filer.addString(AcDb::kDxfSubclass, L"MyDevice");
    filer.addPoint(10, 4.0, 5.0, 0.0);

    MyDevice device;
    device.setText1(L"x");
    CHECK_EQ(device.dxfInFields(&filer), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(4.0, 5.0, 0.0));
    CHECK_WSTR(device.text1().kACharPtr(), L"TEXT1");
    CHECK_WSTR(device.text2().kACharPtr(), L"TEXT2");
    CHECK_NEAR(device.scale(), 1.0, kTol);
}

TEST(Dxf_NewerVersionBecomesProxy)
{
    MemoryDxfFiler filer;
    filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
    filer.addString(AcDb::kDxfSubclass, L"MyDevice");
    filer.addInt16(70, MyDevice::kCurrentVersion + 1);
    MyDevice device;
    CHECK_EQ(device.dxfInFields(&filer), Acad::eMakeMeProxy);
}

TEST(Dxf_InvalidNormalOrScaleIsRejected)
{
    {
        MemoryDxfFiler filer;
        filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
        filer.addString(AcDb::kDxfSubclass, L"MyDevice");
        filer.addPoint(210, 0.0, 0.0, 0.0);
        MyDevice device;
        CHECK_EQ(device.dxfInFields(&filer), Acad::eInvalidDxfCode);
        CHECK_VECTOR(device.normal(), AcGeVector3d::kZAxis);
    }
    {
        MemoryDxfFiler filer;
        filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
        filer.addString(AcDb::kDxfSubclass, L"MyDevice");
        filer.addDouble(40, 0.0);
        filer.addString(300, L"changed");
        MyDevice device;
        CHECK_EQ(device.dxfInFields(&filer), Acad::eInvalidDxfCode);
        CHECK_WSTR(device.text1().kACharPtr(), L"TEXT1");  // объект не изменён
    }
}

TEST(Dxf_WithoutMyDeviceSubclassLeavesObjectUnchanged)
{
    MemoryDxfFiler filer;
    filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
    filer.addString(AcDb::kDxfSubclass, L"OtherClass");
    filer.addString(300, L"changed");
    MyDevice device;
    CHECK_EQ(device.dxfInFields(&filer), Acad::eOk);
    CHECK_WSTR(device.text1().kACharPtr(), L"TEXT1");
}

// ---------------------------------------------------------------------------
// Редактирование
// ---------------------------------------------------------------------------

TEST(TransformBy_Move)
{
    MyDevice device(AcGePoint3d(1.0, 2.0, 0.0));
    CHECK_EQ(device.transformBy(AcGeMatrix3d::translation(AcGeVector3d(10.0, -2.0, 5.0))), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(11.0, 0.0, 5.0));
    CHECK_VECTOR(device.xDirection(), AcGeVector3d::kXAxis);
    CHECK_NEAR(device.scale(), 1.0, kTol);
}

TEST(TransformBy_Rotate)
{
    MyDevice device(AcGePoint3d(10.0, 0.0, 0.0));
    CHECK_EQ(device.transformBy(AcGeMatrix3d::rotation(kPi / 2.0, AcGeVector3d::kZAxis)), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(0.0, 10.0, 0.0));
    CHECK_VECTOR(device.xDirection(), AcGeVector3d::kYAxis);
    CHECK_VECTOR(device.normal(), AcGeVector3d::kZAxis);

    AcDbExtents ext;
    device.getGeomExtents(ext);
    CHECK_POINT(ext.minPoint(), AcGePoint3d(-50.0, 10.0, 0.0));
    CHECK_POINT(ext.maxPoint(), AcGePoint3d(0.0, 110.0, 0.0));
}

TEST(TransformBy_UniformScale)
{
    MyDevice device(AcGePoint3d(10.0, 10.0, 0.0));
    CHECK_EQ(device.transformBy(AcGeMatrix3d::scaling(2.0, AcGePoint3d(10.0, 10.0, 0.0))), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(10.0, 10.0, 0.0));
    CHECK_NEAR(device.scale(), 2.0, kTol);
    AcDbExtents ext;
    device.getGeomExtents(ext);
    CHECK_POINT(ext.maxPoint(), AcGePoint3d(210.0, 110.0, 0.0));
}

TEST(TransformBy_Mirror)
{
    MyDevice device(AcGePoint3d(10.0, 0.0, 0.0));
    AcGeMatrix3d mirror;
    mirror.entry[0][0] = -1.0;  // отражение относительно плоскости YZ
    CHECK_EQ(device.transformBy(mirror), Acad::eOk);
    AcDbExtents ext;
    device.getGeomExtents(ext);
    CHECK_POINT(ext.minPoint(), AcGePoint3d(-110.0, 0.0, 0.0));
    CHECK_POINT(ext.maxPoint(), AcGePoint3d(-10.0, 50.0, 0.0));
    CHECK_NEAR(device.scale(), 1.0, kTol);
}

TEST(TransformBy_NonUniformScaleIsRejected)
{
    MyDevice device(AcGePoint3d(1.0, 1.0, 0.0));
    AcGeMatrix3d stretch;
    stretch.entry[0][0] = 2.0;
    CHECK_EQ(device.transformBy(stretch), Acad::eCannotScaleNonUniformly);
    CHECK_POINT(device.position(), AcGePoint3d(1.0, 1.0, 0.0));
    CHECK_NEAR(device.scale(), 1.0, kTol);
}

TEST(Grips_SingleGripAtInsertionPointMovesDevice)
{
    MyDevice device(AcGePoint3d(3.0, 4.0, 0.0));
    AcGePoint3dArray grips;
    AcDbIntArray osnapModes, geomIds;
    CHECK_EQ(device.getGripPoints(grips, osnapModes, geomIds), Acad::eOk);
    CHECK_EQ(grips.length(), 1);
    if (grips.length() == 1)
        CHECK_POINT(grips[0], AcGePoint3d(3.0, 4.0, 0.0));

    AcDbIntArray none;
    CHECK_EQ(device.moveGripPointsAt(none, AcGeVector3d(1.0, 1.0, 0.0)), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(3.0, 4.0, 0.0));

    AcDbIntArray first;
    first.append(0);
    CHECK_EQ(device.moveGripPointsAt(first, AcGeVector3d(1.0, -1.0, 0.0)), Acad::eOk);
    CHECK_POINT(device.position(), AcGePoint3d(4.0, 3.0, 0.0));
}

TEST(List_PrintsInsertionPointAndTexts)
{
    mock::reset();
    MyDevice device(AcGePoint3d(1.5, 2.0, 0.0));
    device.setText1(L"Щит");
    device.list();
    std::wstring all;
    for (const std::wstring& line : mock::printed)
        all += line;
    CHECK(all.find(L"1.5") != std::wstring::npos);
    CHECK(all.find(L"Щит") != std::wstring::npos);
    CHECK(all.find(L"TEXT2") != std::wstring::npos);
}

// ---------------------------------------------------------------------------
// Команда MYDEVICE
// ---------------------------------------------------------------------------

TEST(Command_CreatesDeviceInModelSpaceAtPickedPoint)
{
    mock::reset();
    mock::getPointValue = AcGePoint3d(5.0, 6.0, 0.0);
    MyDeviceCommand();

    CHECK(mock::lastPrompt.find(L"точку вставки") != std::wstring::npos);
    AcDbBlockTableRecord& ms = modelSpace();
    CHECK_EQ(ms.entities.size(), static_cast<size_t>(1));
    if (ms.entities.size() != 1)
        return;
    MyDevice* pDevice = MyDevice::cast(ms.entities[0]);
    CHECK(pDevice != nullptr);
    if (!pDevice)
        return;
    CHECK_POINT(pDevice->position(), AcGePoint3d(5.0, 6.0, 0.0));
    CHECK_WSTR(pDevice->text1().kACharPtr(), L"TEXT1");
    CHECK_WSTR(pDevice->text2().kACharPtr(), L"TEXT2");
    CHECK_EQ(pDevice->closeCount(), 1);  // объект закрыт после добавления
    CHECK(pDevice->defaultsDatabase() == acdbHostApplicationServices()->workingDatabase());
    CHECK_EQ(acdbHostApplicationServices()->workingDatabase()->blockTable.lastOpenMode, AcDb::kForWrite);
    CHECK(mock::printed.empty());  // без сообщений об ошибках
}

TEST(Command_CancelCreatesNothing)
{
    mock::reset();
    mock::getPointResult = RTCAN;
    MyDeviceCommand();
    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(0));
}

TEST(Command_ConvertsUcsPointToWcs)
{
    mock::reset();
    // ПСК повёрнута на 90° вокруг Z, начало ПСК — (100, 0, 0) в МСК.
    mock::currentUcs = AcGeMatrix3d::translation(AcGeVector3d(100.0, 0.0, 0.0))
                       * AcGeMatrix3d::rotation(kPi / 2.0, AcGeVector3d::kZAxis);
    mock::getPointValue = AcGePoint3d(10.0, 0.0, 0.0);
    MyDeviceCommand();

    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(1));
    if (modelSpace().entities.size() != 1)
        return;
    MyDevice* pDevice = MyDevice::cast(modelSpace().entities[0]);
    CHECK_POINT(pDevice->position(), AcGePoint3d(100.0, 10.0, 0.0));
    CHECK_VECTOR(pDevice->xDirection(), AcGeVector3d::kYAxis);
    CHECK_VECTOR(pDevice->normal(), AcGeVector3d::kZAxis);
}

// Сценарий из задачи: создать объект командой, сохранить чертёж, открыть снова.
TEST(Scenario_CreateSaveReopen_RestoresPositionAndTexts)
{
    mock::reset();
    mock::getPointValue = AcGePoint3d(250.0, 125.0, 0.0);
    MyDeviceCommand();
    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(1));
    if (modelSpace().entities.size() != 1)
        return;
    MyDevice* pCreated = MyDevice::cast(modelSpace().entities[0]);
    pCreated->setText1(L"Насос");
    pCreated->setText2(L"Н-1");

    // «Сохранение»: DWG и DXF.
    MemoryDwgFiler dwg;
    MemoryDxfFiler dxf;
    CHECK_EQ(pCreated->dwgOutFields(&dwg), Acad::eOk);
    CHECK_EQ(pCreated->dxfOutFields(&dxf), Acad::eOk);

    // «Открытие»: новый экземпляр создаётся по описанию класса.
    for (int pass = 0; pass < 2; ++pass)
    {
        AcRxObject* pObj = MyDevice::desc()->create();
        MyDevice* pReopened = MyDevice::cast(pObj);
        if (pass == 0)
        {
            dwg.rewind();
            CHECK_EQ(pReopened->dwgInFields(&dwg), Acad::eOk);
        }
        else
        {
            dxf.rewind();
            CHECK_EQ(pReopened->dxfInFields(&dxf), Acad::eOk);
        }
        CHECK_POINT(pReopened->position(), AcGePoint3d(250.0, 125.0, 0.0));
        CHECK_WSTR(pReopened->text1().kACharPtr(), L"Насос");
        CHECK_WSTR(pReopened->text2().kACharPtr(), L"Н-1");

        // Восстановленный объект рисуется так же, как исходный.
        RecordingWorldDraw before, after;
        pCreated->worldDraw(&before);
        pReopened->worldDraw(&after);
        CHECK_EQ(after.polylines.size(), before.polylines.size());
        CHECK_EQ(after.texts.size(), before.texts.size());
        for (size_t i = 0; i < before.texts.size() && i < after.texts.size(); ++i)
        {
            CHECK_POINT(after.texts[i].position, before.texts[i].position);
            CHECK_WSTR(after.texts[i].message, before.texts[i].message);
        }
        delete pObj;
    }
}

// ---------------------------------------------------------------------------
// Этап 2: свойства Text1 и Text2
// ---------------------------------------------------------------------------

namespace
{
    MyDeviceText makeText(const wchar_t* content, double height, double x, double y,
                          const wchar_t* layer, const wchar_t* style)
    {
        MyDeviceText text;
        text.text = content;
        text.height = height;
        text.x = x;
        text.y = y;
        text.layer = layer;
        text.textStyle = style;
        return text;
    }

    void checkSameText(const char* file, int line, const MyDeviceText& actual,
                       const MyDeviceText& expected)
    {
        if (actual.text != expected.text)
            testing::fail(file, line, "text differs: " + testing::narrow(actual.text.kACharPtr()));
        if (std::fabs(actual.height - expected.height) > kTol)
            testing::fail(file, line, "height differs");
        if (std::fabs(actual.x - expected.x) > kTol || std::fabs(actual.y - expected.y) > kTol)
            testing::fail(file, line, "position differs");
        if (actual.layer != expected.layer)
            testing::fail(file, line, "layer differs: " + testing::narrow(actual.layer.kACharPtr()));
        if (actual.textStyle != expected.textStyle)
            testing::fail(file, line, "text style differs: "
                          + testing::narrow(actual.textStyle.kACharPtr()));
    }

    // Объект в пространстве модели рабочей базы со своими слоями и стилями текстов.
    // Возвращаемым объектом владеет база данных.
    MyDevice* appendStyledDevice()
    {
        AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
        pDb->mockAddLayer(L"DEVICES");
        pDb->mockAddLayer(L"LABELS");
        pDb->mockAddLayer(L"NOTES");
        pDb->mockAddTextStyle(L"GOST");
        MyDevice* pDevice = new MyDevice(AcGePoint3d(100.0, 50.0, 0.0));
        pDevice->setLayer(L"DEVICES");
        pDevice->setTextAt(MyDevice::kText1, makeText(L"Насос", 7.5, 2.0, 40.0, L"LABELS", L"GOST"));
        pDevice->setTextAt(MyDevice::kText2, makeText(L"Н-1", 3.5, 60.0, -8.0, L"NOTES", L"Standard"));
        AcDbObjectId id;
        modelSpace().appendAcDbEntity(id, pDevice);
        return pDevice;
    }

    AcDbObjectId findLayerId(const wchar_t* name)
    {
        AcDbObjectId id;
        acdbHostApplicationServices()->workingDatabase()->layerTable.getAt(name, id);
        return id;
    }
}

#define CHECK_SAME_TEXT(a, e) checkSameText(__FILE__, __LINE__, (a), (e))

TEST(Stage2_DefaultTextProperties)
{
    MyDevice device;
    const MyDeviceText t1 = device.textAt(MyDevice::kText1);
    const MyDeviceText t2 = device.textAt(MyDevice::kText2);
    CHECK_SAME_TEXT(t1, makeText(L"TEXT1", MyDevice::kTextHeight, MyDevice::kTextMargin, 30.0,
                                 L"", L"Standard"));
    CHECK_SAME_TEXT(t2, makeText(L"TEXT2", MyDevice::kTextHeight, MyDevice::kTextMargin, 10.0,
                                 L"", L"Standard"));
    CHECK_SAME_TEXT(MyDevice::defaultText(MyDevice::kText1), t1);
    CHECK_SAME_TEXT(MyDevice::defaultText(MyDevice::kText2), t2);
    // text1()/text2() — те же строки, что и в textAt().
    CHECK(device.text1() == t1.text);
    CHECK(device.text2() == t2.text);
}

TEST(Stage2_SetTextAt_ChangesOnlyThatText)
{
    MyDevice device;
    const MyDeviceText custom = makeText(L"Щит", 2.5, -3.0, 70.0, L"LABELS", L"GOST");
    CHECK_EQ(device.setTextAt(MyDevice::kText2, custom), Acad::eOk);
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText2), custom);
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText1), MyDevice::defaultText(MyDevice::kText1));
    CHECK_WSTR(device.text2().kACharPtr(), L"Щит");

    // setText1() меняет только содержимое.
    CHECK_EQ(device.setText1(L"new"), Acad::eOk);
    MyDeviceText expected = MyDevice::defaultText(MyDevice::kText1);
    expected.text = L"new";
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText1), expected);
}

TEST(Stage2_SetTextAt_RejectsInvalidValues)
{
    MyDevice device;
    const MyDeviceText valid = makeText(L"x", 1.0, 0.0, 0.0, L"", L"Standard");
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    CHECK_EQ(device.setTextAt(-1, valid), Acad::eInvalidInput);
    CHECK_EQ(device.setTextAt(MyDevice::kTextCount, valid), Acad::eInvalidInput);

    MyDeviceText bad = valid;
    bad.height = 0.0;
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);
    bad.height = -2.0;
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);
    bad.height = inf;
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);
    bad.height = nan;
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);

    bad = valid;
    bad.x = nan;
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);
    bad = valid;
    bad.y = -inf;
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);
    bad = valid;
    bad.textStyle = L"";
    CHECK_EQ(device.setTextAt(MyDevice::kText1, bad), Acad::eInvalidInput);

    // Объект не изменился.
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText1), MyDevice::defaultText(MyDevice::kText1));

    // Неверный номер в textAt() не приводит к выходу за границы массива.
    CHECK_SAME_TEXT(device.textAt(5), MyDevice::defaultText(MyDevice::kText1));
}

TEST(Stage2_Draw_UsesTextPropertiesAndDevicePlacement)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    pDevice->setOrientation(AcGeVector3d::kYAxis, AcGeVector3d::kZAxis);  // поворот на 90°
    pDevice->setScale(2.0);

    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() != 2)
        return;
    const RecordingWorldDraw::Text& t1 = wd.texts[0];
    const RecordingWorldDraw::Text& t2 = wd.texts[1];

    // Локальная точка (2, 40) при повороте на 90° и масштабе 2: (100 - 80, 50 + 4).
    CHECK_POINT(t1.position, AcGePoint3d(20.0, 54.0, 0.0));
    // Локальная точка (60, -8): (100 + 16, 50 + 120).
    CHECK_POINT(t2.position, AcGePoint3d(116.0, 170.0, 0.0));
    CHECK_NEAR(t1.height, 15.0, kTol);  // 7.5 * 2
    CHECK_NEAR(t2.height, 7.0, kTol);   // 3.5 * 2
    for (const RecordingWorldDraw::Text& t : wd.texts)
    {
        CHECK(t.hasStyle);         // шрифт — из текстового стиля AutoCAD
        CHECK(t.styleLoaded);      // стиль загружен перед отрисовкой
        CHECK_EQ(t.length, -1);    // строка завершается нулём
        CHECK(!t.raw);             // управляющие коды (%%c и т.п.) обрабатываются
        CHECK_VECTOR(t.direction, AcGeVector3d::kYAxis);
        CHECK_VECTOR(t.normal, AcGeVector3d::kZAxis);
    }
    CHECK_WSTR(t1.message, L"Насос");
    CHECK_WSTR(t2.message, L"Н-1");
    CHECK_WSTR(t1.styleName, L"GOST");
    CHECK_WSTR(t2.styleName, L"Standard");
}

TEST(Stage2_Draw_EachTextOnItsLayer_RectangleOnDeviceLayer)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();

    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK_EQ(wd.polylines.size(), static_cast<size_t>(1));
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.polylines.size() != 1 || wd.texts.size() != 2)
        return;
    // Прямоугольник рисуется до смены слоя — на слое самого объекта.
    CHECK(wd.polylines[0].layerId.isNull());
    CHECK(wd.texts[0].layerId == findLayerId(L"LABELS"));
    CHECK(wd.texts[1].layerId == findLayerId(L"NOTES"));
    CHECK(!wd.texts[0].layerId.isNull());
    CHECK(!wd.texts[1].layerId.isNull());
}

TEST(Stage2_Draw_EmptyOrMissingLayerFallsBackToDeviceLayer)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    MyDeviceText t1 = pDevice->textAt(MyDevice::kText1);
    t1.layer = L"NO_SUCH_LAYER";
    pDevice->setTextAt(MyDevice::kText1, t1);
    MyDeviceText t2 = pDevice->textAt(MyDevice::kText2);
    t2.layer = L"";
    pDevice->setTextAt(MyDevice::kText2, t2);

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    const size_t layerCount = pDb->layerTable.records.size();

    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() != 2)
        return;
    // Слой назначается явно, иначе Text2 унаследовал бы слой Text1.
    CHECK(wd.texts[0].layerId == findLayerId(L"DEVICES"));
    CHECK(wd.texts[1].layerId == findLayerId(L"DEVICES"));
    // Отрисовка не создаёт слоёв и не открывает таблицу на запись.
    CHECK_EQ(pDb->layerTable.records.size(), layerCount);
    CHECK_EQ(pDb->layerTable.addCalls, 0);
    CHECK_EQ(pDb->lastLayerTableMode, AcDb::kForRead);
    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(1));
}

TEST(Stage2_Draw_LayerNameIsCaseInsensitive)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    MyDeviceText t1 = pDevice->textAt(MyDevice::kText1);
    t1.layer = L"labels";
    pDevice->setTextAt(MyDevice::kText1, t1);
    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK(!wd.texts.empty() && wd.texts[0].layerId == findLayerId(L"LABELS"));
}

TEST(Stage2_Draw_MissingTextStyleFallsBackToStandard)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    MyDeviceText t1 = pDevice->textAt(MyDevice::kText1);
    t1.textStyle = L"DELETED_STYLE";
    pDevice->setTextAt(MyDevice::kText1, t1);

    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK(!wd.texts.empty());
    if (wd.texts.empty())
        return;
    CHECK_WSTR(wd.texts[0].styleName, L"Standard");
    CHECK_NEAR(wd.texts[0].height, 7.5, kTol);  // высота свойства сохраняется
    CHECK_EQ(pDb->textStyleTable.addCalls, 0);
    CHECK_EQ(pDb->lastTextStyleTableMode, AcDb::kForRead);
    // Само свойство не меняется: стиль может появиться в чертеже позже.
    CHECK_WSTR(pDevice->textAt(MyDevice::kText1).textStyle.kACharPtr(), L"DELETED_STYLE");
}

TEST(Stage2_Draw_ObjectOutsideDatabaseUsesWorkingDatabase)
{
    mock::reset();
    acdbHostApplicationServices()->workingDatabase()->mockAddTextStyle(L"GOST");
    MyDevice device;
    MyDeviceText t1 = device.textAt(MyDevice::kText1);
    t1.textStyle = L"GOST";
    device.setTextAt(MyDevice::kText1, t1);
    RecordingWorldDraw wd;
    device.worldDraw(&wd);
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() != 2)
        return;
    CHECK_WSTR(wd.texts[0].styleName, L"GOST");
    CHECK_WSTR(wd.texts[1].styleName, L"Standard");
}

TEST(Stage2_GeomExtents_IncludeTextsOutsideRectangle)
{
    MyDevice device(AcGePoint3d(0.0, 0.0, 0.0));
    device.setTextAt(MyDevice::kText1, makeText(L"a", 20.0, -30.0, 45.0, L"", L"Standard"));
    device.setTextAt(MyDevice::kText2, makeText(L"b", 5.0, 120.0, -10.0, L"", L"Standard"));
    AcDbExtents ext;
    CHECK_EQ(device.getGeomExtents(ext), Acad::eOk);
    CHECK_POINT(ext.minPoint(), AcGePoint3d(-30.0, -10.0, 0.0));
    CHECK_POINT(ext.maxPoint(), AcGePoint3d(120.0, 65.0, 0.0));
}

TEST(Stage2_TransformBy_KeepsLocalTextProperties)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    const MyDeviceText t1 = pDevice->textAt(MyDevice::kText1);
    const MyDeviceText t2 = pDevice->textAt(MyDevice::kText2);

    RecordingWorldDraw before;
    pDevice->worldDraw(&before);

    // Перемещение, поворот на 30° и масштаб 3 относительно произвольной точки.
    const AcGeMatrix3d xform = AcGeMatrix3d::translation(AcGeVector3d(5.0, -7.0, 0.0))
        * AcGeMatrix3d::rotation(kPi / 6.0, AcGeVector3d::kZAxis, AcGePoint3d(1.0, 2.0, 0.0))
        * AcGeMatrix3d::scaling(3.0, AcGePoint3d(-4.0, 0.0, 0.0));
    CHECK_EQ(pDevice->transformBy(xform), Acad::eOk);

    // Локальные свойства текстов не меняются...
    CHECK_SAME_TEXT(pDevice->textAt(MyDevice::kText1), t1);
    CHECK_SAME_TEXT(pDevice->textAt(MyDevice::kText2), t2);

    // ...а нарисованные тексты преобразуются вместе с объектом.
    RecordingWorldDraw after;
    pDevice->worldDraw(&after);
    CHECK_EQ(after.texts.size(), before.texts.size());
    for (size_t i = 0; i < before.texts.size() && i < after.texts.size(); ++i)
    {
        AcGePoint3d expected = before.texts[i].position;
        expected.transformBy(xform);
        CHECK_POINT(after.texts[i].position, expected);
        AcGeVector3d dir = before.texts[i].direction;
        dir.transformBy(xform);
        CHECK_VECTOR(after.texts[i].direction, dir.normal());
        CHECK_NEAR(after.texts[i].height, 3.0 * before.texts[i].height, 1.0e-9);
    }
}

TEST(Stage2_Dwg_RoundTripRestoresTextProperties)
{
    mock::reset();
    MyDevice* pOriginal = appendStyledDevice();
    pOriginal->setOrientation(AcGeVector3d(0.0, -1.0, 0.0), AcGeVector3d::kZAxis);
    MemoryDwgFiler filer;
    CHECK_EQ(pOriginal->dwgOutFields(&filer), Acad::eOk);
    filer.rewind();

    AcRxObject* pObj = MyDevice::desc()->create();
    MyDevice* pRestored = MyDevice::cast(pObj);
    CHECK_EQ(pRestored->dwgInFields(&filer), Acad::eOk);
    CHECK_EQ(filer.cursor, filer.items.size());
    CHECK_SAME_DEVICE(*pRestored, *pOriginal);
    for (int i = 0; i < MyDevice::kTextCount; ++i)
        CHECK_SAME_TEXT(pRestored->textAt(i), pOriginal->textAt(i));
    delete pObj;
}

TEST(Stage2_Dwg_ReadsVersion1WithDefaultTextProperties)
{
    // Поток в формате этапа 1: версия 1, затем точка, оси, масштаб и две строки.
    MyDevice entityData;
    MemoryDwgFiler filer;
    static_cast<AcDbEntity&>(entityData).AcDbEntity::dwgOutFields(&filer);
    filer.writeInt16(1);
    filer.writePoint3d(AcGePoint3d(7.0, 8.0, 0.0));
    filer.writeVector3d(AcGeVector3d::kYAxis);
    filer.writeVector3d(AcGeVector3d::kZAxis);
    filer.writeDouble(1.5);
    filer.writeString(AcString(L"старый 1"));
    filer.writeString(AcString(L"старый 2"));
    filer.rewind();

    MyDevice device;
    device.setTextAt(MyDevice::kText1, makeText(L"x", 99.0, 1.0, 1.0, L"L", L"S"));
    CHECK_EQ(device.dwgInFields(&filer), Acad::eOk);
    CHECK_EQ(filer.cursor, filer.items.size());
    CHECK_POINT(device.position(), AcGePoint3d(7.0, 8.0, 0.0));
    CHECK_VECTOR(device.xDirection(), AcGeVector3d::kYAxis);
    CHECK_NEAR(device.scale(), 1.5, kTol);
    MyDeviceText expected1 = MyDevice::defaultText(MyDevice::kText1);
    expected1.text = L"старый 1";
    MyDeviceText expected2 = MyDevice::defaultText(MyDevice::kText2);
    expected2.text = L"старый 2";
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText1), expected1);
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText2), expected2);

    // Объект версии 1 рисуется как на этапе 1.
    RecordingWorldDraw wd;
    device.worldDraw(&wd);
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() == 2)
    {
        CHECK_NEAR(wd.texts[0].height, 1.5 * MyDevice::kTextHeight, kTol);
        CHECK_POINT(wd.texts[0].position, AcGePoint3d(7.0 - 1.5 * 30.0, 8.0 + 1.5 * 5.0, 0.0));
    }
}

TEST(Stage2_Dwg_TruncatedVersion2DataLeavesTextsUnchanged)
{
    mock::reset();
    MyDevice* pOriginal = appendStyledDevice();
    MemoryDwgFiler filer;
    pOriginal->dwgOutFields(&filer);
    filer.items.pop_back();  // потерян стиль Text2
    filer.rewind();
    MyDevice restored;
    CHECK(restored.dwgInFields(&filer) != Acad::eOk);
    CHECK_SAME_TEXT(restored.textAt(MyDevice::kText1), MyDevice::defaultText(MyDevice::kText1));
    CHECK_SAME_TEXT(restored.textAt(MyDevice::kText2), MyDevice::defaultText(MyDevice::kText2));
}

TEST(Stage2_Dxf_WritesTextPropertyGroups)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    MemoryDxfFiler filer;
    CHECK_EQ(pDevice->dxfOutFields(&filer), Acad::eOk);

    const MemoryDxfFiler::Item* pH1 = filer.find(41);
    const MemoryDxfFiler::Item* pH2 = filer.find(42);
    const MemoryDxfFiler::Item* pP1 = filer.find(12);
    const MemoryDxfFiler::Item* pP2 = filer.find(13);
    const MemoryDxfFiler::Item* pL1 = filer.find(302);
    const MemoryDxfFiler::Item* pL2 = filer.find(303);
    const MemoryDxfFiler::Item* pS1 = filer.find(304);
    const MemoryDxfFiler::Item* pS2 = filer.find(305);
    CHECK(pH1 && pH2 && pP1 && pP2 && pL1 && pL2 && pS1 && pS2);
    if (!(pH1 && pH2 && pP1 && pP2 && pL1 && pL2 && pS1 && pS2))
        return;
    CHECK_NEAR(pH1->d[0], 7.5, kTol);
    CHECK_NEAR(pH2->d[0], 3.5, kTol);
    CHECK_NEAR(pP1->d[0], 2.0, kTol);
    CHECK_NEAR(pP1->d[1], 40.0, kTol);
    CHECK_NEAR(pP1->d[2], 0.0, kTol);
    CHECK_NEAR(pP2->d[0], 60.0, kTol);
    CHECK_NEAR(pP2->d[1], -8.0, kTol);
    CHECK_WSTR(pL1->str, L"LABELS");
    CHECK_WSTR(pL2->str, L"NOTES");
    CHECK_WSTR(pS1->str, L"GOST");
    CHECK_WSTR(pS2->str, L"Standard");
}

TEST(Stage2_Dxf_RoundTripRestoresTextProperties)
{
    mock::reset();
    MyDevice* pOriginal = appendStyledDevice();
    MemoryDxfFiler filer;
    pOriginal->dxfOutFields(&filer);
    filer.rewind();

    AcRxObject* pObj = MyDevice::desc()->create();
    MyDevice* pRestored = MyDevice::cast(pObj);
    CHECK_EQ(pRestored->dxfInFields(&filer), Acad::eOk);
    CHECK_EQ(filer.cursor, filer.items.size());
    CHECK_SAME_DEVICE(*pRestored, *pOriginal);
    for (int i = 0; i < MyDevice::kTextCount; ++i)
        CHECK_SAME_TEXT(pRestored->textAt(i), pOriginal->textAt(i));
    delete pObj;
}

TEST(Stage2_Dxf_Version1FileUsesDefaultTextProperties)
{
    MemoryDxfFiler filer;
    filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
    filer.addString(AcDb::kDxfSubclass, L"MyDevice");
    filer.addInt16(70, 1);
    filer.addPoint(10, 1.0, 1.0, 0.0);
    filer.addString(300, L"A");
    filer.addString(301, L"B");

    MyDevice device;
    device.setTextAt(MyDevice::kText2, makeText(L"x", 99.0, 1.0, 1.0, L"L", L"S"));
    CHECK_EQ(device.dxfInFields(&filer), Acad::eOk);
    MyDeviceText expected2 = MyDevice::defaultText(MyDevice::kText2);
    expected2.text = L"B";
    CHECK_SAME_TEXT(device.textAt(MyDevice::kText2), expected2);
    CHECK_WSTR(device.text1().kACharPtr(), L"A");
}

TEST(Stage2_Dxf_InvalidTextPropertiesAreRejected)
{
    struct Case { int code; bool isString; double value; const wchar_t* str; };
    const Case cases[] = {
        { 41, false, 0.0, nullptr },
        { 42, false, -1.0, nullptr },
        { 305, true, 0.0, L"" },
    };
    for (const Case& c : cases)
    {
        MemoryDxfFiler filer;
        filer.addString(AcDb::kDxfSubclass, L"AcDbEntity");
        filer.addString(AcDb::kDxfSubclass, L"MyDevice");
        filer.addString(300, L"changed");
        if (c.isString)
            filer.addString(c.code, c.str);
        else
            filer.addDouble(c.code, c.value);
        MyDevice device;
        CHECK_EQ(device.dxfInFields(&filer), Acad::eInvalidDxfCode);
        CHECK(filer.errorMessage.find(L"Text") != std::wstring::npos);
        CHECK_WSTR(device.text1().kACharPtr(), L"TEXT1");  // объект не изменён
    }
}

TEST(Stage2_List_PrintsAllTextProperties)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    pDevice->list();
    std::wstring all;
    for (const std::wstring& line : mock::printed)
        all += line;
    for (const wchar_t* expected : { L"Text1: Насос", L"Text2: Н-1", L"7.5", L"3.5", L"40",
                                     L"-8", L"LABELS", L"NOTES", L"GOST", L"Standard",
                                     L"Height", L"Layer", L"Text style" })
    {
        if (all.find(expected) == std::wstring::npos)
            testing::fail(__FILE__, __LINE__, "list() output lacks " + testing::narrow(expected));
    }

    mock::printed.clear();
    MyDevice plain;
    plain.list();
    all.clear();
    for (const std::wstring& line : mock::printed)
        all += line;
    CHECK(all.find(L"(MyDevice layer)") != std::wstring::npos);
}

TEST(Stage2_Command_PutsTextsOnCurrentLayer)
{
    mock::reset();
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    pDb->mockAddLayer(L"EQUIPMENT");
    pDb->clayer = L"EQUIPMENT";
    MyDeviceCommand();

    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(1));
    if (modelSpace().entities.size() != 1)
        return;
    MyDevice* pDevice = MyDevice::cast(modelSpace().entities[0]);
    AcString layer;
    pDevice->layer(layer);
    CHECK_WSTR(layer.kACharPtr(), L"EQUIPMENT");
    for (int i = 0; i < MyDevice::kTextCount; ++i)
    {
        MyDeviceText expected = MyDevice::defaultText(i);
        expected.layer = L"EQUIPMENT";
        CHECK_SAME_TEXT(pDevice->textAt(i), expected);
    }
    CHECK_EQ(pDb->layerTable.addCalls, 0);
}

// Сценарий этапа 2: создать объект, изменить свойства обоих текстов, повернуть,
// сохранить и открыть снова — свойства, положение и поворот восстанавливаются.
TEST(Stage2_Scenario_EditSaveReopen_RestoresEverything)
{
    mock::reset();
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    pDb->mockAddLayer(L"LABELS");
    pDb->mockAddTextStyle(L"GOST");
    mock::getPointValue = AcGePoint3d(250.0, 125.0, 0.0);
    MyDeviceCommand();
    CHECK_EQ(modelSpace().entities.size(), static_cast<size_t>(1));
    if (modelSpace().entities.size() != 1)
        return;
    MyDevice* pCreated = MyDevice::cast(modelSpace().entities[0]);
    pCreated->setTextAt(MyDevice::kText1, makeText(L"Насос", 4.0, 10.0, 35.0, L"LABELS", L"GOST"));
    pCreated->setTextAt(MyDevice::kText2, makeText(L"Н-1", 2.5, 10.0, 5.0, L"0", L"Standard"));
    pCreated->transformBy(AcGeMatrix3d::rotation(kPi / 4.0, AcGeVector3d::kZAxis,
                                                 AcGePoint3d(250.0, 125.0, 0.0)));

    MemoryDwgFiler dwg;
    MemoryDxfFiler dxf;
    CHECK_EQ(pCreated->dwgOutFields(&dwg), Acad::eOk);
    CHECK_EQ(pCreated->dxfOutFields(&dxf), Acad::eOk);

    for (int pass = 0; pass < 2; ++pass)
    {
        AcRxObject* pObj = MyDevice::desc()->create();
        MyDevice* pReopened = MyDevice::cast(pObj);
        if (pass == 0)
        {
            dwg.rewind();
            CHECK_EQ(pReopened->dwgInFields(&dwg), Acad::eOk);
        }
        else
        {
            dxf.rewind();
            CHECK_EQ(pReopened->dxfInFields(&dxf), Acad::eOk);
        }
        pReopened->mockAttach(pDb, AcDbObjectId());
        CHECK_SAME_DEVICE(*pReopened, *pCreated);
        CHECK_POINT(pReopened->position(), AcGePoint3d(250.0, 125.0, 0.0));
        CHECK_VECTOR(pReopened->xDirection(), AcGeVector3d(std::sqrt(0.5), std::sqrt(0.5), 0.0));
        for (int i = 0; i < MyDevice::kTextCount; ++i)
            CHECK_SAME_TEXT(pReopened->textAt(i), pCreated->textAt(i));

        RecordingWorldDraw before, after;
        pCreated->worldDraw(&before);
        pReopened->worldDraw(&after);
        CHECK_EQ(after.texts.size(), before.texts.size());
        for (size_t i = 0; i < before.texts.size() && i < after.texts.size(); ++i)
        {
            CHECK_POINT(after.texts[i].position, before.texts[i].position);
            CHECK_VECTOR(after.texts[i].direction, before.texts[i].direction);
            CHECK_NEAR(after.texts[i].height, before.texts[i].height, kTol);
            CHECK_WSTR(after.texts[i].message, before.texts[i].message);
            CHECK_WSTR(after.texts[i].styleName, before.texts[i].styleName);
            CHECK(after.texts[i].layerId == before.texts[i].layerId);
        }
        delete pObj;
    }
}

// ---------------------------------------------------------------------------
// Этап 2: свойства для палитры свойств (MyDeviceProperties)
// ---------------------------------------------------------------------------

TEST(Stage2_Properties_TwoCategoriesWithSixPropertiesEach)
{
    const wchar_t* const names[] = { L"Text", L"Height", L"Position X", L"Position Y",
                                     L"Layer", L"Font" };
    CHECK_EQ(MyDeviceProperties::count(), 12);
    for (int i = 0; i < MyDeviceProperties::count(); ++i)
    {
        const MyDeviceProperties::Descriptor& d = MyDeviceProperties::at(i);
        const int textIndex = i / MyDeviceProperties::kFieldCount;
        CHECK_EQ(d.textIndex, textIndex);
        CHECK_EQ(static_cast<int>(d.field), i % MyDeviceProperties::kFieldCount);
        CHECK_WSTR(d.category, textIndex == MyDevice::kText1 ? L"Text 1" : L"Text 2");
        CHECK_WSTR(d.name, names[i % MyDeviceProperties::kFieldCount]);
        CHECK(d.description != nullptr && *d.description != L'\0');
    }
    CHECK(MyDeviceProperties::isNumeric(MyDeviceProperties::kHeight));
    CHECK(MyDeviceProperties::isNumeric(MyDeviceProperties::kPositionX));
    CHECK(MyDeviceProperties::isNumeric(MyDeviceProperties::kPositionY));
    CHECK(!MyDeviceProperties::isNumeric(MyDeviceProperties::kText));
    CHECK(!MyDeviceProperties::isNumeric(MyDeviceProperties::kLayer));
    CHECK(!MyDeviceProperties::isNumeric(MyDeviceProperties::kFont));
}

TEST(Stage2_Properties_GetReturnsTextValues)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    typedef MyDeviceProperties P;
    CHECK_WSTR(P::getString(*pDevice, MyDevice::kText1, P::kText).kACharPtr(), L"Насос");
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText1, P::kHeight), 7.5, kTol);
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText1, P::kPositionX), 2.0, kTol);
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText1, P::kPositionY), 40.0, kTol);
    CHECK_WSTR(P::getString(*pDevice, MyDevice::kText1, P::kLayer).kACharPtr(), L"LABELS");
    CHECK_WSTR(P::getString(*pDevice, MyDevice::kText1, P::kFont).kACharPtr(), L"GOST");

    CHECK_WSTR(P::getString(*pDevice, MyDevice::kText2, P::kText).kACharPtr(), L"Н-1");
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText2, P::kHeight), 3.5, kTol);
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText2, P::kPositionX), 60.0, kTol);
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText2, P::kPositionY), -8.0, kTol);
    CHECK_WSTR(P::getString(*pDevice, MyDevice::kText2, P::kLayer).kACharPtr(), L"NOTES");
    CHECK_WSTR(P::getString(*pDevice, MyDevice::kText2, P::kFont).kACharPtr(), L"Standard");

    // Неверный номер текста или тип свойства.
    CHECK(P::getString(*pDevice, 2, P::kText).isEmpty());
    CHECK(P::getString(*pDevice, MyDevice::kText1, P::kHeight).isEmpty());
    CHECK_NEAR(P::getDouble(*pDevice, -1, P::kHeight), 0.0, kTol);
    CHECK_NEAR(P::getDouble(*pDevice, MyDevice::kText1, P::kText), 0.0, kTol);
}

TEST(Stage2_Properties_SetChangesOnlyThatPropertyAndRedraws)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    typedef MyDeviceProperties P;
    const MyDeviceText text1 = pDevice->textAt(MyDevice::kText1);

    CHECK_EQ(P::setString(*pDevice, MyDevice::kText2, P::kText, L"Н-2"), Acad::eOk);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText2, P::kHeight, 5.0), Acad::eOk);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText2, P::kPositionX, 12.0), Acad::eOk);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText2, P::kPositionY, 4.0), Acad::eOk);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText2, P::kLayer, L"LABELS"), Acad::eOk);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText2, P::kFont, L"GOST"), Acad::eOk);

    CHECK_SAME_TEXT(pDevice->textAt(MyDevice::kText2),
                    makeText(L"Н-2", 5.0, 12.0, 4.0, L"LABELS", L"GOST"));
    CHECK_SAME_TEXT(pDevice->textAt(MyDevice::kText1), text1);

    // Изображение строится из новых значений.
    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (wd.texts.size() == 2)
    {
        CHECK_WSTR(wd.texts[1].message, L"Н-2");
        CHECK_POINT(wd.texts[1].position, AcGePoint3d(112.0, 54.0, 0.0));
        CHECK_NEAR(wd.texts[1].height, 5.0, kTol);
        CHECK_WSTR(wd.texts[1].styleName, L"GOST");
        CHECK(wd.texts[1].layerId == findLayerId(L"LABELS"));
    }
}

TEST(Stage2_Properties_SetRejectsInvalidValues)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    typedef MyDeviceProperties P;
    const MyDeviceText text1 = pDevice->textAt(MyDevice::kText1);
    const double nan = std::numeric_limits<double>::quiet_NaN();

    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText1, P::kHeight, 0.0), Acad::eInvalidInput);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText1, P::kHeight, -2.0), Acad::eInvalidInput);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText1, P::kPositionX, nan), Acad::eInvalidInput);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText1, P::kText, 1.0), Acad::eInvalidInput);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kHeight, L"1"), Acad::eInvalidInput);
    CHECK_EQ(P::setString(*pDevice, 2, P::kText, L"x"), Acad::eInvalidInput);
    CHECK_EQ(P::setDouble(*pDevice, -1, P::kHeight, 1.0), Acad::eInvalidInput);
    // Шрифт — только существующий текстовый стиль.
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kFont, L"NO_SUCH_STYLE"), Acad::eKeyNotFound);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kFont, L""), Acad::eKeyNotFound);
    // Недопустимое имя слоя: слой не создаётся.
    const size_t layers = acdbHostApplicationServices()->workingDatabase()->layerTable.records.size();
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kLayer, L"A<B"), Acad::eInvalidInput);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kLayer, L"A|B"), Acad::eInvalidInput);
    CHECK_EQ(acdbHostApplicationServices()->workingDatabase()->layerTable.records.size(), layers);

    CHECK_SAME_TEXT(pDevice->textAt(MyDevice::kText1), text1);
}

TEST(Stage2_Properties_SetLayerCreatesMissingLayer)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    typedef MyDeviceProperties P;
    CHECK(findLayerId(L"NEW LABELS").isNull());

    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kLayer, L"NEW LABELS"), Acad::eOk);
    CHECK_WSTR(pDevice->textAt(MyDevice::kText1).layer.kACharPtr(), L"NEW LABELS");
    CHECK(!findLayerId(L"NEW LABELS").isNull());
    CHECK_EQ(pDb->lastLayerTableMode, AcDb::kForWrite);

    RecordingWorldDraw wd;
    pDevice->worldDraw(&wd);
    CHECK_EQ(wd.texts.size(), static_cast<size_t>(2));
    if (!wd.texts.empty())
        CHECK(wd.texts[0].layerId == findLayerId(L"NEW LABELS"));

    // Существующий слой (в любом регистре) и пустое имя не создают записей.
    const int addCalls = pDb->layerTable.addCalls;
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText2, P::kLayer, L"new labels"), Acad::eOk);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText2, P::kLayer, L""), Acad::eOk);
    CHECK_EQ(pDb->layerTable.addCalls, addCalls);
    CHECK(pDevice->textAt(MyDevice::kText2).layer.isEmpty());
}

TEST(Stage2_Properties_ObjectOutsideDatabaseUsesWorkingDatabase)
{
    mock::reset();
    MyDevice device;
    typedef MyDeviceProperties P;
    CHECK(device.database() == nullptr);
    CHECK_EQ(P::setString(device, MyDevice::kText1, P::kLayer, L"OUTSIDE"), Acad::eOk);
    CHECK(!findLayerId(L"OUTSIDE").isNull());
    CHECK_EQ(P::setString(device, MyDevice::kText1, P::kFont, L"Standard"), Acad::eOk);
    CHECK(P::databaseOf(device) == acdbHostApplicationServices()->workingDatabase());
}

TEST(Stage2_Properties_EnsureLayer)
{
    mock::reset();
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    typedef MyDeviceProperties P;
    CHECK_EQ(P::ensureLayer(pDb, L""), Acad::eOk);
    CHECK_EQ(pDb->layerTable.addCalls, 0);
    CHECK_EQ(P::ensureLayer(pDb, L"0"), Acad::eOk);
    CHECK_EQ(pDb->layerTable.addCalls, 0);
    CHECK_EQ(P::ensureLayer(pDb, L"Электрика"), Acad::eOk);
    CHECK_EQ(pDb->layerTable.addCalls, 1);
    CHECK(!findLayerId(L"Электрика").isNull());
    CHECK_EQ(P::ensureLayer(pDb, L"Электрика"), Acad::eOk);
    CHECK_EQ(pDb->layerTable.addCalls, 1);
    CHECK_EQ(P::ensureLayer(pDb, L" lead"), Acad::eInvalidInput);
    CHECK_EQ(P::ensureLayer(pDb, L"a*b"), Acad::eInvalidInput);
    CHECK_EQ(pDb->layerTable.addCalls, 1);
    CHECK_EQ(P::ensureLayer(nullptr, L"X"), Acad::eNullObjectPointer);
}

TEST(Stage2_Properties_LayerAndTextStyleLists)
{
    mock::reset();
    AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase();
    pDb->mockAddLayer(L"LABELS");
    pDb->mockAddTextStyle(L"GOST");
    pDb->mockAddTextStyle(L"SHAPES", true);
    typedef MyDeviceProperties P;

    std::vector<AcString> layers;
    CHECK_EQ(P::layerNames(pDb, layers), Acad::eOk);
    CHECK_EQ(layers.size(), static_cast<size_t>(2));
    if (layers.size() == 2)
    {
        CHECK_WSTR(layers[0].kACharPtr(), L"0");
        CHECK_WSTR(layers[1].kACharPtr(), L"LABELS");
    }

    std::vector<AcString> styles;
    CHECK_EQ(P::textStyleNames(pDb, styles), Acad::eOk);
    CHECK_EQ(styles.size(), static_cast<size_t>(2));
    if (styles.size() == 2)
    {
        CHECK_WSTR(styles[0].kACharPtr(), L"Standard");
        CHECK_WSTR(styles[1].kACharPtr(), L"GOST");
    }
    // Чтение списков не меняет чертёж.
    CHECK_EQ(pDb->lastLayerTableMode, AcDb::kForRead);
    CHECK_EQ(pDb->lastTextStyleTableMode, AcDb::kForRead);

    styles.push_back(L"stale");
    CHECK_EQ(P::textStyleNames(nullptr, styles), Acad::eNullObjectPointer);
    CHECK(styles.empty());
    CHECK_EQ(P::layerNames(nullptr, layers), Acad::eNullObjectPointer);
    CHECK(layers.empty());
}

TEST(Stage2_Properties_ChangesSurviveSaveAndReopen)
{
    mock::reset();
    MyDevice* pDevice = appendStyledDevice();
    typedef MyDeviceProperties P;
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText1, P::kLayer, L"CREATED"), Acad::eOk);
    CHECK_EQ(P::setDouble(*pDevice, MyDevice::kText1, P::kPositionY, 22.0), Acad::eOk);
    CHECK_EQ(P::setString(*pDevice, MyDevice::kText2, P::kFont, L"GOST"), Acad::eOk);

    MemoryDwgFiler dwg;
    CHECK_EQ(pDevice->dwgOutFields(&dwg), Acad::eOk);
    AcRxObject* pObj = MyDevice::desc()->create();
    MyDevice* pReopened = MyDevice::cast(pObj);
    dwg.rewind();
    CHECK_EQ(pReopened->dwgInFields(&dwg), Acad::eOk);
    CHECK_WSTR(P::getString(*pReopened, MyDevice::kText1, P::kLayer).kACharPtr(), L"CREATED");
    CHECK_NEAR(P::getDouble(*pReopened, MyDevice::kText1, P::kPositionY), 22.0, kTol);
    CHECK_WSTR(P::getString(*pReopened, MyDevice::kText2, P::kFont).kACharPtr(), L"GOST");
    delete pObj;
}

int main()
{
    mock::reset();
    if (acrxEntryPoint(AcRx::kInitAppMsg, nullptr) != AcRx::kRetOK)
    {
        std::printf("kInitAppMsg failed\n");
        return 1;
    }
    return testing::runAll();
}
