// MyDeviceProperties.h — свойства текстов MyDevice для палитры свойств.
//
// Здесь собрано всё, что палитре свойств нужно знать о свойствах, кроме COM:
// список свойств (категория «Text 1»/«Text 2», имя, описание), чтение и запись
// значений, списки слоёв и текстовых стилей чертежа и создание отсутствующего слоя.
// COM-обёртка для AutoCAD находится в MyDeviceOPM.cpp; этот файл не зависит
// от COM и проверяется модульными тестами.
#pragma once

#include "StdAfx.h"
#include "MyDevice.h"

#include <vector>

class MyDeviceProperties
{
public:
    // Свойство одного текста.
    enum Field
    {
        kText = 0,    // содержимое (строка)
        kHeight,      // высота (число)
        kPositionX,   // локальная X (число)
        kPositionY,   // локальная Y (число)
        kLayer,       // имя слоя (строка); отсутствующий слой создаётся
        kFont,        // имя текстового стиля (выбор из списка стилей чертежа)
        kFieldCount
    };

    struct Descriptor
    {
        int textIndex;             // MyDevice::kText1 или MyDevice::kText2
        Field field;
        const ACHAR* category;     // «Text 1» или «Text 2»
        const ACHAR* name;         // имя в палитре
        const ACHAR* description;  // подсказка в палитре
    };

    // Все свойства: сначала шесть свойств Text1, затем шесть свойств Text2.
    static int count();
    static const Descriptor& at(int index);

    // Числовое ли свойство (высота и координаты).
    static bool isNumeric(Field field);

    // Чтение значения. Для строкового свойства getDouble() возвращает 0,
    // для числового getString() — пустую строку.
    static AcString getString(const MyDevice& device, int textIndex, Field field);
    static double getDouble(const MyDevice& device, int textIndex, Field field);

    // Запись значения. Объект должен быть открыт на запись.
    //  * eInvalidInput — неверный номер текста, тип свойства или значение
    //    (высота <= 0, недопустимое имя слоя и т. п.);
    //  * eKeyNotFound — текстового стиля с таким именем нет в чертеже.
    // Для свойства Layer отсутствующий слой создаётся в базе данных объекта.
    // Пустое имя слоя означает «слой самого MyDevice».
    static Acad::ErrorStatus setString(MyDevice& device, int textIndex, Field field,
                                       const AcString& value);
    static Acad::ErrorStatus setDouble(MyDevice& device, int textIndex, Field field,
                                       double value);

    // Имена слоёв и текстовых стилей чертежа в порядке таблицы.
    // Стили-описания форм (SHX shape files) в список не входят: ими нельзя писать текст.
    static Acad::ErrorStatus layerNames(AcDbDatabase* pDb, std::vector<AcString>& names);
    static Acad::ErrorStatus textStyleNames(AcDbDatabase* pDb, std::vector<AcString>& names);

    // Создаёт слой name, если его нет. Пустое имя — ничего не делать.
    static Acad::ErrorStatus ensureLayer(AcDbDatabase* pDb, const AcString& name);

    // База данных объекта, а если он ещё не добавлен в чертёж — рабочая.
    static AcDbDatabase* databaseOf(const MyDevice& device);
};
