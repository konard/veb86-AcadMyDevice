// arx_mock.cpp — реализация имитации ObjectARX для модульных тестов.
#include "arx_mock.h"

#include <cwctype>
#include <memory>

const AcGeVector3d AcGeVector3d::kIdentity(0.0, 0.0, 0.0);
const AcGeVector3d AcGeVector3d::kXAxis(1.0, 0.0, 0.0);
const AcGeVector3d AcGeVector3d::kYAxis(0.0, 1.0, 0.0);
const AcGeVector3d AcGeVector3d::kZAxis(0.0, 0.0, 1.0);
const AcGePoint3d AcGePoint3d::kOrigin(0.0, 0.0, 0.0);

// ---------------------------------------------------------------------------
// Состояние имитации
// ---------------------------------------------------------------------------

namespace mock
{
    int getPointResult = RTNORM;
    AcGePoint3d getPointValue;
    std::wstring lastPrompt;
    AcGeMatrix3d currentUcs;
    std::vector<std::wstring> printed;
    int mdiAwareCalls = 0;
    int unlockCalls = 0;
    int buildHierarchyCalls = 0;
}

namespace
{
    // Резидентные записи таблиц символов — для поиска объекта по идентификатору.
    // Объявлен раньше рабочей базы: при выходе база удаляется первой и ещё обращается к нему.
    std::map<intptr_t, AcDbObject*> g_objects;
    // Идентификаторы уникальны во всех базах, как в AutoCAD.
    intptr_t g_nextObjectId = 1;
    std::unique_ptr<AcDbDatabase> g_workingDatabase;

    AcDbObjectId newObjectId()
    {
        return AcDbObjectId(g_nextObjectId++);
    }

    bool sameSymbolName(const std::wstring& a, const std::wstring& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::towupper(a[i]) != std::towupper(b[i]))
                return false;
        return true;
    }
    AcEdCommandStack g_commandStack;
    AcDbHostApplicationServices g_hostServices;

    std::map<std::wstring, AcRxClass*>& classRegistry()
    {
        static std::map<std::wstring, AcRxClass*> registry;
        if (registry.empty())
        {
            AcRxClass* rxObject = new AcRxClass(L"AcRxObject", nullptr, 0, 0, 0, nullptr, nullptr, nullptr);
            AcRxClass* dbObject = new AcRxClass(L"AcDbObject", rxObject, 0, 0, 0, nullptr, nullptr, nullptr);
            AcRxClass* dbEntity = new AcRxClass(L"AcDbEntity", dbObject, 0, 0, 0, nullptr, nullptr, nullptr);
            registry[rxObject->name()] = rxObject;
            registry[dbObject->name()] = dbObject;
            registry[dbEntity->name()] = dbEntity;
        }
        return registry;
    }

    // Приводит формат acutPrintf к стандартному vswprintf:
    // %s в acutPrintf означает строку ACHAR (в glibc это %ls),
    // %q0 — вещественное число в текущих единицах (заменяется на %g).
    std::wstring toStdFormat(const wchar_t* format)
    {
        std::wstring out;
        for (const wchar_t* p = format; *p; ++p)
        {
            out += *p;
            if (*p != L'%')
                continue;
            ++p;
            if (*p == L'%')
            {
                out += *p;
                continue;
            }
            while (*p && std::wcschr(L"-+ #0123456789.", *p))
                out += *p++;
            if (*p == L's')
                out += L"ls";
            else if (*p == L'q')
            {
                out += L'g';
                if (p[1] >= L'0' && p[1] <= L'9')
                    ++p;
            }
            else if (*p)
                out += *p;
            else
                break;
        }
        return out;
    }
}

void mock::reset()
{
    g_workingDatabase.reset(new AcDbDatabase());
    getPointResult = RTNORM;
    getPointValue = AcGePoint3d::kOrigin;
    lastPrompt.clear();
    currentUcs.setToIdentity();
    printed.clear();
    mdiAwareCalls = 0;
    unlockCalls = 0;
    buildHierarchyCalls = 0;
}

// ---------------------------------------------------------------------------
// ADS / acedXXX
// ---------------------------------------------------------------------------

int acutPrintf(const ACHAR* format, ...)
{
    wchar_t buffer[4096];
    va_list args;
    va_start(args, format);
    const int n = std::vswprintf(buffer, sizeof(buffer) / sizeof(buffer[0]),
                                 toStdFormat(format).c_str(), args);
    va_end(args);
    mock::printed.push_back(n >= 0 ? buffer : L"<format error>");
    return n;
}

int acedGetPoint(const ads_point /*pt*/, const ACHAR* prompt, ads_point result)
{
    mock::lastPrompt = prompt ? prompt : L"";
    if (mock::getPointResult == RTNORM)
    {
        result[0] = mock::getPointValue.x;
        result[1] = mock::getPointValue.y;
        result[2] = mock::getPointValue.z;
    }
    return mock::getPointResult;
}

Acad::ErrorStatus acedGetCurrentUCS(AcGeMatrix3d& mat)
{
    mat = mock::currentUcs;
    return Acad::eOk;
}

const ACHAR* acadErrorStatusText(Acad::ErrorStatus /*es*/)
{
    return L"<error>";
}

// ---------------------------------------------------------------------------
// AcRx
// ---------------------------------------------------------------------------

AcRxClass* mockFindClass(const ACHAR* name)
{
    auto& registry = classRegistry();
    auto it = registry.find(name);
    return it == registry.end() ? nullptr : it->second;
}

AcRxClass* newAcRxClass(const ACHAR* className, const ACHAR* parentClassName,
                        int dwgVer, int maintVer, int proxyFlags, AcRxCreateFunc create,
                        const ACHAR* dxfName, const ACHAR* appName)
{
    AcRxClass* parent = mockFindClass(parentClassName);
    if (parent == nullptr || mockFindClass(className) != nullptr)
        return nullptr;
    AcRxClass* cls = new AcRxClass(className, parent, dwgVer, maintVer, proxyFlags,
                                   create, dxfName, appName);
    classRegistry()[className] = cls;
    return cls;
}

void deleteAcRxClass(AcRxClass* pClassObj)
{
    if (pClassObj == nullptr)
        return;
    classRegistry().erase(pClassObj->name());
    delete pClassObj;
}

void acrxBuildClassHierarchy()
{
    mock::buildHierarchyCalls++;
}

bool acrxRegisterAppMDIAware(void* /*appId*/)
{
    mock::mdiAwareCalls++;
    return true;
}

bool acrxUnlockApplication(void* /*appId*/)
{
    mock::unlockCalls++;
    return true;
}

// ---------------------------------------------------------------------------
// AcDbObject / AcDbEntity: общие данные, которые пишет базовый класс.
// ---------------------------------------------------------------------------

Acad::ErrorStatus AcDbObject::dwgInFields(AcDbDwgFiler* pFiler) { return pFiler->filerStatus(); }
Acad::ErrorStatus AcDbObject::dwgOutFields(AcDbDwgFiler* pFiler) const { return pFiler->filerStatus(); }
Acad::ErrorStatus AcDbObject::dxfInFields(AcDbDxfFiler* pFiler) { return pFiler->filerStatus(); }
Acad::ErrorStatus AcDbObject::dxfOutFields(AcDbDxfFiler* pFiler) const { return pFiler->filerStatus(); }

AcRxClass* AcDbEntity::desc()
{
    return mockFindClass(L"AcDbEntity");
}

Acad::ErrorStatus AcDbEntity::dwgOutFields(AcDbDwgFiler* pFiler) const
{
    pFiler->writeString(m_layer);
    return pFiler->filerStatus();
}

Acad::ErrorStatus AcDbEntity::dwgInFields(AcDbDwgFiler* pFiler)
{
    pFiler->readString(m_layer);
    return pFiler->filerStatus();
}

Acad::ErrorStatus AcDbEntity::dxfOutFields(AcDbDxfFiler* pFiler) const
{
    pFiler->writeItem(AcDb::kDxfSubclass, L"AcDbEntity");
    pFiler->writeString(AcDb::kDxfLayerName, m_layer);
    return pFiler->filerStatus();
}

Acad::ErrorStatus AcDbEntity::dxfInFields(AcDbDxfFiler* pFiler)
{
    if (!pFiler->atSubclassData(L"AcDbEntity"))
        return pFiler->filerStatus();

    resbuf rb;
    while (pFiler->readResBuf(&rb) == Acad::eOk)
    {
        if (rb.restype == AcDb::kDxfLayerName)
        {
            m_layer = rb.resval.rstring;
            continue;
        }
        // Начало следующего подкласса или чужая группа.
        pFiler->pushBackItem();
        break;
    }
    return pFiler->filerStatus();
}

// ---------------------------------------------------------------------------
// База данных
// ---------------------------------------------------------------------------

AcDbBlockTableRecord::~AcDbBlockTableRecord()
{
    for (AcDbEntity* pEntity : entities)
        delete pEntity;
}

Acad::ErrorStatus AcDbBlockTableRecord::appendAcDbEntity(AcDbObjectId& id, AcDbEntity* pEntity)
{
    if (pEntity == nullptr)
        return Acad::eInvalidInput;
    entities.push_back(pEntity);
    id = newObjectId();
    pEntity->mockAttach(database(), id);
    return Acad::eOk;
}

// ---------------------------------------------------------------------------
// Таблицы символов
// ---------------------------------------------------------------------------

Acad::ErrorStatus AcDbSymbolUtilities::Services::validateSymbolName(const ACHAR* name,
                                                                    bool allowVerticalBar) const
{
    if (name == nullptr || *name == L'\0')
        return Acad::eInvalidInput;
    const std::wstring str(name);
    if (str.find_first_of(L"<>/\\\":;?*,=`") != std::wstring::npos)
        return Acad::eInvalidInput;
    if (!allowVerticalBar && str.find(L'|') != std::wstring::npos)
        return Acad::eInvalidInput;
    // Имя не может начинаться или заканчиваться пробелом.
    if (str.front() == L' ' || str.back() == L' ')
        return Acad::eInvalidInput;
    return Acad::eOk;
}

const AcDbSymbolUtilities::Services* acdbSymUtil()
{
    static AcDbSymbolUtilities::Services services;
    return &services;
}

Acad::ErrorStatus AcDbSymbolTableRecord::setName(const ACHAR* pName)
{
    if (acdbSymUtil()->validateSymbolName(pName, false) != Acad::eOk)
        return Acad::eInvalidInput;
    m_name = pName;
    return Acad::eOk;
}

AcDbSymbolTable::~AcDbSymbolTable()
{
    for (AcDbSymbolTableRecord* pRecord : records)
    {
        g_objects.erase(pRecord->objectId().value());
        delete pRecord;
    }
}

Acad::ErrorStatus AcDbSymbolTable::getAt(const ACHAR* entryName, AcDbObjectId& recordId,
                                         bool /*getErasedRecord*/) const
{
    if (entryName == nullptr)
        return Acad::eInvalidInput;
    for (const AcDbSymbolTableRecord* pRecord : records)
    {
        if (sameSymbolName(pRecord->mockName(), entryName))
        {
            recordId = pRecord->objectId();
            return Acad::eOk;
        }
    }
    return Acad::eKeyNotFound;
}

Acad::ErrorStatus AcDbSymbolTable::addRecord(AcDbSymbolTableRecord* pRecord)
{
    addCalls++;
    if (pRecord == nullptr || pRecord->mockName().empty())
        return Acad::eInvalidInput;
    if (has(pRecord->mockName().c_str()))
        return Acad::eDuplicateRecordName;
    pRecord->mockAttach(m_pDb, newObjectId());
    records.push_back(pRecord);
    g_objects[pRecord->objectId().value()] = pRecord;
    return Acad::eOk;
}

Acad::ErrorStatus fromAcDbTextStyle(AcGiTextStyle& style, const AcDbObjectId& styleId)
{
    auto it = g_objects.find(styleId.value());
    AcDbTextStyleTableRecord* pRecord =
        it == g_objects.end() ? nullptr : dynamic_cast<AcDbTextStyleTableRecord*>(it->second);
    if (pRecord == nullptr)
        return Acad::eInvalidInput;
    style.setStyleName(pRecord->mockName().c_str());
    return Acad::eOk;
}

// ---------------------------------------------------------------------------
// AcDbEntity: слой
// ---------------------------------------------------------------------------

void AcDbEntity::setDatabaseDefaults(AcDbDatabase* pDb)
{
    m_pDefaultsDb = pDb;
    if (pDb)
        m_layer = pDb->clayer;
}

AcDbObjectId AcDbEntity::layerId() const
{
    AcDbDatabase* pDb = database() ? database() : m_pDefaultsDb;
    AcDbObjectId id;
    if (pDb == nullptr || pDb->layerTable.getAt(m_layer.kACharPtr(), id) != Acad::eOk)
        return AcDbObjectId();
    return id;
}

Acad::ErrorStatus AcDbBlockTable::getAt(const ACHAR* entryName, AcDbBlockTableRecord*& pRec,
                                        AcDb::OpenMode openMode, bool /*openErasedRec*/) const
{
    if (std::wcscmp(entryName, ACDB_MODEL_SPACE) != 0 || modelSpace == nullptr)
        return Acad::eKeyNotFound;
    lastOpenMode = openMode;
    pRec = modelSpace;
    return Acad::eOk;
}

AcDbDatabase::AcDbDatabase()
    : layerTable(this)
    , textStyleTable(this)
    , clayer(L"0")
{
    blockTable.modelSpace = &modelSpace;
    modelSpace.mockAttach(this, newObjectId());
    mockAddLayer(L"0");
    mockAddTextStyle(L"Standard");
}

Acad::ErrorStatus AcDbDatabase::getLayerTable(AcDbLayerTable*& pTable, AcDb::OpenMode mode)
{
    lastLayerTableMode = mode;
    pTable = &layerTable;
    return Acad::eOk;
}

Acad::ErrorStatus AcDbDatabase::getTextStyleTable(AcDbTextStyleTable*& pTable, AcDb::OpenMode mode)
{
    lastTextStyleTableMode = mode;
    pTable = &textStyleTable;
    return Acad::eOk;
}

AcDbObjectId AcDbDatabase::mockAddLayer(const ACHAR* name)
{
    AcDbLayerTableRecord* pRecord = new AcDbLayerTableRecord();
    if (pRecord->setName(name) != Acad::eOk || layerTable.add(pRecord) != Acad::eOk)
    {
        delete pRecord;
        return AcDbObjectId();
    }
    layerTable.addCalls--;  // служебное добавление не считается
    return pRecord->objectId();
}

AcDbObjectId AcDbDatabase::mockAddTextStyle(const ACHAR* name, bool isShapeFile)
{
    AcDbTextStyleTableRecord* pRecord = new AcDbTextStyleTableRecord();
    pRecord->setIsShapeFile(isShapeFile);
    if (pRecord->setName(name) != Acad::eOk || textStyleTable.add(pRecord) != Acad::eOk)
    {
        delete pRecord;
        return AcDbObjectId();
    }
    textStyleTable.addCalls--;
    return pRecord->objectId();
}

AcDbDatabase::~AcDbDatabase()
{
}

Acad::ErrorStatus AcDbDatabase::getBlockTable(AcDbBlockTable*& pTable, AcDb::OpenMode /*mode*/)
{
    pTable = &blockTable;
    return Acad::eOk;
}

AcDbDatabase* AcDbHostApplicationServices::workingDatabase() const
{
    return g_workingDatabase.get();
}

AcDbHostApplicationServices* acdbHostApplicationServices()
{
    return &g_hostServices;
}

// ---------------------------------------------------------------------------
// Команды
// ---------------------------------------------------------------------------

Acad::ErrorStatus AcEdCommandStack::addCommand(const ACHAR* cmdGroupName, const ACHAR* cmdGlobalName,
                                               const ACHAR* cmdLocalName, Adesk::Int32 commandFlags,
                                               AcRxFunctionPtr functionAddr)
{
    if (lookupGlobalCmd(cmdGlobalName) != nullptr)
        return Acad::eInvalidInput;
    commands.push_back(Command{ cmdGroupName, cmdGlobalName, cmdLocalName, commandFlags, functionAddr });
    return Acad::eOk;
}

Acad::ErrorStatus AcEdCommandStack::removeGroup(const ACHAR* groupName)
{
    std::vector<Command> kept;
    for (const Command& cmd : commands)
        if (cmd.group != groupName)
            kept.push_back(cmd);
    const bool removed = kept.size() != commands.size();
    commands.swap(kept);
    return removed ? Acad::eOk : Acad::eKeyNotFound;
}

const AcEdCommandStack::Command* AcEdCommandStack::lookupGlobalCmd(const ACHAR* cmdName) const
{
    for (const Command& cmd : commands)
        if (cmd.globalName == cmdName)
            return &cmd;
    return nullptr;
}

AcEdCommandStack* mockCommandStack()
{
    return &g_commandStack;
}
