#include "../core/json/JsonWriter.h"
#include "../core/backup/BackupStore.h"
#include "../core/model/CollectorError.h"
#include "../core/model/Entry.h"
#include "../core/tx/Transaction.h"
#include "../core/runner/CollectorRunner.h"
#include "../core/util/CliArgs.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

// Entry point for uboot-core.exe
// Communicates exclusively via JSON on stdout.
// stderr is reserved for fatal startup errors only.

namespace {

void WriteJsonDocument(const rapidjson::Document &doc, bool prettyPrint) {
    rapidjson::StringBuffer buffer;
    if (prettyPrint) {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
    } else {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
    }
    std::cout << buffer.GetString() << std::endl;
}

int WriteJsonError(const std::string &message, int code = 1) {
    rapidjson::Document doc;
    doc.SetObject();
    auto &alloc = doc.GetAllocator();
    doc.AddMember("error", rapidjson::Value(message.c_str(), alloc), alloc);
    WriteJsonDocument(doc, false);
    return code;
}

std::string ToLower(std::string value) {
    for (char &c : value) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

rapidjson::Value NormalizeOperation(const rapidjson::Value &op,
                                    rapidjson::Document::AllocatorType &alloc) {
    rapidjson::Value normalized(rapidjson::kObjectType);

    std::string type;
    if (op.HasMember("type") && op["type"].IsString()) {
        type = op["type"].GetString();
    }
    normalized.AddMember("type", rapidjson::Value(type.c_str(), alloc), alloc);

    std::string opId;
    if (op.HasMember("op_id") && op["op_id"].IsString()) {
        opId = op["op_id"].GetString();
    } else if (op.HasMember("opId") && op["opId"].IsString()) {
        opId = op["opId"].GetString();
    }
    normalized.AddMember("op_id", rapidjson::Value(opId.c_str(), alloc), alloc);

    std::string action;
    if (op.HasMember("action") && op["action"].IsString()) {
        action = ToLower(op["action"].GetString());
    }
    normalized.AddMember("action", rapidjson::Value(action.c_str(), alloc), alloc);

    std::string target;
    if (op.HasMember("fullKeyPath") && op["fullKeyPath"].IsString()) {
        target = op["fullKeyPath"].GetString();
        if (op.HasMember("valueName") && op["valueName"].IsString()) {
            target += "\\";
            target += op["valueName"].GetString();
        }
    } else if (op.HasMember("serviceName") && op["serviceName"].IsString()) {
        target = op["serviceName"].GetString();
    } else if (op.HasMember("service") && op["service"].IsString()) {
        target = op["service"].GetString();
    } else if (op.HasMember("taskPath") && op["taskPath"].IsString()) {
        target = op["taskPath"].GetString();
    } else if (op.HasMember("path") && op["path"].IsString()) {
        target = op["path"].GetString();
    }
    normalized.AddMember("target", rapidjson::Value(target.c_str(), alloc), alloc);

    rapidjson::Value metadata(rapidjson::kObjectType);
    metadata.CopyFrom(op, alloc);
    normalized.AddMember("metadata", metadata, alloc);

    return normalized;
}

bool LoadManifestDocument(const std::string &manifestPath, rapidjson::Document &doc) {
    std::ifstream ifs(manifestPath);
    if (!ifs.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    doc.Parse(content.c_str());
    return !doc.HasParseError() && doc.IsObject();
}

rapidjson::Value BuildPlanFromManifest(const std::string &txId,
                                       const std::string &manifestPath,
                                       rapidjson::Document::AllocatorType &alloc) {
    rapidjson::Value plan(rapidjson::kObjectType);
    plan.AddMember("tx_id", rapidjson::Value(txId.c_str(), alloc), alloc);

    rapidjson::Value entryIds(rapidjson::kArrayType);
    plan.AddMember("entry_ids", entryIds, alloc);

    plan.AddMember("reason", "", alloc);
    plan.AddMember("executed", false, alloc);

    rapidjson::Value operations(rapidjson::kArrayType);

    rapidjson::Document manifest;
    if (LoadManifestDocument(manifestPath, manifest)) {
        if (manifest.HasMember("state") && manifest["state"].IsString()) {
            std::string state = manifest["state"].GetString();
            bool executed =
                state == "Applied" || state == "Verified" || state == "Committed";
            plan["executed"].SetBool(executed);
        }

        if (manifest.HasMember("operations") && manifest["operations"].IsArray()) {
            for (const auto &op : manifest["operations"].GetArray()) {
                operations.PushBack(NormalizeOperation(op, alloc), alloc);
            }
        }
    }

    plan.AddMember("operations", operations, alloc);
    return plan;
}

} // namespace

int main(int argc, char *argv[]) {
    // Force UTF-8 output on Windows
    SetConsoleOutputCP(CP_UTF8);

    uboot::CliArgs args = uboot::CliArgs::Parse(argc, argv);

    if (!args.isValid) {
        std::cerr << "{\"error\":\"" << args.errorMessage << "\"}" << std::endl;
        return 1;
    }

    // Default to all sources when none specified
    if (args.sources.empty()) {
        args.sources.push_back("all");
    }

    if (args.command == "collect" || args.command.empty()) {
        uboot::CollectorRunner runner;
        std::vector<uboot::Entry> entries;
        std::vector<uboot::CollectorError> errors;

        runner.Run(args.sources, entries, errors);

        bool ok = uboot::JsonWriter::Write(
            args.outputPath,
            entries,
            errors,
            args.prettyPrint,
            args.schemaVersion
        );

        return ok ? 0 : 1;
    }

    if (args.command == "actions") {
        BackupStore store;
        if (!store.init()) {
            return WriteJsonError("Failed to initialize backup store");
        }

        if (args.subCommand == "plan") {
            Transaction tx(store);
            Transaction::Config config;
            config.forceCritical = args.forceCritical;
            config.bulkConfirm = args.bulkConfirm;
            config.safeMode = args.safeMode;
            tx.setConfig(config);

            bool planned = tx.plan();

            rapidjson::Document doc;
            doc.SetObject();
            auto &alloc = doc.GetAllocator();

            std::string txId = tx.getId();
            doc.AddMember("tx_id", rapidjson::Value(txId.c_str(), alloc), alloc);

            rapidjson::Value entryIds(rapidjson::kArrayType);
            for (const std::string &id : args.targetIds) {
                entryIds.PushBack(rapidjson::Value(id.c_str(), alloc), alloc);
            }
            doc.AddMember("entry_ids", entryIds, alloc);
            doc.AddMember("reason", rapidjson::Value(args.reason.c_str(), alloc), alloc);
            doc.AddMember("executed", false, alloc);

            rapidjson::Value operations(rapidjson::kArrayType);
            doc.AddMember("operations", operations, alloc);

            WriteJsonDocument(doc, args.prettyPrint);
            return planned ? 0 : 1;
        }

        if (args.subCommand == "apply" || args.subCommand == "undo") {
            std::string txPath = store.getTransactionDirectory(args.txId);
            if (txPath.empty()) {
                return WriteJsonError("Transaction not found: " + args.txId);
            }

            std::filesystem::path manifestPath =
                std::filesystem::path(txPath) / "manifest.json";

            Transaction tx(store);
            if (!tx.loadFromManifest(manifestPath.string())) {
                return WriteJsonError("Failed to load transaction manifest: " +
                                      manifestPath.string());
            }

            bool success =
                (args.subCommand == "apply") ? tx.runAll() : tx.undoAll();

            rapidjson::Document doc;
            doc.SetObject();
            auto &alloc = doc.GetAllocator();
            doc.AddMember("tx_id", rapidjson::Value(args.txId.c_str(), alloc), alloc);

            if (args.subCommand == "apply") {
                doc.AddMember("applied", success, alloc);
            } else {
                doc.AddMember("undone", success, alloc);
            }

            WriteJsonDocument(doc, args.prettyPrint);
            return success ? 0 : 1;
        }

        if (args.subCommand == "list") {
            std::vector<TransactionInfo> transactions = store.listTransactions();

            rapidjson::Document doc;
            doc.SetObject();
            auto &alloc = doc.GetAllocator();
            rapidjson::Value list(rapidjson::kArrayType);

            for (const auto &info : transactions) {
                std::filesystem::path manifestPath =
                    std::filesystem::path(info.path) / "manifest.json";
                list.PushBack(
                    BuildPlanFromManifest(info.id, manifestPath.string(), alloc),
                    alloc);
            }

            doc.AddMember("transactions", list, alloc);
            WriteJsonDocument(doc, args.prettyPrint);
            return 0;
        }

        return WriteJsonError("Unknown action: " + args.subCommand);
    }

    // Unknown command fallback
    return WriteJsonError("Unknown command: " + args.command);
}
