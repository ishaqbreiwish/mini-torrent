#include "bencode.hpp"
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>

namespace bencode {

// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 1): implement decode
//
// BENCODE FORMAT:
//   Integer  →  i <decimal> e          e.g.  i42e   i-7e   i0e
//   String   →  <length> : <bytes>     e.g.  4:spam   0:
//   List     →  l <value>* e           e.g.  li1ei2ee  →  [1, 2]
//   Dict     →  d (<key> <value>)* e   e.g.  d3:fooi1ee  →  {"foo": 1}
//               keys must be strings and come in sorted order
//
// STRATEGY — look at data[pos] and branch:
//
//   'i'      → integer
//   'l'      → list
//   'd'      → dict
//   '0'-'9'  → string  (the digit starts the length prefix)
//   else     → throw std::runtime_error("bencode: unexpected char")
//
// ── PARSING INTEGER ──────────────────────────────────────────────────────────
//   data[pos] == 'i'; advance pos past it.
//   Find 'e' from pos: size_t e_pos = data.find('e', pos);
//   Extract digit string: data.substr(pos, e_pos - pos)
//   Convert with std::stoll().
//   Set pos = e_pos + 1  (past the 'e').
//   Return BencodeValue{the integer}.
//
// ── PARSING STRING ───────────────────────────────────────────────────────────
//   data[pos] is a digit. Find ':' from pos.
//   Length = std::stoll(data.substr(pos, colon_pos - pos))
//   Set pos = colon_pos + 1  (past the ':').
//   Extract data.substr(pos, length), set pos += length.
//   Return BencodeValue{the string}.
//
// ── PARSING LIST ─────────────────────────────────────────────────────────────
//   data[pos] == 'l'; advance pos.
//   Create BencodeList list;
//   while (data[pos] != 'e'):
//     list.push_back(decode(data, pos));   ← recursive!
//   Advance pos past 'e'.
//   Return BencodeValue{list}.
//
// ── PARSING DICT ─────────────────────────────────────────────────────────────
//   data[pos] == 'd'; advance pos.
//   Create BencodeDict dict;
//   while (data[pos] != 'e'):
//     BencodeValue key = decode(data, pos);   ← must be a string
//     BencodeValue val = decode(data, pos);
//     dict[key.as_str()] = val;
//   Advance pos past 'e'.
//   Return BencodeValue{dict}.
// ─────────────────────────────────────────────────────────────────────────────


int parseInt(const std::string& data, int pos) {
    int endPos = pos;
    while (data[endPos] != 'e') {
        endPos++;
        if (pos == data.size()) {
            throw std::runtime_error("bencode: unexpected end of input at pos, integer did not terminate");
        }
    }

    std::string sub = data.substr(pos, (endPos - pos));
    pos = endPos + 1;
    return std::stoll(sub);
}

std::string parseStr(const std::string& data, int pos) {
    std::string len_str = "";

    while(data[pos] != ':') {
        len_str += data[pos];
        pos++;
        if (pos == data.size()) {
            throw std::runtime_error("bencode: expected colon at " + std::to_string(pos));
        }
    }

    int len_int = std::stoll(len_str);
    pos++;
    std::string result = data.substr(pos, len_int);
    pos += len_int;
    return result;
}

BencodeValue decode(const std::string& data, size_t& position) {
    if (position >= data.size())
        throw std::runtime_error("bencode: unexpected end of input at pos " + std::to_string(position));


    for (int pos = position; pos < data.size(); pos++) {
        if (data[pos] == 'i') {
            pos++;
            int val = parseInt(data, pos);
            return BencodeValue{val};
        }
        else if (std::isdigit(data[pos])) {
            std::string val = parseStr(data, pos);
            return BencodeValue{val};
        }
        else if (data[pos] == 'l') {
            throw std::runtime_error("bencode: lists not yet implemented");
        }
        else if (data[pos] == 'd') {
            throw std::runtime_error("bencode: dicts not yet implemented");
        }

        throw std::runtime_error("bencode: unknown type at pos " + std::to_string(pos));
    }
}



// ─────────────────────────────────────────────────────────────────────────────
// TODO (Phase 1): implement encode
//
// Rules:
//   integer  →  "i" + std::to_string(n) + "e"
//   string   →  std::to_string(s.size()) + ":" + s
//   list     →  "l" + encode(elem0) + encode(elem1) + ... + "e"
//   dict     →  "d" + encode(key0) + encode(val0) + ... + "e"
//               std::map iterates keys in sorted order — bencode requires this
//
// OPTION A — simple if/else:
//   if (val.is_int())  { ... }
//   else if (val.is_str()) { ... }
//   ...
//
// OPTION B — std::visit (idiomatic C++17):
//   return std::visit([](auto& v) -> std::string {
//       using T = std::decay_t<decltype(v)>;
//       if constexpr (std::is_same_v<T, int64_t>)     { return "i" + ...; }
//       if constexpr (std::is_same_v<T, std::string>) { return ...; }
//       if constexpr (std::is_same_v<T, BencodeList>) { ... }
//       if constexpr (std::is_same_v<T, BencodeDict>) { ... }
//   }, val.data);
// ─────────────────────────────────────────────────────────────────────────────
std::string encode(const BencodeValue& val) {
    // TODO: implement this — remove the lines below and replace with your code
    (void)val;
    throw std::runtime_error("bencode::encode not implemented");
}

} // namespace bencode
