// ─────────────────────────────────────────────────────────────────────────────
// Bencode unit tests
// Build: cmake .. && make test_bencode
// Run:   ./test_bencode
//
// All tests should pass before you move on to Phase 2.
// ─────────────────────────────────────────────────────────────────────────────

#include "bencode.hpp"
#include <iostream>
#include <cassert>

static int tests_run = 0, tests_passed = 0;

#define CHECK(expr)                                                     \
    do {                                                                \
        ++tests_run;                                                    \
        if (expr) {                                                     \
            ++tests_passed;                                             \
        } else {                                                        \
            std::cerr << "FAIL [line " << __LINE__ << "]: " #expr "\n";\
        }                                                               \
    } while (0)

int main() {
    using namespace bencode;

    // ── Integers ──────────────────────────────────────────────────────────────
    CHECK(decode("i42e").as_int()   == 42);
    CHECK(decode("i0e").as_int()    == 0);
    CHECK(decode("i-7e").as_int()   == -7);
    CHECK(decode("i1000e").as_int() == 1000);

    // ── Strings ───────────────────────────────────────────────────────────────
    CHECK(decode("4:spam").as_str()  == "spam");
    CHECK(decode("0:").as_str()      == "");
    CHECK(decode("11:hello world").as_str() == "hello world");

    // ── Lists ─────────────────────────────────────────────────────────────────
    {
        auto list = decode("li1ei2ei3ee").as_list();
        CHECK(list.size() == 3);
        CHECK(list[0].as_int() == 1);
        CHECK(list[1].as_int() == 2);
        CHECK(list[2].as_int() == 3);
    }

    {   // list of strings
        auto list = decode("l4:spam3:fooe").as_list();
        CHECK(list.size() == 2);
        CHECK(list[0].as_str() == "spam");
        CHECK(list[1].as_str() == "foo");
    }

    // ── Dicts ─────────────────────────────────────────────────────────────────
    {
        auto dict = decode("d3:fooi42ee").as_dict();
        CHECK(dict.at("foo").as_int() == 42);
    }

    {
        auto dict = decode("d3:bar4:spam3:fooi1ee").as_dict();
        CHECK(dict.at("foo").as_int()  == 1);
        CHECK(dict.at("bar").as_str()  == "spam");
    }

    // ── Nested ────────────────────────────────────────────────────────────────
    {
        // {"foo": [42]} = d3:fooli42eee
        auto root = decode("d3:fooli42eee").as_dict();
        CHECK(root.at("foo").as_list()[0].as_int() == 42);
    }

    {
        // [{"a": 1}, {"b": 2}] = ld1:ai1eed1:bi2eee
        auto list = decode("ld1:ai1eed1:bi2eee").as_list();
        CHECK(list.size() == 2);
        CHECK(list[0].as_dict().at("a").as_int() == 1);
        CHECK(list[1].as_dict().at("b").as_int() == 2);
    }

    // ── Encode round-trips ────────────────────────────────────────────────────
    CHECK(encode(decode("i42e"))          == "i42e");
    CHECK(encode(decode("i-7e"))          == "i-7e");
    CHECK(encode(decode("4:spam"))        == "4:spam");
    CHECK(encode(decode("li1ei2ee"))      == "li1ei2ee");
    CHECK(encode(decode("d3:fooi1ee"))    == "d3:fooi1ee");

    // ── Result ────────────────────────────────────────────────────────────────
    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
