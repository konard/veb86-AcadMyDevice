// MyDevice.h — пользовательский объект MyDevice (наследник AcDbEntity).
//
// MyDevice хранит точку вставки, ориентацию и два текста (Text1, Text2)
// и сам рисует прямоугольник 100x50 и оба текста в subWorldDraw(),
// не создавая отдельных AcDbText/AcDbMText.
#pragma once

#include "StdAfx.h"

// Параметры одного текста MyDevice.
// Высота и положение задаются в локальной системе координат объекта:
// при перемещении, повороте и масштабировании MyDevice они не меняются,
// а текст преобразуется вместе с объектом.
struct MyDeviceText
{
    AcString text;       // содержимое
    double   height;     // высота текста
    double   x;          // начало базовой линии, локальная X
    double   y;          // начало базовой линии, локальная Y
    AcString layer;      // имя слоя; пустая строка — слой самого MyDevice
    AcString textStyle;  // имя текстового стиля AutoCAD (шрифт)
};

class MyDevice : public AcDbEntity
{
public:
    ACRX_DECLARE_MEMBERS(MyDevice);

    // Версия формата данных MyDevice в DWG/DXF.
    // Увеличивать при каждом изменении набора сохраняемых полей.
    // 1 — этап 1 (только строки Text1/Text2), 2 — полные свойства текстов.
    static constexpr Adesk::Int16 kCurrentVersion = 2;

    // Геометрия в локальной системе координат объекта (единицы чертежа).
    static const double kWidth;       // ширина прямоугольника
    static const double kHeight;      // высота прямоугольника
    static const double kTextHeight;  // высота текста по умолчанию
    static const double kTextMargin;  // отступ текста от левого края по умолчанию

    // Номера текстов для textAt()/setTextAt().
    enum TextIndex { kText1 = 0, kText2 = 1, kTextCount = 2 };

    // Текстовый стиль по умолчанию — есть в любом чертеже.
    static const ACHAR* const kDefaultTextStyle;

    MyDevice();
    explicit MyDevice(const AcGePoint3d& position);
    ~MyDevice() override;

    // Точка вставки (левый нижний угол прямоугольника) в МСК.
    AcGePoint3d position() const;
    Acad::ErrorStatus setPosition(const AcGePoint3d& position);

    // Направление оси X объекта и нормаль к его плоскости (единичные векторы, МСК).
    AcGeVector3d xDirection() const;
    AcGeVector3d normal() const;
    Acad::ErrorStatus setOrientation(const AcGeVector3d& xDirection,
                                     const AcGeVector3d& normal);

    // Масштаб объекта (изменяется командой SCALE).
    double scale() const;
    Acad::ErrorStatus setScale(double scale);

    AcString text1() const;
    Acad::ErrorStatus setText1(const AcString& text);

    AcString text2() const;
    Acad::ErrorStatus setText2(const AcString& text);

    // Все параметры текста index (kText1 или kText2).
    // setTextAt() отклоняет неверный номер, высоту <= 0, нечисловые координаты
    // и пустое имя стиля (eInvalidInput); объект при этом не меняется.
    // Слой и стиль хранятся по имени и в базе данных не создаются.
    MyDeviceText textAt(int index) const;
    Acad::ErrorStatus setTextAt(int index, const MyDeviceText& data);

    // Параметры текста по умолчанию (как на этапе 1).
    static MyDeviceText defaultText(int index);

    // Матрица перехода из локальной системы координат объекта в МСК.
    AcGeMatrix3d localToWorld() const;

    // Сохранение и загрузка DWG.
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* pFiler) const override;
    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* pFiler) override;

    // Сохранение и загрузка DXF.
    Acad::ErrorStatus dxfOutFields(AcDbDxfFiler* pFiler) const override;
    Acad::ErrorStatus dxfInFields(AcDbDxfFiler* pFiler) override;

protected:
    // Остальные перегрузки базового класса остаются доступными.
    using AcDbEntity::subGetGripPoints;
    using AcDbEntity::subMoveGripPointsAt;

    Adesk::Boolean subWorldDraw(AcGiWorldDraw* pWd) override;
    void subList() const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& xform) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subGetGripPoints(AcGePoint3dArray& gripPoints,
                                       AcDbIntArray& osnapModes,
                                       AcDbIntArray& geomIds) const override;
    Acad::ErrorStatus subMoveGripPointsAt(const AcDbIntArray& indices,
                                          const AcGeVector3d& offset) override;

private:
    // Углы прямоугольника в МСК: [0] — точка вставки, далее против часовой стрелки.
    void getCorners(AcGePoint3d corners[4]) const;

    // Рисует один текст его стилем и на его слое.
    void drawText(AcGiWorldDraw* pWd, AcDbDatabase* pDb, const MyDeviceText& text) const;

    AcGePoint3d  m_position;
    AcGeVector3d m_xDirection;
    AcGeVector3d m_normal;
    double       m_scale;
    MyDeviceText m_texts[kTextCount];
};
