//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// string_functions.cpp
//
// Identification: src/function/string_functions.cpp
//
// Copyright (c) 2015-2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "function/string_functions.h"

#include "common/macros.h"
#include "executor/executor_context.h"

namespace peloton {
namespace function {

uint32_t StringFunctions::Ascii(UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
                                const char *str, uint32_t length) {
  PL_ASSERT(str != nullptr);
  return length <= 1 ? 0 : static_cast<uint32_t>(str[0]);
}

#define NextByte(p, plen) ((p)++, (plen)--)

bool StringFunctions::Like(UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
                           const char *t, uint32_t tlen, const char *p,
                           uint32_t plen) {
  PL_ASSERT(t != nullptr);
  PL_ASSERT(p != nullptr);
  if (plen == 1 && *p == '%') return true;

  while (tlen > 0 && plen > 0) {
    if (*p == '\\') {
      NextByte(p, plen);
      if (plen <= 0) return false;
      if (tolower(*p) != tolower(*t)) return false;
    } else if (*p == '%') {
      char firstpat;
      NextByte(p, plen);

      while (plen > 0) {
        if (*p == '%')
          NextByte(p, plen);
        else if (*p == '_') {
          if (tlen <= 0) return false;
          NextByte(t, tlen);
          NextByte(p, plen);
        } else
          break;
      }

      if (plen <= 0) return true;

      if (*p == '\\') {
        if (plen < 2) return false;
        firstpat = tolower(p[1]);
      } else
        firstpat = tolower(*p);

      while (tlen > 0) {
        if (tolower(*t) == firstpat) {
          int matched = Like(ctx, t, tlen, p, plen);

          if (matched != false) return matched;
        }

        NextByte(t, tlen);
      }
      return false;
    } else if (*p == '_') {
      NextByte(t, tlen);
      NextByte(p, plen);
      continue;
    } else if (tolower(*p) != tolower(*t)) {
      return false;
    }
    NextByte(t, tlen);
    NextByte(p, plen);
  }

  if (tlen > 0) return false;

  while (plen > 0 && *p == '%') NextByte(p, plen);
  if (plen <= 0) return true;

  return false;
}

#undef NextByte

StringFunctions::StrWithLen StringFunctions::Substr(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_length, int32_t from, int32_t len) {
  int32_t signed_end = from + len - 1;
  if (signed_end < 0 || str_length == 0) {
    return StringFunctions::StrWithLen{nullptr, 0};
  }

  uint32_t begin = from > 0 ? unsigned(from) - 1 : 0;
  uint32_t end = unsigned(signed_end);

  if (end > str_length) {
    end = str_length;
  }

  if (begin > end) {
    return StringFunctions::StrWithLen{nullptr, 0};
  }

  return StringFunctions::StrWithLen{str + begin, end - begin + 1};
}

StringFunctions::StrWithLen StringFunctions::Repeat(
    executor::ExecutorContext &ctx, const char *str, uint32_t length,
    uint32_t num_repeat) {
  // Determine the number of bytes we need
  uint32_t total_len = ((length - 1) * num_repeat) + 1;

  // Allocate new memory
  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(total_len));

  // Perform repeat
  char *ptr = new_str;
  for (uint32_t i = 0; i < num_repeat; i++) {
    PL_MEMCPY(ptr, str, length - 1);
    ptr += (length - 1);
  }

  // We done
  return StringFunctions::StrWithLen{new_str, total_len};
}

StringFunctions::StrWithLen StringFunctions::LTrim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len, const char *from, UNUSED_ATTRIBUTE uint32_t from_len) {
  PL_ASSERT(str != nullptr && from != nullptr);

  // llvm expects the len to include the terminating '\0'
  if (str_len == 1) {
    return StringFunctions::StrWithLen{nullptr, 1};
  }

  str_len -= 1;
  int tail = str_len - 1, head = 0;

  while (head < (int) str_len && strchr(from, str[head]) != nullptr) {
    head++;
  }

  // Determine length and return
  auto new_len = static_cast<uint32_t>(std::max(tail - head + 1, 0) + 1);
  return StringFunctions::StrWithLen{str + head, new_len};
}

StringFunctions::StrWithLen StringFunctions::RTrim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len, const char *from, UNUSED_ATTRIBUTE uint32_t from_len) {
  PL_ASSERT(str != nullptr && from != nullptr);

  // llvm expects the len to include the terminating '\0'
  if (str_len == 1) {
    return StringFunctions::StrWithLen{nullptr, 1};
  }

  str_len -= 1;
  int tail = str_len - 1, head = 0;
  while (tail >= 0 && strchr(from, str[tail]) != nullptr) {
    tail--;
  }

  auto new_len = static_cast<uint32_t>(std::max(tail - head + 1, 0) + 1);
  return StringFunctions::StrWithLen{str + head, new_len};
}

StringFunctions::StrWithLen StringFunctions::Trim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len) {
  return BTrim(ctx, str, str_len, " ", 2);
}

StringFunctions::StrWithLen StringFunctions::BTrim(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx, const char *str,
    uint32_t str_len, const char *from, UNUSED_ATTRIBUTE uint32_t from_len) {
  PL_ASSERT(str != nullptr && from != nullptr);

  // Skip the tailing 0
  str_len--;

  if (str_len == 0) {
    return StringFunctions::StrWithLen{str, 1};
  }

  int head = 0;
  int tail = str_len - 1;

  // Trim tail
  while (tail >= 0 && strchr(from, str[tail]) != nullptr) {
    tail--;
  }

  // Trim head
  while (head < (int) str_len && strchr(from, str[head]) != nullptr) {
    head++;
  }

  // Done
  auto new_len = static_cast<uint32_t>(std::max(tail - head + 1, 0) + 1);
  return StringFunctions::StrWithLen{str + head, new_len};
}

uint32_t StringFunctions::Length(
    UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
    UNUSED_ATTRIBUTE const char *str, uint32_t length) {
  PL_ASSERT(str != nullptr);
  return length;
}
char *StringFunctions::Upper(UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
                             const char *str,
                             const uint32_t length) {
  PL_ASSERT(str != nullptr);

  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(length));
  assert(new_str);
  for (uint32_t i = 0; i < length; i++) {
    new_str[i] = static_cast<char>(std::toupper(str[i]));
  }

  return new_str;
}

char *StringFunctions::Lower(UNUSED_ATTRIBUTE executor::ExecutorContext &ctx,
                             const char *str,
                             const uint32_t length) {
  PL_ASSERT(str != nullptr);

  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(length));

  for (uint32_t i = 0; i < length; i++) {
    new_str[i] = static_cast<char>(std::tolower(str[i]));
  }

  return new_str;
}
StringFunctions::StrWithLen StringFunctions::Concat(executor::ExecutorContext &ctx,
                                                    const char **concat_strs,
                                                    const uint32_t *strs_length,
                                                    const uint32_t size) {
  PL_ASSERT(concat_strs != nullptr && strs_length != nullptr);
  uint32_t total_length = 0;
  for (uint32_t i = 0; i < size; i++) {
    const char *cur = concat_strs[i];
    if(cur == nullptr || *cur == '\0'){
      continue;
    }
    if (strcmp(cur, "NULL") == 0) {
      continue;
    }
    uint32_t curLen = strs_length[i] - 1;
    total_length += curLen;
  }

  total_length = total_length + 1;

  auto *pool = ctx.GetPool();
  auto *new_str = reinterpret_cast<char *>(pool->Allocate(total_length));
  auto ptr = new_str;
  for (uint32_t i = 0; i < size; i++) {
    const char *cur = concat_strs[i];
    if(cur == nullptr || *cur == '\0'){
      continue;
    }
    if (strcmp(cur, "NULL") == 0) {
      continue;
    }
    uint32_t curLen = strs_length[i] - 1;
    PL_MEMCPY(ptr, concat_strs[i], curLen);
    ptr += curLen;
  }
  new_str[total_length] = '\0';
  return StringFunctions::StrWithLen{new_str, total_length};
}

type::Value StringFunctions::UpperAdaptor(const std::vector<type::Value> &args) {
  PL_ASSERT(args.size() == 1);
  if (args[0].IsNull()) {
    return type::ValueFactory::GetNullValueByType(type::TypeId::VARCHAR);
  }

  executor::ExecutorContext ctx{nullptr};
  auto ret = StringFunctions::Upper(ctx, args[0].GetAs<const char *>(),
                                    args[0].GetLength());
  return type::ValueFactory::GetVarcharValue(ret);
}
type::Value StringFunctions::LowerAdaptor(const std::vector<type::Value> &args) {
  PL_ASSERT(args.size() == 1);
  if (args[0].IsNull()) {
    return type::ValueFactory::GetNullValueByType(type::TypeId::VARCHAR);
  }

  executor::ExecutorContext ctx{nullptr};
  auto ret = StringFunctions::Lower(ctx, args[0].GetAs<const char *>(),
                                    args[0].GetLength());
  return type::ValueFactory::GetVarcharValue(ret);
}
type::Value StringFunctions::ConcatAdaptor(const std::vector<type::Value> &args) {
  PL_ASSERT(args.size() == 2);
  std::cout << "in concatadaptor" << std::endl;
  if (args[0].IsNull() || args[1].IsNull()) {
    return type::ValueFactory::GetNullValueByType(type::TypeId::VARCHAR);
  }
  executor::ExecutorContext ctx{nullptr};
  const char *v[2] = {args.at(0).GetData(), args.at(1).GetData()};
  uint32_t s[2] = {
      static_cast<uint32_t>(strlen(args.at(0).GetData()) + 1),
      static_cast<uint32_t>(strlen(args.at(1).GetData()) + 1)};
  auto ret = StringFunctions::Concat(ctx, v, s, 2);

  std::string str(ret.str, ret.length - 1);
  return type::ValueFactory::GetVarcharValue(str);
}
}  // namespace function
}  // namespace peloton
