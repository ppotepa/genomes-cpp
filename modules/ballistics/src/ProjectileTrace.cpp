#include <genomes/ballistics/ProjectileTrace.hpp>

namespace genomes::ballistics {

void ProjectileTrace::onSegment(const TraceSegment& segment) noexcept {
    if (segments_.size() < maximum_segments_) {
        segments_.push_back(segment);
    }
}

void ProjectileTrace::onContact(const TraceContact& contact) noexcept {
    if (contacts_.size() < maximum_contacts_) {
        contacts_.push_back(contact);
    }
}

void ProjectileTrace::onTerminal(const TraceTerminal& terminal) noexcept {
    if (terminals_.size() < maximum_terminals_) {
        terminals_.push_back(terminal);
    }
}

void ProjectileTrace::clear() noexcept {
    segments_.clear();
    contacts_.clear();
    terminals_.clear();
}

} // namespace genomes::ballistics
