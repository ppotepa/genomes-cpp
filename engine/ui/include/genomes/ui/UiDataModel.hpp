#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <functional>

namespace genomes::ui {

using UiScalar = std::variant<bool, std::int64_t, double, std::string>;

struct UiOption final {
    std::string value;
    std::string label;
    bool enabled{true};
    [[nodiscard]] bool operator==(const UiOption&) const = default;
};

struct UiMetricRow final {
    std::string label;
    std::string value;
};

using UiTableRow = std::unordered_map<std::string, UiScalar>;

enum class UiCommitPolicy : std::uint8_t { Live, OnChange, Explicit };

struct UiFieldState final {
    UiScalar value{};
    std::string error;
    bool enabled{true};
    bool visible{true};
    bool read_only{false};
    bool dirty{false};
    UiCommitPolicy commit_policy{UiCommitPolicy::OnChange};
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> step;
    std::vector<UiOption> options;
};

class UiDataModel final {
public:
    [[nodiscard]] bool set(std::string key, UiScalar value) {
        auto [it, inserted] = fields_.try_emplace(std::move(key));
        if (!inserted && it->second.value == value) return false;
        it->second.value = std::move(value);
        it->second.dirty = true;
        changed_.push_back(it->first);
        ++revision_;
        return true;
    }

    [[nodiscard]] bool set_field(std::string key, UiFieldState field) {
        const auto it = fields_.find(key);
        if (it != fields_.end() && same_field(it->second, field)) return false;
        field.dirty = true;
        fields_[key] = std::move(field);
        changed_.push_back(std::move(key));
        ++revision_;
        return true;
    }

    [[nodiscard]] bool set_list(std::string key, std::vector<UiTableRow> value) {
        const auto it = lists_.find(key);
        if (it != lists_.end() && it->second == value) return false;
        lists_[key] = std::move(value);
        changed_.push_back(std::move(key));
        ++revision_;
        return true;
    }

    [[nodiscard]] const UiScalar* find(std::string_view key) const noexcept {
        const auto it = fields_.find(std::string{key});
        return it == fields_.end() ? nullptr : &it->second.value;
    }
    [[nodiscard]] UiFieldState* find_field(std::string_view key) noexcept {
        const auto it = fields_.find(std::string{key});
        return it == fields_.end() ? nullptr : &it->second;
    }
    [[nodiscard]] const UiFieldState* find_field(std::string_view key) const noexcept {
        return const_cast<UiDataModel*>(this)->find_field(key);
    }
    [[nodiscard]] const std::vector<UiTableRow>* find_list(std::string_view key) const noexcept {
        const auto it = lists_.find(std::string{key});
        return it == lists_.end() ? nullptr : &it->second;
    }
    [[nodiscard]] const std::vector<std::string>& changed_keys() const noexcept { return changed_; }
    [[nodiscard]] std::optional<std::string> first_error_key() const {
        for (const auto& [key, field] : fields_) if (field.visible && field.enabled && !field.error.empty()) return key;
        return {};
    }
    [[nodiscard]] bool validate() const {
        for (const auto& [key, field] : fields_) {
            if (!field.visible || !field.enabled || field.read_only) continue;
            if (!field.error.empty()) return false;
            if (const auto* text = std::get_if<std::string>(&field.value); text != nullptr && text->empty()) return false;
            if (const auto* number = std::get_if<double>(&field.value); number != nullptr && !std::isfinite(*number)) return false;
            const auto numeric = std::visit([](const auto& value) -> std::optional<double> {
                using Value = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<Value, std::int64_t> || std::is_same_v<Value, double>)
                    return static_cast<double>(value);
                return {};
            }, field.value);
            if (numeric.has_value()) {
                if (field.minimum && *numeric < *field.minimum) return false;
                if (field.maximum && *numeric > *field.maximum) return false;
            }
            if (!field.options.empty()) {
                const auto* selected = std::get_if<std::string>(&field.value);
                if (selected == nullptr) return false;
                const auto option = std::find_if(field.options.begin(), field.options.end(),
                    [selected](const UiOption& candidate) { return candidate.enabled && candidate.value == *selected; });
                if (option == field.options.end()) return false;
            }
        }
        return true;
    }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }
    [[nodiscard]] std::size_t size() const noexcept { return fields_.size(); }
    void for_each_field(const std::function<void(std::string_view, const UiFieldState&)>& visitor) const {
        for (const auto& [key, field] : fields_) visitor(key, field);
    }
    void for_each_list(const std::function<void(std::string_view, const std::vector<UiTableRow>&)>& visitor) const {
        for (const auto& [key, list] : lists_) visitor(key, list);
    }

    void acknowledge_changes() {
        for (const auto& key : changed_) if (auto* field = find_field(key)) field->dirty = false;
        changed_.clear();
    }
    void clear() noexcept {
        if (fields_.empty() && lists_.empty()) return;
        fields_.clear(); lists_.clear(); changed_.clear(); ++revision_;
    }

    // Parsing is deliberately locale-independent: RML values use a dot decimal
    // separator and invalid/non-finite values never replace the working value.
    [[nodiscard]] bool apply_number(std::string_view key, std::string_view text) {
        auto* field = find_field(key);
        if (!field || !field->enabled || !field->visible || field->read_only || text.empty()) {
            return reject(field, "A value is required");
        }
        double parsed = 0.0;
        const auto converted = std::from_chars(text.data(), text.data() + text.size(), parsed,
                                               std::chars_format::general);
        if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size() ||
            !std::isfinite(parsed)) return reject(field, "Enter a finite number");
        double value = parsed;
        if (field->minimum) value = std::max(value, *field->minimum);
        if (field->maximum) value = std::min(value, *field->maximum);
        if (field->step && *field->step > 0.0) value = std::round(value / *field->step) * *field->step;
        if (field->minimum) value = std::max(value, *field->minimum);
        if (field->maximum) value = std::min(value, *field->maximum);
        const bool had_error = !field->error.empty();
        field->error.clear();
        bool changed = false;
        if (std::holds_alternative<std::int64_t>(field->value)) {
            constexpr double integer_min = static_cast<double>(std::numeric_limits<std::int64_t>::min());
            constexpr double integer_max = static_cast<double>(std::numeric_limits<std::int64_t>::max());
            if (value <= integer_min) changed = set(std::string{key}, std::numeric_limits<std::int64_t>::min());
            else if (value >= integer_max) changed = set(std::string{key}, std::numeric_limits<std::int64_t>::max());
            else changed = set(std::string{key}, static_cast<std::int64_t>(std::llround(value)));
        } else {
            changed = set(std::string{key}, value);
        }
        if (!changed && had_error) mark_changed(key, *field);
        return changed || had_error;
    }

    [[nodiscard]] bool apply_option(std::string_view key, std::string_view value) {
        auto* field = find_field(key);
        if (!field || !field->enabled || !field->visible || field->read_only) return reject(field, "This control is unavailable");
        const auto match = std::find_if(field->options.begin(), field->options.end(), [value](const UiOption& option) {
            return option.enabled && option.value == value;
        });
        if (match == field->options.end()) return reject(field, "Choose an available option");
        const bool had_error = !field->error.empty();
        field->error.clear();
        const bool changed = set(std::string{key}, std::string{value});
        if (!changed && had_error) mark_changed(key, *field);
        return changed || had_error;
    }

private:
    static bool same_field(const UiFieldState& left, const UiFieldState& right) {
        return left.value == right.value && left.error == right.error && left.enabled == right.enabled &&
               left.visible == right.visible && left.read_only == right.read_only &&
               left.commit_policy == right.commit_policy && left.minimum == right.minimum &&
               left.maximum == right.maximum && left.step == right.step && left.options == right.options;
    }
    [[nodiscard]] bool reject(UiFieldState* field, std::string error) {
        if (!field) return false;
        if (field->error == error) return false;
        field->error = std::move(error); field->dirty = true;
        for (const auto& [key, candidate] : fields_) if (&candidate == field) { changed_.push_back(key); break; }
        ++revision_;
        return false;
    }
    void mark_changed(std::string_view key, UiFieldState& field) {
        field.dirty = true;
        changed_.emplace_back(key);
        ++revision_;
    }

    std::unordered_map<std::string, UiFieldState> fields_;
    std::unordered_map<std::string, std::vector<UiTableRow>> lists_;
    std::vector<std::string> changed_;
    std::uint64_t revision_{0};
};

} // namespace genomes::ui
