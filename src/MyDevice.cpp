// MyDevice.cpp — реализация пользовательского объекта MyDevice.
#include "StdAfx.h"
#include "MyDevice.h"

ACRX_DXF_DEFINE_MEMBERS(
    MyDevice, AcDbEntity,
    AcDb::kDHL_CURRENT, AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kNoOperation, MYDEVICE,
    MyDeviceApp
    |Product Desc: MyDevice custom entity
    |Company: veb86
)

const double MyDevice::kWidth = 100.0;
const double MyDevice::kHeight = 50.0;
const double MyDevice::kTextHeight = 10.0;
const double MyDevice::kTextMargin = 5.0;

namespace
{
    // Базовые линии текстов в локальной системе координат объекта.
    const double kText1BaselineY = 30.0;
    const double kText2BaselineY = 10.0;

    const double kTolerance = 1.0e-10;

    // Групповые коды DXF подкласса MyDevice.
    const AcDb::DxfCode kDxfVersion = AcDb::kDxfInt16;                                   // 70
    const AcDb::DxfCode kDxfPosition = AcDb::kDxfXCoord;                                 // 10
    const AcDb::DxfCode kDxfXDirection = static_cast<AcDb::DxfCode>(AcDb::kDxfXCoord + 1); // 11
    const AcDb::DxfCode kDxfScale = AcDb::kDxfReal;                                      // 40
    const AcDb::DxfCode kDxfNormal = AcDb::kDxfNormalX;                                  // 210
    const AcDb::DxfCode kDxfText1 = AcDb::kDxfXTextString;                               // 300
    const AcDb::DxfCode kDxfText2 = static_cast<AcDb::DxfCode>(AcDb::kDxfXTextString + 1); // 301

    const ACHAR kDxfSubclassName[] = _T("MyDevice");

    AcGePoint3d asPoint(const resbuf& rb)
    {
        return AcGePoint3d(rb.resval.rpoint[0], rb.resval.rpoint[1], rb.resval.rpoint[2]);
    }

    AcGeVector3d asVector(const resbuf& rb)
    {
        return AcGeVector3d(rb.resval.rpoint[0], rb.resval.rpoint[1], rb.resval.rpoint[2]);
    }

    // Приводит пару (ось X, нормаль) к ортонормированному виду.
    // Возвращает false, если векторы вырождены или параллельны.
    bool normalizeOrientation(AcGeVector3d& xDirection, AcGeVector3d& normal)
    {
        const double normalLength = normal.length();
        if (normalLength < kTolerance)
            return false;
        normal *= 1.0 / normalLength;

        // Убираем из оси X составляющую вдоль нормали.
        xDirection -= normal * xDirection.dotProduct(normal);
        const double xLength = xDirection.length();
        if (xLength < kTolerance)
            return false;
        xDirection *= 1.0 / xLength;
        return true;
    }
}

MyDevice::MyDevice()
    : m_position(AcGePoint3d::kOrigin)
    , m_xDirection(AcGeVector3d::kXAxis)
    , m_normal(AcGeVector3d::kZAxis)
    , m_scale(1.0)
    , m_text1(_T("TEXT1"))
    , m_text2(_T("TEXT2"))
{
}

MyDevice::MyDevice(const AcGePoint3d& position)
    : MyDevice()
{
    m_position = position;
}

MyDevice::~MyDevice()
{
}

AcGePoint3d MyDevice::position() const
{
    assertReadEnabled();
    return m_position;
}

Acad::ErrorStatus MyDevice::setPosition(const AcGePoint3d& position)
{
    assertWriteEnabled();
    m_position = position;
    return Acad::eOk;
}

AcGeVector3d MyDevice::xDirection() const
{
    assertReadEnabled();
    return m_xDirection;
}

AcGeVector3d MyDevice::normal() const
{
    assertReadEnabled();
    return m_normal;
}

Acad::ErrorStatus MyDevice::setOrientation(const AcGeVector3d& xDirection,
                                           const AcGeVector3d& normal)
{
    AcGeVector3d x = xDirection;
    AcGeVector3d n = normal;
    if (!normalizeOrientation(x, n))
        return Acad::eInvalidInput;

    assertWriteEnabled();
    m_xDirection = x;
    m_normal = n;
    return Acad::eOk;
}

double MyDevice::scale() const
{
    assertReadEnabled();
    return m_scale;
}

Acad::ErrorStatus MyDevice::setScale(double scale)
{
    if (!(scale > kTolerance))
        return Acad::eInvalidInput;

    assertWriteEnabled();
    m_scale = scale;
    return Acad::eOk;
}

AcString MyDevice::text1() const
{
    assertReadEnabled();
    return m_text1;
}

Acad::ErrorStatus MyDevice::setText1(const AcString& text)
{
    assertWriteEnabled();
    m_text1 = text;
    return Acad::eOk;
}

AcString MyDevice::text2() const
{
    assertReadEnabled();
    return m_text2;
}

Acad::ErrorStatus MyDevice::setText2(const AcString& text)
{
    assertWriteEnabled();
    m_text2 = text;
    return Acad::eOk;
}

AcGeMatrix3d MyDevice::localToWorld() const
{
    assertReadEnabled();
    const AcGeVector3d yDirection = m_normal.crossProduct(m_xDirection);
    AcGeMatrix3d xform;
    xform.setCoordSystem(m_position,
                         m_xDirection * m_scale,
                         yDirection * m_scale,
                         m_normal * m_scale);
    return xform;
}

void MyDevice::getCorners(AcGePoint3d corners[4]) const
{
    const AcGeMatrix3d xform = localToWorld();
    corners[0].set(0.0, 0.0, 0.0);
    corners[1].set(kWidth, 0.0, 0.0);
    corners[2].set(kWidth, kHeight, 0.0);
    corners[3].set(0.0, kHeight, 0.0);
    for (int i = 0; i < 4; ++i)
        corners[i].transformBy(xform);
}

// ---------------------------------------------------------------------------
// DWG
// ---------------------------------------------------------------------------

Acad::ErrorStatus MyDevice::dwgOutFields(AcDbDwgFiler* pFiler) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = AcDbEntity::dwgOutFields(pFiler);
    if (es != Acad::eOk)
        return es;

    // Версия всегда записывается первой.
    pFiler->writeInt16(kCurrentVersion);
    pFiler->writePoint3d(m_position);
    pFiler->writeVector3d(m_xDirection);
    pFiler->writeVector3d(m_normal);
    pFiler->writeDouble(m_scale);
    pFiler->writeString(m_text1);
    pFiler->writeString(m_text2);

    return pFiler->filerStatus();
}

Acad::ErrorStatus MyDevice::dwgInFields(AcDbDwgFiler* pFiler)
{
    assertWriteEnabled();

    Acad::ErrorStatus es = AcDbEntity::dwgInFields(pFiler);
    if (es != Acad::eOk)
        return es;

    Adesk::Int16 version = 0;
    if ((es = pFiler->readInt16(&version)) != Acad::eOk)
        return es;
    // Объект сохранён более новой версией приложения — пусть станет прокси.
    if (version > kCurrentVersion || version < 1)
        return Acad::eMakeMeProxy;

    pFiler->readPoint3d(&m_position);
    pFiler->readVector3d(&m_xDirection);
    pFiler->readVector3d(&m_normal);
    pFiler->readDouble(&m_scale);
    pFiler->readString(m_text1);
    pFiler->readString(m_text2);

    return pFiler->filerStatus();
}

// ---------------------------------------------------------------------------
// DXF
// ---------------------------------------------------------------------------

Acad::ErrorStatus MyDevice::dxfOutFields(AcDbDxfFiler* pFiler) const
{
    assertReadEnabled();

    Acad::ErrorStatus es = AcDbEntity::dxfOutFields(pFiler);
    if (es != Acad::eOk)
        return es;

    pFiler->writeItem(AcDb::kDxfSubclass, kDxfSubclassName);
    pFiler->writeInt16(kDxfVersion, kCurrentVersion);
    pFiler->writePoint3d(kDxfPosition, m_position);
    pFiler->writeVector3d(kDxfXDirection, m_xDirection, 16);
    pFiler->writeDouble(kDxfScale, m_scale);
    // Нормаль всегда пишется с максимальной точностью.
    pFiler->writeVector3d(kDxfNormal, m_normal, 16);
    pFiler->writeString(kDxfText1, m_text1);
    pFiler->writeString(kDxfText2, m_text2);

    return pFiler->filerStatus();
}

Acad::ErrorStatus MyDevice::dxfInFields(AcDbDxfFiler* pFiler)
{
    assertWriteEnabled();

    if (AcDbEntity::dxfInFields(pFiler) != Acad::eOk
        || !pFiler->atSubclassData(kDxfSubclassName))
    {
        return pFiler->filerStatus();
    }

    // Значения по умолчанию на случай отсутствия необязательных групп.
    Adesk::Int16 version = kCurrentVersion;
    AcGePoint3d position = AcGePoint3d::kOrigin;
    AcGeVector3d xDirection = AcGeVector3d::kXAxis;
    AcGeVector3d normal = AcGeVector3d::kZAxis;
    double scale = 1.0;
    AcString text1(_T("TEXT1"));
    AcString text2(_T("TEXT2"));

    Acad::ErrorStatus es = Acad::eOk;
    resbuf rb;
    while (es == Acad::eOk && (es = pFiler->readResBuf(&rb)) == Acad::eOk)
    {
        switch (rb.restype)
        {
        case kDxfVersion:
            version = rb.resval.rint;
            break;
        case kDxfPosition:
            position = asPoint(rb);
            break;
        case kDxfXDirection:
            xDirection = asVector(rb);
            break;
        case kDxfScale:
            scale = rb.resval.rreal;
            break;
        case kDxfNormal:
            normal = asVector(rb);
            break;
        case kDxfText1:
            text1 = rb.resval.rstring;
            break;
        case kDxfText2:
            text2 = rb.resval.rstring;
            break;
        default:
            // Чужая группа — возвращаем её, чтобы её прочитал следующий подкласс.
            pFiler->pushBackItem();
            es = Acad::eEndOfFile;
            break;
        }
    }

    // Здесь es должен быть eEndOfFile — либо от readResBuf(), либо от pushBackItem().
    if (es != Acad::eEndOfFile)
        return Acad::eInvalidResBuf;

    if (version > kCurrentVersion || version < 1)
        return Acad::eMakeMeProxy;

    if (!normalizeOrientation(xDirection, normal))
    {
        pFiler->setError(Acad::eInvalidDxfCode,
                         _T("\nMyDevice: invalid direction or normal vector."));
        return pFiler->filerStatus();
    }
    if (!(scale > kTolerance))
    {
        pFiler->setError(Acad::eInvalidDxfCode,
                         _T("\nMyDevice: invalid scale %g."), scale);
        return pFiler->filerStatus();
    }

    m_position = position;
    m_xDirection = xDirection;
    m_normal = normal;
    m_scale = scale;
    m_text1 = text1;
    m_text2 = text2;

    return pFiler->filerStatus();
}

// ---------------------------------------------------------------------------
// Графика и редактирование
// ---------------------------------------------------------------------------

Adesk::Boolean MyDevice::subWorldDraw(AcGiWorldDraw* pWd)
{
    assertReadEnabled();

    if (pWd->regenAbort())
        return Adesk::kTrue;

    // Прямоугольник 100x50: замкнутая полилиния из 5 точек.
    AcGePoint3d outline[5];
    getCorners(outline);
    outline[4] = outline[0];
    pWd->geometry().polyline(5, outline, &m_normal);

    // Тексты рисуются примитивами AcGi, без создания AcDbText/AcDbMText.
    const AcGeMatrix3d xform = localToWorld();
    const double textHeight = kTextHeight * m_scale;

    AcGePoint3d text1Position(kTextMargin, kText1BaselineY, 0.0);
    text1Position.transformBy(xform);
    pWd->geometry().text(text1Position, m_normal, m_xDirection,
                         textHeight, 1.0, 0.0, m_text1.kACharPtr());

    AcGePoint3d text2Position(kTextMargin, kText2BaselineY, 0.0);
    text2Position.transformBy(xform);
    pWd->geometry().text(text2Position, m_normal, m_xDirection,
                         textHeight, 1.0, 0.0, m_text2.kACharPtr());

    return Adesk::kTrue;
}

void MyDevice::subList() const
{
    assertReadEnabled();
    AcDbEntity::subList();

    acutPrintf(_T("%18s%16s X = %-9.16q0, Y = %-9.16q0, Z = %-9.16q0\n"),
               _T(""), _T("Insertion point:"),
               m_position.x, m_position.y, m_position.z);
    acutPrintf(_T("%18s%16s %s\n"), _T(""), _T("Text1:"), m_text1.kACharPtr());
    acutPrintf(_T("%18s%16s %s\n"), _T(""), _T("Text2:"), m_text2.kACharPtr());
}

Acad::ErrorStatus MyDevice::subTransformBy(const AcGeMatrix3d& xform)
{
    // Неравномерное масштабирование исказило бы текст — запрещаем его.
    if (!xform.isUniScaledOrtho())
        return Acad::eCannotScaleNonUniformly;

    AcGeVector3d xAxis = m_xDirection * m_scale;
    AcGeVector3d yAxis = m_normal.crossProduct(m_xDirection) * m_scale;
    xAxis.transformBy(xform);
    yAxis.transformBy(xform);

    const double newScale = xAxis.length();
    if (newScale < kTolerance)
        return Acad::eInvalidInput;

    assertWriteEnabled();
    m_position.transformBy(xform);
    m_xDirection = xAxis * (1.0 / newScale);
    // При зеркалировании нормаль меняет знак, так что геометрия отражается корректно.
    m_normal = xAxis.crossProduct(yAxis).normal();
    m_scale = newScale;
    return Acad::eOk;
}

Acad::ErrorStatus MyDevice::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();

    AcGePoint3d corners[4];
    getCorners(corners);
    for (int i = 0; i < 4; ++i)
        extents.addPoint(corners[i]);
    return Acad::eOk;
}

Acad::ErrorStatus MyDevice::subGetGripPoints(AcGePoint3dArray& gripPoints,
                                             AcDbIntArray& /*osnapModes*/,
                                             AcDbIntArray& /*geomIds*/) const
{
    assertReadEnabled();
    // Одна ручка в точке вставки — перемещает весь объект.
    gripPoints.append(m_position);
    return Acad::eOk;
}

Acad::ErrorStatus MyDevice::subMoveGripPointsAt(const AcDbIntArray& indices,
                                                const AcGeVector3d& offset)
{
    if (indices.length() == 0)
        return Acad::eOk;

    assertWriteEnabled();
    m_position += offset;
    return Acad::eOk;
}
