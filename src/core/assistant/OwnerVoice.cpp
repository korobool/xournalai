#include "OwnerVoice.h"

#include <cmath>    // for sqrt
#include <fstream>  // for ifstream, ofstream

#include <glib.h>  // for g_get_user_data_dir

#include <nlohmann/json.hpp>

namespace xoj::assistant {

using json = nlohmann::json;

fs::path OwnerVoice::file() { return fs::path(g_get_user_data_dir()) / "xournalai" / "voice" / "owner.json"; }

OwnerVoice OwnerVoice::load() {
    OwnerVoice v;
    std::ifstream in(file());
    if (!in) {
        return v;
    }
    const json j = json::parse(in, nullptr, false);
    if (!j.is_object() || !j.contains("model") || !j["model"].is_string() || !j.contains("fingerprints") ||
        !j["fingerprints"].is_array()) {
        return v;
    }
    v.modelName = j["model"].get<std::string>();
    for (const auto& f: j["fingerprints"]) {
        if (!f.is_array() || f.empty()) {
            continue;
        }
        Voice voice;
        for (const auto& x: f) {
            if (!x.is_number()) {
                voice.clear();
                break;
            }
            voice.push_back(x.get<float>());
        }
        if (!voice.empty() && (v.voices.empty() || voice.size() == v.voices.front().size())) {
            v.voices.push_back(std::move(voice));
        }
    }
    return v;
}

void OwnerVoice::add(const std::string& model, const Voice& voice) {
    if (model.empty() || voice.empty()) {
        return;
    }
    if (model != modelName || (!voices.empty() && voices.front().size() != voice.size())) {
        modelName = model;  // (another model: its fingerprints can't be compared with the old ones)
        voices.clear();
    }
    voices.push_back(voice);
    if (voices.size() > KEEP) {
        voices.erase(voices.begin(), voices.begin() + static_cast<long>(voices.size() - KEEP));
    }
}

bool OwnerVoice::save() const {
    std::error_code ec;
    fs::create_directories(file().parent_path(), ec);
    json list = json::array();
    for (const auto& v: voices) {
        list.push_back(v);
    }
    std::ofstream out(file());
    out << json({{"model", modelName}, {"fingerprints", list}}).dump() << "\n";
    return static_cast<bool>(out);
}

Voice OwnerVoice::average() const {
    if (voices.empty()) {
        return {};
    }
    std::vector<double> sum(voices.front().size(), 0.0);
    for (const auto& v: voices) {
        for (size_t i = 0; i < sum.size() && i < v.size(); i++) {
            sum[i] += v[i];
        }
    }
    double n = 0;
    for (double x: sum) {
        n += x * x;
    }
    n = std::sqrt(n);
    Voice out;
    out.reserve(sum.size());
    for (double x: sum) {
        out.push_back(static_cast<float>(n > 0 ? x / n : 0));
    }
    return out;
}

}  // namespace xoj::assistant
