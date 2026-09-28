#include <genomes/foundation/Error.hpp>
#include <genomes/foundation/Handle.hpp>
#include <genomes/foundation/Result.hpp>
#include <genomes/foundation/StableHash.hpp>
#include <genomes/foundation/StrongId.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace {

struct FirstIdTag;
struct SecondIdTag;
struct SlotTag;

using FirstId = genomes::foundation::StrongId<FirstIdTag>;
using SecondId = genomes::foundation::StrongId<SecondIdTag>;
using SlotHandle = genomes::foundation::Handle<SlotTag>;
using Error = genomes::foundation::Error;

static_assert(std::is_trivially_copyable_v<FirstId>);
static_assert(std::is_standard_layout_v<FirstId>);
static_assert(sizeof(FirstId) == sizeof(std::uint64_t));
static_assert(!std::is_convertible_v<FirstId, SecondId>);

} // namespace

int main() {
    const FirstId invalid;
    const FirstId first(42);
    assert(!invalid.isValid());
    assert(first.isValid());
    assert(first.value() == 42);
    assert(first > invalid);

    SlotHandle old{0, 1};
    std::uint32_t generation = 1;
    assert(old.isValid());
    assert(old.packed() == (std::uint64_t{1} << 32));
    ++generation;
    assert(old.generation != generation);
    const SlotHandle current{old.index, generation};
    assert(current.isValid());
    assert(current != old);

    auto success = genomes::foundation::Result<int, Error>::success(7);
    assert(success);
    assert(success.value() == 7);

    auto failure = genomes::foundation::Result<int, Error>::failure(
        {genomes::foundation::ErrorCode::NotFound, "missing"});
    assert(!failure);
    assert(failure.error().code == genomes::foundation::ErrorCode::NotFound);

    auto move_only = genomes::foundation::Result<std::unique_ptr<int>, Error>::success(
        std::make_unique<int>(9));
    assert(move_only && *move_only.value() == 9);

    auto void_success = genomes::foundation::Result<void, Error>::success();
    assert(void_success);
    auto void_failure = genomes::foundation::Result<void, Error>::failure(
        {genomes::foundation::ErrorCode::InvalidState, "closed"});
    assert(!void_failure);
    assert(void_failure.error().code == genomes::foundation::ErrorCode::InvalidState);

    using genomes::foundation::stableHashCombine;
    using genomes::foundation::stableHashString;
    using genomes::foundation::stableHashU64;
    assert(genomes::foundation::StableHashAlgorithmVersion == 1);
    assert(stableHashString("") == 0xcbf29ce484222325ull);
    assert(stableHashString("Genomes") == 0x1ca006a5dfe590c5ull);
    assert(stableHashString("terrain") == 0x3f8b069c872d66aeull);
    assert(stableHashString("building") == 0xfae140ba7fda6475ull);
    assert(stableHashU64(0) == 0xa8c7f832281a39c5ull);
    assert(stableHashU64(1) == 0x89cd31291d2aefa4ull);
    assert(stableHashU64(UINT64_MAX) == 0x8cf51a8bfca3883dull);
    assert(stableHashCombine(0, 0) == 0x88201fb960ff6465ull);
    assert(stableHashCombine(stableHashString("city"), 42) == 0x9fe52ca5c0284cffull);
    return 0;
}
