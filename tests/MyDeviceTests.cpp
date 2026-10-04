// MyDeviceTests.cpp — модульные тесты MyDevice на имитации ObjectARX.
//
// Проверяются требования этапа 1:
//  * регистрация класса MyDevice (наследник AcDbEntity) и команды MYDEVICE;
//  * значения по умолчанию Text1 = "TEXT1", Text2 = "TEXT2";
//  * отрисовка прямоугольника 100x50 и двух текстов самим объектом
//    (без создания AcDbText/AcDbMText);
//  * сохранение и восстановление DWG и DXF;
//  * команда MYDEVICE создаёт объект в пространстве модели в указанной точке.
#include "StdAfx.h"
#include "MyDevice.h"
#include "MyDeviceCommand.h"
#include "mock_filers.h"
#include "test_framework.h"

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

    CHECK_EQ(acrxEntryPoint(AcRx::kUnloadAppMsg, nullptr), AcRx::kRetOK);
    CHECK(mockFindClass(L"MyDevice") == nullptr);
    CHECK(acedRegCmds->lookupGlobalCmd(L"MYDEVICE") == nullptr);

    // Возвращаем приложение в загруженное состояние для остальных тестов.
    CHECK_EQ(acrxEntryPoint(AcRx::kInitAppMsg, nullptr), AcRx::kRetOK);
    CHECK(MyDevice::desc() != nullptr);
    CHECK(acedRegCmds->lookupGlobalCmd(L"MYDEVICE") != nullptr);
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
