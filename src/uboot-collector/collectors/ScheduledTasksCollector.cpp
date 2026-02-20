#include "ScheduledTasksCollector.h"
#include "../util/ComUtils.h"
#include "../util/CommandResolver.h"
#include <algorithm>
#include <ocidl.h>
#include <oleauto.h>
#include <sstream>
#include <taskschd.h>
#include <windows.h>
#include <wrl/client.h>


#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace uboot {

using Microsoft::WRL::ComPtr;

CollectorResult ScheduledTasksCollector::Collect() {
  CollectorResult result;

  // Initialize COM
  util::ComInit com(COINIT_MULTITHREADED);

  // Create Task Service object
  ComPtr<ITaskService> taskService;
  HRESULT hr =
      CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                       IID_ITaskService, (void **)&taskService);
  if (FAILED(hr)) {
    result.errors.push_back(CollectorError(
        GetName(), "Failed to create TaskScheduler COM object", (int)hr));
    return result;
  }

  // Connect to Task Scheduler
  hr = taskService->Connect(_variant_t(), _variant_t(), _variant_t(),
                            _variant_t());
  if (FAILED(hr)) {
    result.errors.push_back(CollectorError(
        GetName(), "Failed to connect to Task Scheduler", (int)hr));
    return result;
  }

  // Get root task folder
  ComPtr<ITaskFolder> rootFolder;
  hr = taskService->GetFolder(_bstr_t(L"\\"), &rootFolder);
  if (FAILED(hr)) {
    result.errors.push_back(
        CollectorError(GetName(), "Failed to get root task folder", (int)hr));
    return result;
  }

  // Enumerate all tasks recursively
  EnumerateTaskFolder(rootFolder.Get(), "\\", result.entries, result.errors);

  // Sort entries deterministically
  std::sort(result.entries.begin(), result.entries.end(),
            [](const Entry &a, const Entry &b) {
              if (a.location != b.location)
                return a.location < b.location;
              if (a.key != b.key)
                return a.key < b.key;
              return a.arguments < b.arguments;
            });

  return result;
}

void ScheduledTasksCollector::EnumerateTaskFolder(
    IDispatch *folderDispatch, const std::string &folderPath,
    std::vector<Entry> &outEntries, std::vector<CollectorError> &outErrors) {
  if (!folderDispatch)
    return;

  ComPtr<ITaskFolder> folder;
  HRESULT hr =
      folderDispatch->QueryInterface(IID_ITaskFolder, (void **)&folder);
  if (FAILED(hr)) {
    return;
  }

  // Get tasks in this folder
  ComPtr<IRegisteredTaskCollection> taskCollection;
  hr = folder->GetTasks(TASK_ENUM_HIDDEN, &taskCollection);
  if (SUCCEEDED(hr) && taskCollection) {
    long count = 0;
    taskCollection->get_Count(&count);

    for (long i = 1; i <= count; i++) {
      ComPtr<IRegisteredTask> pTask;
      hr = taskCollection->get_Item(_variant_t(i), &pTask);
      if (SUCCEEDED(hr) && pTask) {
        // Get task name
        BSTR taskName = nullptr;
        pTask->get_Name(&taskName);

        int nameLen = WideCharToMultiByte(CP_UTF8, 0, taskName, -1, nullptr, 0,
                                          nullptr, nullptr);
        std::string taskNameUtf8(nameLen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, taskName, -1, &taskNameUtf8[0], nameLen,
                            nullptr, nullptr);

        std::string fullTaskName = folderPath;
        if (fullTaskName.back() != '\\')
          fullTaskName += "\\";
        fullTaskName += taskNameUtf8;

        // Extract actions from this task
        ComPtr<IDispatch> taskDispatch;
        pTask->QueryInterface(IID_IDispatch, (void **)&taskDispatch);
        if (taskDispatch) {
          ExtractTaskActions(taskNameUtf8, folderPath, taskDispatch.Get(),
                             outEntries, outErrors);
        }

        SysFreeString(taskName);
      }
    }
  }

  // Get subfolders
  ComPtr<ITaskFolderCollection> subFolders;
  hr = folder->GetFolders(0, &subFolders);
  if (SUCCEEDED(hr) && subFolders) {
    long count = 0;
    subFolders->get_Count(&count);

    for (long i = 1; i <= count; i++) {
      ComPtr<ITaskFolder> pSubFolder;
      hr = subFolders->get_Item(_variant_t(i), &pSubFolder);
      if (SUCCEEDED(hr) && pSubFolder) {
        // Get subfolder name
        BSTR folderName = nullptr;
        pSubFolder->get_Name(&folderName);

        int nameLen = WideCharToMultiByte(CP_UTF8, 0, folderName, -1, nullptr,
                                          0, nullptr, nullptr);
        std::string subFolderName(nameLen - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, folderName, -1, &subFolderName[0],
                            nameLen, nullptr, nullptr);

        std::string subFolderPath = folderPath;
        if (subFolderPath.back() != '\\')
          subFolderPath += "\\";
        subFolderPath += subFolderName;

        ComPtr<IDispatch> subFolderDispatch;
        pSubFolder->QueryInterface(IID_IDispatch, (void **)&subFolderDispatch);
        if (subFolderDispatch) {
          EnumerateTaskFolder(subFolderDispatch.Get(), subFolderPath,
                              outEntries, outErrors);
        }

        SysFreeString(folderName);
      }
    }
  }
}

void ScheduledTasksCollector::ExtractTaskActions(
    const std::string &taskName, const std::string &folderPath,
    IDispatch *taskDispatch, std::vector<Entry> &outEntries,
    std::vector<CollectorError> &outErrors) {
  if (!taskDispatch)
    return;

  // Get Definition property from task
  DISPID dispid = 0;
  LPOLESTR propName = L"Definition";
  HRESULT hr = taskDispatch->GetIDsOfNames(IID_NULL, &propName, 1,
                                           LOCALE_USER_DEFAULT, &dispid);
  if (FAILED(hr))
    return;

  DISPPARAMS params = {};
  VARIANT result;
  VariantInit(&result);

  hr = taskDispatch->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                            DISPATCH_PROPERTYGET, &params, &result, nullptr,
                            nullptr);
  if (FAILED(hr) || result.vt != VT_DISPATCH) {
    VariantClear(&result);
    return;
  }

  ComPtr<IDispatch> definition;
  definition.Attach(result.pdispVal); // Attach takes ownership and will Release

  // Get Actions from Definition
  propName = L"Actions";
  hr = definition->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT,
                                 &dispid);
  if (SUCCEEDED(hr)) {
    VariantInit(&result);
    hr = definition->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                            DISPATCH_PROPERTYGET, &params, &result, nullptr,
                            nullptr);
    if (SUCCEEDED(hr) && result.vt == VT_DISPATCH) {
      ComPtr<IDispatch> actions;
      actions.Attach(result.pdispVal);

      // Count actions
      propName = L"Count";
      hr = actions->GetIDsOfNames(IID_NULL, &propName, 1, LOCALE_USER_DEFAULT,
                                  &dispid);
      long actionCount = 0;
      if (SUCCEEDED(hr)) {
        VARIANT countResult;
        VariantInit(&countResult);
        hr = actions->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                             DISPATCH_PROPERTYGET, &params, &countResult,
                             nullptr, nullptr);
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
        hr = actions->Invoke(itemDispid, IID_NULL, LOCALE_USER_DEFAULT,
                             DISPATCH_METHOD, &itemParams, &actionResult,
                             nullptr, nullptr);

        if (SUCCEEDED(hr) && actionResult.vt == VT_DISPATCH) {
          ComPtr<IDispatch> action;
          action.Attach(actionResult.pdispVal);

          // Get action Type
          propName = L"Type";
          hr = action->GetIDsOfNames(IID_NULL, &propName, 1,
                                     LOCALE_USER_DEFAULT, &dispid);
          int actionType = 0;
          if (SUCCEEDED(hr)) {
            VARIANT typeResult;
            VariantInit(&typeResult);
            hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                                DISPATCH_PROPERTYGET, &params, &typeResult,
                                nullptr, nullptr);
            if (SUCCEEDED(hr)) {
              actionType = typeResult.intVal;
            }
            VariantClear(&typeResult);
          }

          // Only process ExecAction (type 0)
          if (actionType == 0) {
            // Get Path
            propName = L"Path";
            hr = action->GetIDsOfNames(IID_NULL, &propName, 1,
                                       LOCALE_USER_DEFAULT, &dispid);
            std::string execPath;
            if (SUCCEEDED(hr)) {
              VARIANT pathResult;
              VariantInit(&pathResult);
              hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                                  DISPATCH_PROPERTYGET, &params, &pathResult,
                                  nullptr, nullptr);
              if (SUCCEEDED(hr)) {
                execPath = VariantToString(pathResult);
              }
              VariantClear(&pathResult);
            }

            // Get Arguments
            propName = L"Arguments";
            hr = action->GetIDsOfNames(IID_NULL, &propName, 1,
                                       LOCALE_USER_DEFAULT, &dispid);
            std::string arguments;
            if (SUCCEEDED(hr)) {
              VARIANT argsResult;
              VariantInit(&argsResult);
              hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                                  DISPATCH_PROPERTYGET, &params, &argsResult,
                                  nullptr, nullptr);
              if (SUCCEEDED(hr)) {
                arguments = VariantToString(argsResult);
              }
              VariantClear(&argsResult);
            }

            // Get WorkingDirectory
            propName = L"WorkingDirectory";
            hr = action->GetIDsOfNames(IID_NULL, &propName, 1,
                                       LOCALE_USER_DEFAULT, &dispid);
            std::string workingDir;
            if (SUCCEEDED(hr)) {
              VARIANT wdResult;
              VariantInit(&wdResult);
              hr = action->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                                  DISPATCH_PROPERTYGET, &params, &wdResult,
                                  nullptr, nullptr);
              if (SUCCEEDED(hr)) {
                workingDir = VariantToString(wdResult);
              }
              VariantClear(&wdResult);
            }

            // Create Entry
            Entry entry(GetName(),  // source = "ScheduledTasks"
                        "Machine",  // scope = "Machine"
                        taskName,   // key = task name
                        folderPath, // location = folder path
                        arguments,  // arguments
                        ""          // imagePath (to be resolved)
            );

            // Resolve command and path
            CommandResolver::PopulateEntryCommand(entry, execPath, workingDir);

            outEntries.push_back(entry);
          }
        }

        VariantClear(&itemIndex);
      }
    }
  }
}

std::string ScheduledTasksCollector::VariantToString(const VARIANT &var) {
  std::string result;

  if (var.vt == VT_BSTR && var.bstrVal) {
    int len = WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, nullptr, 0,
                                  nullptr, nullptr);
    result.resize(len - 1);
    WideCharToMultiByte(CP_UTF8, 0, var.bstrVal, -1, &result[0], len, nullptr,
                        nullptr);
  }

  return result;
}

} // namespace uboot
