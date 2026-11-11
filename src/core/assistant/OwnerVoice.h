/*
 * xournalai (based on Xournal++)
 *
 * The owner's own voice (1.24.0, experimental; the owner, 11 Nov 2026: «Распознавание команд именно моим голосом –
 * это должна быть опциональная фишка … по умолчанию выключена и просто работает на голос»). With «🎤 Only my voice»
 * on, what he says with «Hold», the AI lasso and the AI highlighter is certainly him: the speech recognizer's voice
 * fingerprint of it (docs/mcp/TRANSCRIBERS.md) is kept, the last few of them, in the app's own data folder
 * (<data>/xournalai/voice/owner.json). Fingerprints only, never sound: the speech can't be got back from them. Their
 * average is his voice; only fingerprints of the same model are ever compared with it.
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <string>  // for string
#include <vector>  // for vector

#include "util/PathUtil.h"  // for fs

#include "Transcriber.h"  // for Voice

namespace xoj::assistant {

class OwnerVoice final {
public:
    static constexpr size_t KEEP = 20;  ///< the last this many fingerprints

    static fs::path file();
    /// What is kept (nothing when there is no file or it can't be read)
    static OwnerVoice load();
    /// Keeps a fingerprint of his (another model's: what was kept of the old model is dropped)
    void add(const std::string& model, const Voice& voice);
    bool save() const;

    const std::string& model() const { return modelName; }
    size_t samples() const { return voices.size(); }
    /// His voice: the average of what is kept (unit length); empty when nothing is
    Voice average() const;

private:
    std::string modelName;
    std::vector<Voice> voices;
};

}  // namespace xoj::assistant
