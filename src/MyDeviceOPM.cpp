// MyDeviceOPM.cpp — динамические свойства MyDevice для палитры свойств AutoCAD.
//
// Каждое из 12 свойств — отдельный COM-объект, реализующий IDynamicProperty
// (имя, тип, чтение и запись значения) и ICategorizeProperties (категория
// «Text 1»/«Text 2»). Свойство Font дополнительно реализует IDynamicEnumProperty:
// палитра показывает раскрывающийся список текстовых стилей чертежа.
// Layer — строка: можно ввести имя любого слоя, отсутствующий слой создаётся.
//
// После записи значения объект закрывается, AutoCAD перерисовывает его,
// а IDynamicPropertyNotify::OnChanged() обновляет палитру.
#include "StdAfx.h"
#include "MyDeviceOPM.h"
#include "MyDevice.h"
#include "MyDeviceProperties.h"

#include <ole2.h>

#include "acdocman.h"
#include "dbobjptr.h"
#include "dynprops.h"
#include "category.h"

#include <vector>

namespace
{
    // Категории палитры: положительные номера принадлежат приложению.
    const PROPCAT kFirstCategory = 1001;  // «Text 1»; «Text 2» — kFirstCategory + 1

    // GUID свойства index: базовый GUID, последний байт которого равен номеру свойства.
    GUID propertyGuid(int index)
    {
        // {0A80016C-9B74-4E72-9854-5AF3DD408A00}
        GUID guid = { 0x0a80016c, 0x9b74, 0x4e72,
                      { 0x98, 0x54, 0x5a, 0xf3, 0xdd, 0x40, 0x8a, 0x00 } };
        guid.Data4[7] = static_cast<unsigned char>(index);
        return guid;
    }

    HRESULT allocString(const ACHAR* value, BSTR* pResult)
    {
        if (pResult == nullptr)
            return E_POINTER;
        *pResult = ::SysAllocString(value != nullptr ? value : L"");
        return *pResult != nullptr ? S_OK : E_OUTOFMEMORY;
    }

    AcDbObjectId objectIdOf(LONG_PTR objectID)
    {
        AcDbObjectId id;
        id.setFromOldId(static_cast<Adesk::IntDbId>(objectID));
        return id;
    }

    class MyDeviceDynamicProperty : public IDynamicProperty,
                                    public IDynamicEnumProperty,
                                    public ICategorizeProperties
    {
    public:
        explicit MyDeviceDynamicProperty(int index)
            : m_refCount(1), m_index(index), m_descriptor(MyDeviceProperties::at(index)),
              m_pNotify(nullptr)
        {
        }

        virtual ~MyDeviceDynamicProperty()
        {
            if (m_pNotify != nullptr)
                m_pNotify->Release();
        }

        // *** IUnknown ***
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override
        {
            if (ppvObject == nullptr)
                return E_POINTER;
            *ppvObject = nullptr;
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IDynamicProperty))
                *ppvObject = static_cast<IDynamicProperty*>(this);
            else if (riid == __uuidof(ICategorizeProperties))
                *ppvObject = static_cast<ICategorizeProperties*>(this);
            else if (riid == __uuidof(IDynamicEnumProperty) && isFont())
                *ppvObject = static_cast<IDynamicEnumProperty*>(this);
            else
                return E_NOINTERFACE;
            AddRef();
            return S_OK;
        }

        STDMETHODIMP_(ULONG) AddRef() override
        {
            return static_cast<ULONG>(::InterlockedIncrement(&m_refCount));
        }

        STDMETHODIMP_(ULONG) Release() override
        {
            const LONG count = ::InterlockedDecrement(&m_refCount);
            if (count == 0)
                delete this;
            return static_cast<ULONG>(count);
        }

        // *** IDynamicProperty ***
        STDMETHODIMP GetGUID(GUID* propGUID) override
        {
            if (propGUID == nullptr)
                return E_POINTER;
            *propGUID = propertyGuid(m_index);
            return S_OK;
        }

        STDMETHODIMP GetDisplayName(BSTR* bstrName) override
        {
            return allocString(m_descriptor.name, bstrName);
        }

        STDMETHODIMP IsPropertyEnabled(LONG_PTR /*objectID*/, BOOL* pbEnabled) override
        {
            if (pbEnabled == nullptr)
                return E_POINTER;
            *pbEnabled = TRUE;
            return S_OK;
        }

        STDMETHODIMP IsPropertyReadOnly(BOOL* pbReadonly) override
        {
            if (pbReadonly == nullptr)
                return E_POINTER;
            *pbReadonly = FALSE;
            return S_OK;
        }

        STDMETHODIMP GetDescription(BSTR* bstrName) override
        {
            return allocString(m_descriptor.description, bstrName);
        }

        STDMETHODIMP GetCurrentValueName(BSTR* /*pbstrName*/) override
        {
            return E_NOTIMPL;
        }

        STDMETHODIMP GetCurrentValueType(VARTYPE* pVarType) override
        {
            if (pVarType == nullptr)
                return E_POINTER;
            *pVarType = MyDeviceProperties::isNumeric(m_descriptor.field) ? VT_R8 : VT_BSTR;
            return S_OK;
        }

        STDMETHODIMP GetCurrentValueData(LONG_PTR objectID, VARIANT* pvarData) override
        {
            if (pvarData == nullptr)
                return E_POINTER;
            AcDbObjectPointer<MyDevice> pDevice(objectIdOf(objectID), AcDb::kForRead);
            if (pDevice.openStatus() != Acad::eOk)
                return E_FAIL;

            ::VariantInit(pvarData);
            if (MyDeviceProperties::isNumeric(m_descriptor.field))
            {
                V_VT(pvarData) = VT_R8;
                V_R8(pvarData) = MyDeviceProperties::getDouble(*pDevice, m_descriptor.textIndex,
                                                                m_descriptor.field);
                return S_OK;
            }
            const AcString value = MyDeviceProperties::getString(*pDevice, m_descriptor.textIndex,
                                                                 m_descriptor.field);
            V_VT(pvarData) = VT_BSTR;
            return allocString(value.kACharPtr(), &V_BSTR(pvarData));
        }

        STDMETHODIMP SetCurrentValueData(LONG_PTR objectID, const VARIANT varData) override
        {
            const AcDbObjectId id = objectIdOf(objectID);
            // Палитра может вызвать запись вне команды: документ блокируется на время записи.
            AcApDocument* pDoc = id.database() != nullptr ? acDocManager->document(id.database())
                                                          : nullptr;
            const bool locked = pDoc != nullptr
                && acDocManager->lockDocument(pDoc, AcAp::kWrite) == Acad::eOk;
            const HRESULT hr = setValue(id, varData);
            if (locked)
                acDocManager->unlockDocument(pDoc);

            if (SUCCEEDED(hr) && m_pNotify != nullptr)
                m_pNotify->OnChanged(this);
            return hr;
        }

        STDMETHODIMP Connect(IDynamicPropertyNotify* pSink) override
        {
            if (pSink != nullptr)
                pSink->AddRef();
            if (m_pNotify != nullptr)
                m_pNotify->Release();
            m_pNotify = pSink;
            return S_OK;
        }

        STDMETHODIMP Disconnect() override
        {
            if (m_pNotify != nullptr)
            {
                m_pNotify->Release();
                m_pNotify = nullptr;
            }
            return S_OK;
        }

        // *** IDynamicEnumProperty (только Font) ***
        STDMETHODIMP GetNumPropertyValues(LONG* numValues) override
        {
            if (numValues == nullptr)
                return E_POINTER;
            // Список перечитывается при каждом открытии: стили могли добавить или удалить.
            MyDeviceProperties::textStyleNames(
                acdbHostApplicationServices()->workingDatabase(), m_styleNames);
            *numValues = static_cast<LONG>(m_styleNames.size());
            return S_OK;
        }

        STDMETHODIMP GetPropValueName(LONG index, BSTR* valueName) override
        {
            if (index < 0 || static_cast<size_t>(index) >= m_styleNames.size())
                return E_INVALIDARG;
            return allocString(m_styleNames[index].kACharPtr(), valueName);
        }

        STDMETHODIMP GetPropValueData(LONG index, VARIANT* valueData) override
        {
            if (valueData == nullptr)
                return E_POINTER;
            if (index < 0 || static_cast<size_t>(index) >= m_styleNames.size())
                return E_INVALIDARG;
            ::VariantInit(valueData);
            V_VT(valueData) = VT_BSTR;
            return allocString(m_styleNames[index].kACharPtr(), &V_BSTR(valueData));
        }

        // *** ICategorizeProperties ***
        STDMETHODIMP MapPropertyToCategory(DISPID /*dispid*/, PROPCAT* ppropcat) override
        {
            if (ppropcat == nullptr)
                return E_POINTER;
            *ppropcat = kFirstCategory + m_descriptor.textIndex;
            return S_OK;
        }

        STDMETHODIMP GetCategoryName(PROPCAT propcat, LCID /*lcid*/, BSTR* pbstrName) override
        {
            const int textIndex = propcat - kFirstCategory;
            if (textIndex < 0 || textIndex >= MyDevice::kTextCount)
                return E_INVALIDARG;
            return allocString(MyDeviceProperties::at(textIndex * MyDeviceProperties::kFieldCount)
                                   .category, pbstrName);
        }

    private:
        bool isFont() const { return m_descriptor.field == MyDeviceProperties::kFont; }

        HRESULT setValue(const AcDbObjectId& id, const VARIANT& varData)
        {
            const bool numeric = MyDeviceProperties::isNumeric(m_descriptor.field);
            VARIANT value;
            ::VariantInit(&value);
            if (FAILED(::VariantChangeType(&value, const_cast<VARIANT*>(&varData), 0,
                                           numeric ? VT_R8 : VT_BSTR)))
                return E_INVALIDARG;

            HRESULT hr = E_FAIL;
            AcDbObjectPointer<MyDevice> pDevice(id, AcDb::kForWrite);
            if (pDevice.openStatus() == Acad::eOk)
            {
                const Acad::ErrorStatus es = numeric
                    ? MyDeviceProperties::setDouble(*pDevice, m_descriptor.textIndex,
                                                    m_descriptor.field, V_R8(&value))
                    : MyDeviceProperties::setString(*pDevice, m_descriptor.textIndex,
                                                    m_descriptor.field,
                                                    AcString(V_BSTR(&value) != nullptr
                                                             ? V_BSTR(&value) : L""));
                if (es == Acad::eOk)
                {
                    hr = S_OK;
                }
                else
                {
                    acutPrintf(_T("\nMyDevice: недопустимое значение свойства %s (%s)."),
                               m_descriptor.name, m_descriptor.category);
                    hr = E_INVALIDARG;
                }
            }
            ::VariantClear(&value);
            return hr;
        }

        LONG m_refCount;
        const int m_index;
        const MyDeviceProperties::Descriptor& m_descriptor;
        IDynamicPropertyNotify* m_pNotify;
        std::vector<AcString> m_styleNames;
    };

    // Свойства, добавленные в палитру; по одной ссылке на каждое хранит приложение.
    std::vector<MyDeviceDynamicProperty*> g_properties;

    // Менеджер свойств класса MyDevice или nullptr, если палитра недоступна.
    // Возвращённый указатель нужно освободить через Release().
    IPropertyManager* propertyManager()
    {
        OPMPropertyExtensionFactory* pFactory = GET_OPMEXTENSION_CREATE_PROTOCOL();
        if (pFactory == nullptr)
            return nullptr;
        OPMPropertyExtension* pExtension = pFactory->CreateOPMObjectProtocol(MyDevice::desc());
        return pExtension != nullptr ? pExtension->GetPropertyManager() : nullptr;
    }
}

bool registerMyDeviceProperties()
{
    if (!g_properties.empty())
        return true;
    IPropertyManager* pManager = propertyManager();
    if (pManager == nullptr)
        return false;
    for (int i = 0; i < MyDeviceProperties::count(); ++i)
    {
        MyDeviceDynamicProperty* pProperty = new MyDeviceDynamicProperty(i);
        if (SUCCEEDED(pManager->AddProperty(pProperty)))
            g_properties.push_back(pProperty);
        else
            pProperty->Release();
    }
    pManager->Release();
    return !g_properties.empty();
}

void unregisterMyDeviceProperties()
{
    IPropertyManager* pManager = propertyManager();
    for (MyDeviceDynamicProperty* pProperty : g_properties)
    {
        if (pManager != nullptr)
            pManager->RemoveProperty(pProperty);
        pProperty->Release();
    }
    g_properties.clear();
    if (pManager != nullptr)
        pManager->Release();
}
