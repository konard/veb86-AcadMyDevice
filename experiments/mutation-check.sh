#!/usr/bin/env bash
# Убеждается, что модульные тесты ловят типичные ошибки: вносит в копию исходников
# (build/src) по одной мутации и проверяет, что тесты падают.
# Требует предварительного: cmake -S tests -B build
set -u
cd "$(dirname "$0")/.."
B=build/src
mutate() {  # file sed-expr description
    cp "$B/$1" "$B/$1.orig"
    sed -i "$2" "$B/$1"
    if cmp -s "$B/$1" "$B/$1.orig"; then
        echo "NOT APPLIED: $3"; status=1
    elif cmake --build build >/dev/null 2>&1 && ./build/MyDeviceTests >/dev/null; then
        echo "SURVIVED:    $3"; status=1
    else
        echo "killed:      $3"
    fi
    mv "$B/$1.orig" "$B/$1"
    touch "$B/$1"
}
status=0
mutate MyDevice.cpp '235s/m_text2/m_text1/' "Text2 читается в Text1 (DWG)"
mutate MyDevice.cpp '234,235{s/m_text1/TMP/;s/m_text2/m_text1/;s/TMP/m_text2/}' "порядок Text1/Text2 в dwgIn"
mutate MyDevice.cpp 's/m_text1(_T("TEXT1"))/m_text1(_T("TXT1"))/;s/m_text1(L"TEXT1")/m_text1(L"TXT1")/' "значение по умолчанию Text1"
mutate MyDevice.cpp '185s/kHeight/kWidth/' "размер прямоугольника"
mutate MyDevice.cpp '313s/pFiler->pushBackItem();//' "нет pushBackItem в dxfIn"
mutate MyDevice.cpp '398s/!xform.isUniScaledOrtho()/false/' "нет проверки неравномерного масштаба"
mutate MyDeviceCommand.cpp '30s/es = pModelSpace->appendAcDbEntity(entityId, pEntity);/es = Acad::eOk;/' "объект не добавляется в модель"
cmake --build build >/dev/null
exit $status
