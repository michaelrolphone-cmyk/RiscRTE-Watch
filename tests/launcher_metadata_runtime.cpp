// Presentation-only probe of unchanged production Runtime normalization and
// catalog selection. Test-only -fno-access-control seeds already-admitted policy
// slots; this does not claim native admission, app execution, or hardware proof.
#include "bootstrap/Runtime.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
using RiscBoot::Runtime;
static bool owner() { return true; }
static bool health(risc_runtime_health_v1*) { return true; }
static void delay(uint32_t) {}
static bool log(const char*) { return true; }
int main(int argc, char** argv) {
    assert(argc > 1);
    Runtime runtime({owner, health, delay, log});
    const size_t count = static_cast<size_t>(argc - 1);
    assert(count <= Runtime::MaxAppPolicies);
    runtime.policies_ = RiscBoot::metadataArray<Runtime::AppPolicy>(count);
    assert(runtime.policies_);
    runtime.policyCount_ = runtime.bootPolicyCount_ = runtime.policyCapacity_ = count;
    strcpy(runtime.root_, "/catalog");
    strcpy(runtime.default_, "/catalog/default.elf");
    JsonDocument output;
    JsonArray rows = output["rows"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        std::ifstream file(argv[i + 1]); assert(file);
        std::stringstream bytes; bytes << file.rdbuf();
        const auto raw = bytes.str();
        JsonDocument document;
        assert(RiscBoot::parse(raw.c_str(), raw.size(), document));
        const auto manifest = document.as<JsonObjectConst>();
        auto& policy = runtime.policies_[i];
        assert(RiscBoot::text(manifest["id"], policy.id, sizeof(policy.id)));
        assert(RiscBoot::path(runtime.root_, manifest["file_name"], policy.elf, sizeof(policy.elf)));
        Runtime::appCatalogMetadata(manifest, policy.catalog);
        JsonObject row = rows.add<JsonObject>();
        row["id"] = policy.id;
        row["file_name"] = manifest["file_name"];
        row["display_name"] = policy.catalog.displayName;
        row["visible"] = policy.catalog.visible;
    }
    uint32_t visible = 0;
    assert(runtime.appCatalogRefresh(visible));
    output["visible_count"] = visible;
    JsonArray selected = output["selected"].to<JsonArray>();
    for (size_t i = 0; i < count; ++i) {
        if (runtime.appCatalogState_[i]) selected.add(runtime.policies_[i].id);
        assert(bool(runtime.appCatalogState_[i]) == bool(rows[i]["visible"]));
    }
    serializeJson(output, std::cout);
    std::cout << '\n';
}
