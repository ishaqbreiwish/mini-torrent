#pragma once
#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>

// ── Bencode type system ───────────────────────────────────────────────────────
//
// Bencode has 4 types: integer, byte-string, list, dict.
// We model them as a recursive C++ type using std::variant.
//
// std::variant<A, B, C> is a type-safe tagged union — it holds exactly ONE of
// A, B, or C at a time.  Think of it as:  enum Tag {Int, Str, List, Dict} + data.
//
// std::holds_alternative<T>(v)  → true if v currently holds type T
// std::get<T>(v)                → extract the T (throws if wrong type)

struct BencodeValue;                                        // forward-declare for recursion
using BencodeList = std::vector<BencodeValue>;
using BencodeDict = std::map<std::string, BencodeValue>;   // keys are always strings

struct BencodeValue {
    std::variant<int64_t, std::string, BencodeList, BencodeDict> data;

    // Type checks
    bool is_int()  const { return std::holds_alternative<int64_t>(data); }
    bool is_str()  const { return std::holds_alternative<std::string>(data); }
    bool is_list() const { return std::holds_alternative<BencodeList>(data); }
    bool is_dict() const { return std::holds_alternative<BencodeDict>(data); }

    // Mutable accessors (throw std::bad_variant_access if wrong type)
    int64_t&     as_int()  { return std::get<int64_t>(data); }
    std::string& as_str()  { return std::get<std::string>(data); }
    BencodeList& as_list() { return std::get<BencodeList>(data); }
    BencodeDict& as_dict() { return std::get<BencodeDict>(data); }

    // Const accessors
    const int64_t&     as_int()  const { return std::get<int64_t>(data); }
    const std::string& as_str()  const { return std::get<std::string>(data); }
    const BencodeList& as_list() const { return std::get<BencodeList>(data); }
    const BencodeDict& as_dict() const { return std::get<BencodeDict>(data); }
};

namespace bencode {
    // Decode one bencode value from data starting at pos. Advances pos past it.
    BencodeValue decode(const std::string& data, size_t& pos);

    // Convenience: decode from the start of data
    inline BencodeValue decode(const std::string& data) {
        size_t pos = 0;
        return decode(data, pos);
    }

    // Encode a BencodeValue back to a bencode-formatted byte string
    std::string encode(const BencodeValue& val);

    // decode helpers
    int parseInt(const std::string& data, size_t& pos);
    std::string parseStr(std::string& data, int pos);
}
