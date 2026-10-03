// MyDevice.h — пользовательский объект MyDevice (наследник AcDbEntity).
//
// MyDevice хранит точку вставки, ориентацию и два текста (Text1, Text2)
// и сам рисует прямоугольник 100x50 и оба текста в subWorldDraw(),
// не создавая отдельных AcDbText/AcDbMText.
#pragma once

#include "StdAfx.h"

class MyDevice : public AcDbEntity
{
public:
    ACRX_DECLARE_MEMBERS(MyDevice);

    // Версия формата данных MyDevice в DWG/DXF.
    // Увеличивать при каждом изменении набора сохраняемых полей.
    static constexpr Adesk::Int16 kCurrentVersion = 1;

    // Геометрия в локальной системе координат объекта (единицы чертежа).
    static const double kWidth;       // ширина прямоугольника
    static const double kHeight;      // высота прямоугольника
    static const double kTextHeight;  // высота текста
    static const double kTextMargin;  // отступ текста от левого края

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

    AcGePoint3d  m_position;
    AcGeVector3d m_xDirection;
    AcGeVector3d m_normal;
    double       m_scale;
    AcString     m_text1;
    AcString     m_text2;
};
