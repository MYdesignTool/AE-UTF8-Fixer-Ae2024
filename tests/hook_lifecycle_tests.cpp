#include "../UTF8Fixer.cpp"
#include <cstdio>
#define CHECK(expr) do { ++checks; if (!(expr)) { std::printf("FAIL line %d: %s\n", __LINE__, #expr); return 1; } } while (0)
namespace {
void* fake_target = reinterpret_cast<void*>(0x12340000);
void* fake_trampoline = reinterpret_cast<void*>(0x12350000);
void* published{};
unsigned creates{}, enables{}, removes{};
bool removed_after_restore{}, enabled_after_publish{};
MH_STATUS create_result = MH_OK, enable_result = MH_ERROR_MEMORY_PROTECT;
MH_STATUS WINAPI Create(LPVOID, LPVOID, LPVOID* original) {
    ++creates; *original = fake_trampoline; return create_result;
}
MH_STATUS WINAPI Enable(LPVOID) {
    ++enables; enabled_after_publish = published == fake_trampoline; return enable_result;
}
MH_STATUS WINAPI Remove(LPVOID) {
    ++removes; removed_after_restore = published == fake_target; return MH_OK;
}
}
int main() {
    int checks{};
    const HookBackend backend{&Create, &Enable, &Remove};
    const auto detour = reinterpret_cast<void*>(0x12360000);
    const auto publish = [](void* p) { published = p; };
    CHECK(!Install(fake_target, detour, publish, backend));
    CHECK(creates == 1 && enables == 1 && removes == 1);
    CHECK(enabled_after_publish && removed_after_restore);
    CHECK(published == fake_target && !hooks.contains(fake_target));
    // A failed optional encoder hook must leave a valid original function pointer,
    // not an address inside the fake_trampoline that RemoveHook just released.
    for (int i = 0; i < 100; ++i) CHECK(!Install(fake_target, detour, publish, backend));
    CHECK(published == fake_target && !hooks.contains(fake_target));
    enable_result = MH_OK;
    CHECK(Install(fake_target, detour, publish, backend));
    CHECK(published == fake_trampoline && hooks.contains(fake_target));
    auto previous_creates = creates;
    CHECK(Install(fake_target, detour, publish, backend));
    CHECK(creates == previous_creates);
    hooks.erase(fake_target);
    create_result = MH_ERROR_UNSUPPORTED_FUNCTION;
    published = nullptr;
    const auto previous_enables = enables, previous_removes = removes;
    CHECK(!Install(fake_target, detour, publish, backend));
    CHECK(!published && !hooks.contains(fake_target));
    CHECK(enables == previous_enables && removes == previous_removes);
    CHECK(!Install(nullptr, detour, publish, backend));
    std::printf("PASS: %d hook lifecycle checks, failure rollback, retry and duplicate-target handling.\n", checks);
}
