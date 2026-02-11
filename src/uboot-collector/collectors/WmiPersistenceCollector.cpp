#include "WmiPersistenceCollector.h"
#include "../util/CommandResolver.h"
#include <windows.h>
#include <wbemidl.h>
#include <oleauto.h>
#include <algorithm>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace uboot {

CollectorResult WmiPersistenceCollector::Collect() {
    CollectorResult result;
    
    // Initialize COM
    HRESULT comInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninitialize = (comInit == S_OK);
    
    EnumerateWmiFilters(result.entries, result.errors);
    
    if (shouldUninitialize) CoUninitialize();
    
    // Sort entries deterministically
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b) {
        if (a.key != b.key) return a.key < b.key;
        return a.arguments < b.arguments;
    });
    
    return result;
}

void WmiPersistenceCollector::EnumerateWmiFilters(
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    // Initialize COM security
    HRESULT hr = CoInitializeSecurity(
        nullptr,
        -1,
        nullptr,
        nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE,
        nullptr
    );
    
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        outErrors.push_back(CollectorError(GetName(), "Failed to initialize COM security", (int)hr));
        return;
    }
    
    // Create locator
    IWbemLocator* pLocator = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&pLocator);
    if (FAILED(hr)) {
        outErrors.push_back(CollectorError(GetName(), "Failed to create WbemLocator", (int)hr));
        return;
    }
    
    // Connect to WMI
    IWbemServices* pServices = nullptr;
    hr = pLocator->ConnectServer(
        _bstr_t(L"ROOT\\subscription"),
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
        &pServices
    );
    
    if (FAILED(hr)) {
        outErrors.push_back(CollectorError(GetName(), "Failed to connect to WMI", (int)hr));
        pLocator->Release();
        return;
    }
    
    // Create enumerator for CommandLineEventConsumer instances
    IEnumWbemClassObject* pEnumerator = nullptr;
    hr = pServices->CreateInstanceEnum(
        _bstr_t(L"CommandLineEventConsumer"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnumerator
    );
    
    if (SUCCEEDED(hr)) {
        ULONG returned = 0;
        IWbemClassObject* pObject = nullptr;
        
        while (SUCCEEDED(pEnumerator->Next(WBEM_INFINITE, 1, &pObject, &returned)) && returned) {
            VARIANT vtName;
            VariantInit(&vtName);
            VARIANT vtCommand;
            VariantInit(&vtCommand);
            
            // Get Name property
            hr = pObject->Get(L"Name", 0, &vtName, nullptr, nullptr);
            
            // Get CommandLineTemplate property
            hr = pObject->Get(L"CommandLineTemplate", 0, &vtCommand, nullptr, nullptr);
            
            if (SUCCEEDED(hr) && vtCommand.vt == VT_BSTR) {
                std::string filterName;
                if (vtName.vt == VT_BSTR) {
                    int nameLen = WideCharToMultiByte(CP_UTF8, 0, vtName.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                    filterName.resize(nameLen - 1);
                    WideCharToMultiByte(CP_UTF8, 0, vtName.bstrVal, -1, &filterName[0], nameLen, nullptr, nullptr);
                }
                
                int cmdLen = WideCharToMultiByte(CP_UTF8, 0, vtCommand.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                std::string command(cmdLen - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, vtCommand.bstrVal, -1, &command[0], cmdLen, nullptr, nullptr);
                
                // Create Entry
                Entry entry(
                    GetName(),       // source = "WmiPersistence"
                    "Machine",       // scope = "Machine"
                    filterName,      // key = filter name
                    "root\\subscription\\CommandLineEventConsumer", // location
                    "",              // arguments
                    ""               // imagePath
                );
                
                // Resolve command
                CommandResolver::PopulateEntryCommand(entry, command, "");
                
                outEntries.push_back(entry);
            }
            
            VariantClear(&vtName);
            VariantClear(&vtCommand);
            pObject->Release();
        }
        
        pEnumerator->Release();
    }
    
    pServices->Release();
    pLocator->Release();
}

} // namespace uboot
