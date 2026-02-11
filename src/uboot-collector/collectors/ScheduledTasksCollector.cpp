#include "ScheduledTasksCollector.h"
#include "../util/CommandResolver.h"
#include <windows.h>
#include <taskschd.h>
#include <oleauto.h>
#include <ocidl.h>
#include <algorithm>
#include <sstream>

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace uboot {

CollectorResult ScheduledTasksCollector::Collect() {
    CollectorResult result;
    
    // Initialize COM
    HRESULT comInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool shouldUninitialize = (comInit == S_OK);
    
    // Create Task Service object
    ITaskService* taskService = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, 
                                  IID_ITaskService, (void**)&taskService);
    if (FAILED(hr)) {
        result.errors.push_back(CollectorError(GetName(), 
            "Failed to create TaskScheduler COM object", (int)hr));
        if (shouldUninitialize) CoUninitialize();
        return result;
    }
    
    // Connect to Task Scheduler
    hr = taskService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) {
        result.errors.push_back(CollectorError(GetName(), 
            "Failed to connect to Task Scheduler", (int)hr));
        taskService->Release();
        if (shouldUninitialize) CoUninitialize();
        return result;
    }
    
    // Get root task folder
    ITaskFolder* rootFolder = nullptr;
    hr = taskService->GetFolder(_bstr_t(L"\\"), &rootFolder);
    if (FAILED(hr)) {
        result.errors.push_back(CollectorError(GetName(), 
            "Failed to get root task folder", (int)hr));
        taskService->Release();
        if (shouldUninitialize) CoUninitialize();
        return result;
    }
    
    // Enumerate all tasks recursively
    EnumerateTaskFolder(rootFolder, "\\", result.entries, result.errors);
    
    rootFolder->Release();
    taskService->Release();
    
    if (shouldUninitialize) CoUninitialize();
    
    // Sort entries deterministically
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b) {
        if (a.location != b.location) return a.location < b.location;
        if (a.key != b.key) return a.key < b.key;
        return a.arguments < b.arguments;
    });
    
    return result;
}

void ScheduledTasksCollector::EnumerateTaskFolder(
    IDispatch* folderDispatch,
    const std::string& folderPath,
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    if (!folderDispatch) return;
    
    ITaskFolder* folder = nullptr;
    HRESULT hr = folderDispatch->QueryInterface(IID_ITaskFolder, (void**)&folder);
    if (FAILED(hr)) {
        return;
    }
    
    // Get tasks in this folder
    IRegisteredTaskCollection* taskCollection = nullptr;
    hr = folder->GetTasks(TASK_ENUM_HIDDEN, &taskCollection);
    if (SUCCEEDED(hr) && taskCollection) {
        long count = 0;
        taskCollection->get_Count(&count);
        
        for (long i = 1; i <= count; i++) {
            IRegisteredTask* task = nullptr;
            hr = taskCollection->Item(_variant_t(i), &task);
            if (SUCCEEDED(hr) && task) {
                // Get task name
                BSTR taskName = nullptr;
                task->get_Name(&taskName);
                
                int nameLen = WideCharToMultiByte(CP_UTF8, 0, taskName, -1, nullptr, 0, nullptr, nullptr);
                std::string taskNameUtf8(nameLen - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, taskName, -1, &taskNameUtf8[0], nameLen, nullptr, nullptr);
                
                std::string fullTaskName = folderPath;
                if (fullTaskName.back() != '\\') fullTaskName += "\\";
                fullTaskName += taskNameUtf8;
                
                // Extract actions from this task
                IDispatch* taskDispatch = nullptr;
                task->QueryInterface(IID_IDispatch, (void**)&taskDispatch);
                if (taskDispatch) {
                    ExtractTaskActions(taskNameUtf8, folderPath, taskDispatch, outEntries, outErrors);
                    taskDispatch->Release();
                }
                
                SysFreeString(taskName);
                task->Release();
            }
        }
        taskCollection->Release();
    }
    
    // Get subfolders
    ITaskFolderCollection* subFolders = nullptr;
    hr = folder->GetFolders(0, &subFolders);
    if (SUCCEEDED(hr) && subFolders) {
        long count = 0;
        subFolders->get_Count(&count);
        
        for (long i = 1; i <= count; i++) {
            ITaskFolder* subFolder = nullptr;
            hr = subFolders->Item(_variant_t(i), &subFolder);
            if (SUCCEEDED(hr) && subFolder) {
                // Get subfolder name
                BSTR folderName = nullptr;
                subFolder->get_Name(&folderName);
                
                int nameLen = WideCharToMultiByte(CP_UTF8, 0, folderName, -1, nullptr, 0, nullptr, nullptr);
                std::string subFolderName(nameLen - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, folderName, -1, &subFolderName[0], nameLen, nullptr, nullptr);
                
                std::string subFolderPath = folderPath;
                if (subFolderPath.back() != '\\') subFolderPath += "\\";
                subFolderPath += subFolderName;
                
                IDispatch* subFolderDispatch = nullptr;
                subFolder->QueryInterface(IID_IDispatch, (void**)&subFolderDispatch);
                if (subFolderDispatch) {
                    EnumerateTaskFolder(subFolderDispatch, subFolderPath, outEntries, outErrors);
                    subFolderDispatch->Release();
                }
                
                SysFreeString(folderName);
                subFolder->Release();
            }
        }
        subFolders->Release();
    }
    
    folder->Release();
}

void ScheduledTasksCollector::ExtractTaskActions(
    const std::string& taskName,
    const std::string& folderPath,
    IDispatch* taskDispatch,
    std::vector<Entry>& outEntries,
    std::vector<CollectorError>& outErrors
) {
    if (!taskDispatch) return;
    
    // Get Definition property from task
    DISPID dispid = 0;
    LPOLESTR propName = L"Definition";
    HRESULT hr = taskDispatch->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return;
    
    DISPPARAMS params = {};
    VARIANT result;
    VariantInit(&result);
    
    hr = taskDispatch->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &result, nullptr, nullptr);
    if (FAILED(hr) || result.vt != VT_DISPATCH) {
        VariantClear(&result);
        return;
    }
    
    IDispatch* definition = result.pdispVal;
    
    // Get Actions from Definition
    propName = L"Actions";
    hr = definition->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
    if (SUCCEEDED(hr)) {
        VariantInit(&result);
        hr = definition->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &result, nullptr, nullptr);
        if (SUCCEEDED(hr) && result.vt == VT_DISPATCH) {
            IDispatch* actions = result.pdispVal;
            
            // Count actions
            propName = L"Count";
            hr = actions->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
            long actionCount = 0;
            if (SUCCEEDED(hr)) {
                VARIANT countResult;
                VariantInit(&countResult);
                hr = actions->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &countResult, nullptr, nullptr);
                if (SUCCEEDED(hr)) {
                    actionCount = countResult.intVal;
                }
                VariantClear(&countResult);
            }
            
            // Iterate actions
            for (long i = 1; i <= actionCount; i++) {
                DISPID itemDispid = DISPID_VALUE;
                DISPPARAMS itemParams = {};
                VARIANT itemIndex;
                VariantInit(&itemIndex);
                itemIndex.vt = VT_I4;
                itemIndex.intVal = i;
                itemParams.rgvarg = &itemIndex;
                itemParams.cArgs = 1;
                
                VARIANT actionResult;
                VariantInit(&actionResult);
                hr = actions->Invoke(itemDispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &itemParams, &actionResult, nullptr, nullptr);
                
                if (SUCCEEDED(hr) && actionResult.vt == VT_DISPATCH) {
                    IDispatch* action = actionResult.pdispVal;
                    
                    // Get action Type
                    propName = L"Type";
                    hr = action->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
                    int actionType = 0;
                    if (SUCCEEDED(hr)) {
                        VARIANT typeResult;
                        VariantInit(&typeResult);
                        hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &typeResult, nullptr, nullptr);
                        if (SUCCEEDED(hr)) {
                            actionType = typeResult.intVal;
                        }
                        VariantClear(&typeResult);
                    }
                    
                    // Only process ExecAction (type 0)
                    if (actionType == 0) {
                        // Get Path
                        propName = L"Path";
                        hr = action->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
                        std::string execPath;
                        if (SUCCEEDED(hr)) {
                            VARIANT pathResult;
                            VariantInit(&pathResult);
                            hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &pathResult, nullptr, nullptr);
                            if (SUCCEEDED(hr)) {
                                execPath = VariantToString(pathResult);
                            }
                            VariantClear(&pathResult);
                        }
                        
                        // Get Arguments
                        propName = L"Arguments";
                        hr = action->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
                        std::string arguments;
                        if (SUCCEEDED(hr)) {
                            VARIANT argsResult;
                            VariantInit(&argsResult);
                            hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &argsResult, nullptr, nullptr);
                            if (SUCCEEDED(hr)) {
                                arguments = VariantToString(argsResult);
                            }
                            VariantClear(&argsResult);
                        }
                        
                        // Get WorkingDirectory
                        propName = L"WorkingDirectory";
                        hr = action->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT, &dispid);
                        std::string workingDir;
                        if (SUCCEEDED(hr)) {
                            VARIANT wdResult;
                            VariantInit(&wdResult);
                            hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &params, &wdResult, nullptr, nullptr);
                            if (SUCCEEDED(hr)) {
                                workingDir = VariantToString(wdResult);
                            }
                            VariantClear(&wdResult);
                        }
                        
                        // Create Entry
                        Entry entry(
                            GetName(),      // source = "ScheduledTasks"
                            "Machine",      // scope = "Machine"
                            taskName,       // key = task name
                            folderPath,     // location = folder path
                            arguments,      // arguments
                            ""              // imagePath (to be resolved)
                        );
                        
                        // Resolve command and path
                        CommandResolver::PopulateEntryCommand(entry, execPath, workingDir);
                        
                        outEntries.push_back(entry);
                    }
                    
                    action->Release();
                }
                
                VariantClear(&itemIndex);
            }
            
            actions->Release();
        }
        VariantClear(&result);
    }
    
    definition->Release();
}

std::string ScheduledTasksCollector::VariantToString(const VARIANT& var) {
    std::string result;
    
    if (var.vt == VT_BSTR && var.bstrVal) {
        int len = WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, nullptr, 0, nullptr, nullptr);
        result.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, &result[0], len, nullptr, nullptr);
    }
    
    return result;
}

} // namespace uboot
