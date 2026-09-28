#include "ElementIds.h"

#include <cctype>  // for isdigit

#include "model/Element.h"  // for Element

namespace xoj::api {

ElementIds& ElementIds::get() {
    static ElementIds* instance = [] {
        auto* ids = new ElementIds();  // intentionally leaked: elements may be destroyed during static teardown
        Element::setDestructionObserver(&ElementIds::onElementDestroyed);
        return ids;
    }();
    return *instance;
}

void ElementIds::onElementDestroyed(const Element* e) {
    ElementIds& self = get();
    std::lock_guard lock(self.mutex);
    self.origins.erase(e);
    if (auto it = self.byElement.find(e); it != self.byElement.end()) {
        self.byId.erase(it->second);
        self.byElement.erase(it);
    }
}

std::string ElementIds::idOf(const Element* e) {
    std::lock_guard lock(mutex);
    auto [it, inserted] = byElement.try_emplace(e, next);
    if (inserted) {
        byId.emplace(next, e);
        next++;
    }
    return "e" + std::to_string(it->second);
}

std::optional<std::string> ElementIds::existingId(const Element* e) const {
    std::lock_guard lock(mutex);
    if (auto it = byElement.find(e); it != byElement.end()) {
        return "e" + std::to_string(it->second);
    }
    return std::nullopt;
}

const Element* ElementIds::lookup(const std::string& id) const {
    auto n = parse(id);
    if (!n) {
        return nullptr;
    }
    std::lock_guard lock(mutex);
    auto it = byId.find(*n);
    return it == byId.end() ? nullptr : it->second;
}

size_t ElementIds::size() const {
    std::lock_guard lock(mutex);
    return byElement.size();
}

void ElementIds::setOrigin(const Element* e, const std::string& operation) {
    std::lock_guard lock(mutex);
    origins[e] = operation;
}

std::optional<std::string> ElementIds::origin(const Element* e) const {
    std::lock_guard lock(mutex);
    if (auto it = origins.find(e); it != origins.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<const Element*> ElementIds::elementsOfOperation(const std::string& operation) const {
    std::lock_guard lock(mutex);
    std::vector<const Element*> out;
    for (const auto& [e, op]: origins) {
        if (op == operation) {
            out.push_back(e);
        }
    }
    return out;
}

std::optional<uint64_t> ElementIds::parse(const std::string& id) {
    size_t start = (!id.empty() && (id[0] == 'e' || id[0] == 'E')) ? 1 : 0;
    if (start >= id.size() || id.size() - start > 18) {
        return std::nullopt;
    }
    uint64_t n = 0;
    for (size_t i = start; i < id.size(); i++) {
        if (!std::isdigit(static_cast<unsigned char>(id[i]))) {
            return std::nullopt;
        }
        n = n * 10 + static_cast<uint64_t>(id[i] - '0');
    }
    return n;
}

}  // namespace xoj::api
