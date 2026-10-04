#include <genomes/api/Api.hpp>

#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string>

namespace genomes::api {
namespace {

[[nodiscard]] foundation::Error parseError(const char* message) {
    return {foundation::ErrorCode::InvalidArgument, message};
}

[[nodiscard]] std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1U);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1U);
    return value;
}

[[nodiscard]] ModuleId moduleFor(std::string_view verb) noexcept {
    const auto dot = verb.find('.');
    const auto domain = verb.substr(0U, dot);
    if (domain == "core") return foundation::stable_id("core");
    if (domain == "units") return foundation::stable_id("module.infantry");
    std::string name{"module."};
    name.append(domain);
    return foundation::stable_id(name);
}

[[nodiscard]] foundation::Result<EncodedValue, foundation::Error> scalar(
    std::string_view token) {
    token = trim(token);
    if (token.size() >= 5U && token.starts_with("hex\"") && token.back() == '\"') {
        const std::string_view hexadecimal = token.substr(4U, token.size() - 5U);
        if ((hexadecimal.size() & 1U) != 0U) {
            return foundation::Result<EncodedValue, foundation::Error>::failure(
                parseError("hex command argument must have an even number of digits"));
        }
        const auto nibble = [](char character) constexpr -> std::optional<std::uint8_t> {
            if (character >= '0' && character <= '9')
                return static_cast<std::uint8_t>(character - '0');
            if (character >= 'a' && character <= 'f')
                return static_cast<std::uint8_t>(character - 'a' + 10);
            if (character >= 'A' && character <= 'F')
                return static_cast<std::uint8_t>(character - 'A' + 10);
            return std::nullopt;
        };
        std::vector<std::uint8_t> bytes;
        bytes.reserve(hexadecimal.size() / 2U);
        for (std::size_t index = 0U; index < hexadecimal.size(); index += 2U) {
            const auto high = nibble(hexadecimal[index]);
            const auto low = nibble(hexadecimal[index + 1U]);
            if (!high || !low) {
                return foundation::Result<EncodedValue, foundation::Error>::failure(
                    parseError("hex command argument contains a non-hexadecimal digit"));
            }
            bytes.push_back(static_cast<std::uint8_t>((*high << 4U) | *low));
        }
        return foundation::Result<EncodedValue, foundation::Error>::success(
            EncodedValue::binary(bytes));
    }
    if (token.size() >= 2U && token.front() == '"' && token.back() == '"')
        return foundation::Result<EncodedValue, foundation::Error>::success(
            EncodedValue::string(token.substr(1U, token.size() - 2U)));
    if (token == "true")
        return foundation::Result<EncodedValue, foundation::Error>::success(
            EncodedValue::boolean(true));
    if (token == "false")
        return foundation::Result<EncodedValue, foundation::Error>::success(
            EncodedValue::boolean(false));
    if (token.find('.') != std::string_view::npos) {
        std::string owned{token};
        char* end = nullptr;
        const double value = std::strtod(owned.c_str(), &end);
        if (end != owned.c_str() && *end == '\0')
            return foundation::Result<EncodedValue, foundation::Error>::success(
                EncodedValue::floatingPoint(value));
    } else {
        if (!token.empty() && token.front() == '-') {
            std::int64_t value{0};
            const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
            if (parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size())
                return foundation::Result<EncodedValue, foundation::Error>::success(
                    EncodedValue::signedInteger(value));
        }
        std::uint64_t value{0U};
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
        if (parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size())
            return foundation::Result<EncodedValue, foundation::Error>::success(
                EncodedValue::unsignedInteger(value));
    }
    if (!token.empty())
        return foundation::Result<EncodedValue, foundation::Error>::success(
            EncodedValue::string(token));
    return foundation::Result<EncodedValue, foundation::Error>::failure(
        parseError("empty command argument"));
}

} // namespace

foundation::Result<CommandEnvelope, foundation::Error> TextCommandAdapter::parse(
    std::string_view expression, foundation::SimulationTick target_tick,
    StableId source, std::uint8_t priority) const {
    expression = trim(expression);
    const auto open = expression.find('(');
    const auto close = expression.rfind(')');
    if (open == std::string_view::npos || close != expression.size() - 1U || open == 0U ||
        close < open) {
        return foundation::Result<CommandEnvelope, foundation::Error>::failure(
            parseError("command must use verb(arg1, arg2) syntax"));
    }
    const auto verb_text = trim(expression.substr(0U, open));
    if (verb_text.empty() || verb_text.find_first_of(" \t\r\n") != std::string_view::npos) {
        return foundation::Result<CommandEnvelope, foundation::Error>::failure(
            parseError("invalid command verb"));
    }
    const ModuleId module = moduleFor(verb_text);
    const ApiId verb = foundation::stable_id(verb_text);
    const auto* descriptor = registry_.findCommand(module, verb);
    if (descriptor == nullptr) {
        return foundation::Result<CommandEnvelope, foundation::Error>::failure(
            {foundation::ErrorCode::NotFound, "command is not registered"});
    }

    std::vector<EncodedValue> values;
    std::string_view arguments = expression.substr(open + 1U, close - open - 1U);
    if (!trim(arguments).empty()) {
        while (!arguments.empty()) {
            const auto comma = arguments.find(',');
            auto value = scalar(arguments.substr(0U, comma));
            if (!value) {
                return foundation::Result<CommandEnvelope, foundation::Error>::failure(
                    value.error());
            }
            values.push_back(std::move(value.value()));
            if (comma == std::string_view::npos) break;
            arguments.remove_prefix(comma + 1U);
        }
    }
    if (descriptor->arguments.size() != values.size()) {
        return foundation::Result<CommandEnvelope, foundation::Error>::failure(
            parseError("command argument count does not match its schema"));
    }
    for (std::size_t index = 0U; index < values.size(); ++index) {
        if (values[index].type != descriptor->arguments[index]) {
            return foundation::Result<CommandEnvelope, foundation::Error>::failure(
                parseError("command argument type does not match its schema"));
        }
    }
    EncodedValue payload{};
    if (values.size() == 1U) payload = std::move(values.front());
    else if (!values.empty()) payload = EncodedValue::tuple(values);
    return foundation::Result<CommandEnvelope, foundation::Error>::success(
        {module, verb, descriptor->schema_version, target_tick, source, 0U, priority,
         std::move(payload)});
}

} // namespace genomes::api
