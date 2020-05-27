#include <cassert>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <string>
#include <tuple>
#include <vector>

// #define LOGIR
// #define DEBUG

#ifdef DEBUG
#  define dprintf(...) fprintf(stdout, __VA_ARGS__)
#else
#  define dprintf(fmt, ...)
#endif

#include "irsim.h"

namespace irsim {

/* clang-format off */
std::map<Opc, std::string> opc_to_string{
    {Opc::abort, "abort"}, {Opc::helper, "helper"},
    {Opc::alloca, "alloca"}, {Opc::la, "la"},
    {Opc::ld, "ld"}, {Opc::st, "st"}, {Opc::li, "li"},
    {Opc::mov, "mov"}, {Opc::add, "add"}, {Opc::sub, "sub"},
    {Opc::mul, "mul"}, {Opc::div, "div"}, {Opc::jmp, "jmp"},
    {Opc::br, "br"}, {Opc::slt, "slt"}, {Opc::sle, "sle"},
    {Opc::seq, "seq"}, {Opc::sge, "sge"}, {Opc::sgt, "sgt"},
    {Opc::sne, "sne"}, {Opc::call, "call"}, {Opc::ret, "ret"},
    {Opc::mfcr, "mfcr"}, {Opc::mtcr, "mtcr"},
    {Opc::quit, "quit"}, {Opc::mark, "mark"},
};
/* clang-format on */

int Program::run(int *eip) {
  /* clang-format off */
  int _start[] = {
      (int)Opc::alloca, 1,
      (int)Opc::call, ptr_lo(eip), ptr_hi(eip),
      (int)Opc::mfcr, 0, CR_RET,
      (int)Opc::quit, 0,
  };
  /* clang-format on */

  eip = &_start[0];

  while (true) {
#ifdef DEBUG
    auto oldeip = eip;
#endif
    if (eip == nullptr) {
      exception = Exception::IF;
      return -1;
    }

    int opc = *eip++;

#ifdef DEBUG
    dprintf("stack:\n");
    for (auto d = 0u; d < stack.size(); d++) {
      auto &s = stack[d];
      constexpr int step = 6;
      for (auto i = 0u; i < s.size(); i += step) {
        if (i == 0)
          dprintf("%02d:%02d:", d, i);
        else
          dprintf("  :%02d:", i);
        for (auto j = i; j < i + step && j < s.size();
             j++) {
          dprintf(" %08x", s[j]);
        }
        dprintf("\n");
      }
    }
    dprintf("crs:  ");
    for (auto i = 0u; i < ctrl_regs.size(); i++)
      dprintf(" %08x", ctrl_regs[i]);
    dprintf("\n");
#endif

    switch ((Opc)opc) {
    case Opc::abort:
      exception = Exception::ABORT;
      return -1;
    case Opc::helper: {
      int ptrlo = *eip++;
      int ptrhi = *eip++;
      int nr_args = *eip++;
      using F = void(int *);
      F *f = lohi_to_ptr<F>(ptrlo, ptrhi);
      f(eip);
      eip += nr_args;
      dprintf("%p: helper %p\n", oldeip, (void *)f);
    } break;
    case Opc::la: {
      int to = *eip++;
      int from = *eip++;
      stack.back().at(to) =
          ((stack.back().begin() + from) - memory.begin()) *
          4;
      dprintf("%p: la %d, %d\n", oldeip, to, from);
    } break;
    case Opc::ld: {
      int to = *eip++;
      int from = stack.back().at(*eip++);
      dprintf("%p: ld %d, (%d)\n", oldeip, to, from);
      if (memory.size() * 4 <= (size_t)from) {
        exception = Exception::LOAD;
        return -1;
      } else {
        uint8_t *from_ptr = (uint8_t *)&memory[0] + from;
        uint8_t *to_ptr = (uint8_t *)&stack.back().at(to);
        memcpy(to_ptr, from_ptr, sizeof(int));
      }
    } break;
    case Opc::st: {
      int to = stack.back().at(*eip++);
      int from = *eip++;
      dprintf("%p: st (%d), %d\n", oldeip, to, from);
      if (memory.size() * 4 <= (size_t)to) {
        exception = Exception::STORE;
        return -1;
      } else {
        uint8_t *from_ptr =
            (uint8_t *)&stack.back().at(from);
        uint8_t *to_ptr = (uint8_t *)&memory[0] + to;
        memcpy(to_ptr, from_ptr, sizeof(int));
      }
    } break;
    case Opc::li: {
      int to = *eip++;
      int lhs = *eip++;
      stack.back().at(to) = lhs;
      if (lhs < 0 || lhs > 256)
        dprintf("%p: li %d %08x\n", oldeip, to, lhs);
      else
        dprintf("%p: li %d %d\n", oldeip, to, lhs);
    } break;
    case Opc::mov: {
      int to = *eip++;
      int lhs = *eip++;
      dprintf("%p: mov %d %d\n", oldeip, to, lhs);
      stack.back().at(to) = stack.back().at(lhs);
    } break;
    case Opc::add: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) + stack.back().at(rhs);
      dprintf("%p: add %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::sub: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) - stack.back().at(rhs);
      dprintf("%p: sub %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::mul: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) * stack.back().at(rhs);
      dprintf("%p: mul %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::div: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      if (stack.back().at(rhs) == 0) {
        exception = Exception::DIV_ZERO;
        return -1;
      } else if (stack.back().at(lhs) == INT_MIN &&
                 stack.back().at(rhs) == -1) {
        exception = Exception::OF;
        return -1;
      } else {
        int lhsVal = stack.back().at(lhs);
        int rhsVal = stack.back().at(rhs);
        if (lhsVal < 0 && rhsVal > 0) {
          stack.back().at(to) =
              (lhsVal - rhsVal + 1) / rhsVal;
        } else if (lhsVal > 0 && rhsVal < 0) {
          stack.back().at(to) =
              (lhsVal - rhsVal - 1) / rhsVal;
        } else {
          stack.back().at(to) = lhsVal / rhsVal;
        }
      }
      dprintf("%p: div %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::jmp: {
      uint64_t ptrlo = *eip++;
      uint64_t ptrhi = *eip++;
      dprintf("%p: jmp %p\n", oldeip,
          lohi_to_ptr<void>(ptrlo, ptrhi));
      eip = lohi_to_ptr<int>(ptrlo, ptrhi);
    } break;
    case Opc::br: {
      int cond = stack.back().at(*eip++);
      uint64_t ptrlo = *eip++;
      uint64_t ptrhi = *eip++;
      dprintf("%p: cond %d br %p\n", oldeip, cond,
          lohi_to_ptr<void>(ptrlo, ptrhi));
      if (cond) { eip = lohi_to_ptr<int>(ptrlo, ptrhi); }
    } break;
    case Opc::slt: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) < stack.back().at(rhs);
      dprintf("%p: slt %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::sle: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) <= stack.back().at(rhs);
      dprintf("%p: sle %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::seq: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) == stack.back().at(rhs);
      dprintf("%p: seq %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::sge: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) >= stack.back().at(rhs);
      dprintf("%p: sge %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::sgt: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) > stack.back().at(rhs);
      dprintf("%p: sgt %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::sne: {
      int to = *eip++;
      int lhs = *eip++;
      int rhs = *eip++;
      stack.back().at(to) =
          stack.back().at(lhs) != stack.back().at(rhs);
      dprintf("%p: sne %d, %d, %d\n", oldeip, to, lhs, rhs);
    } break;
    case Opc::call: {
      int ptrlo = *eip++;
      int ptrhi = *eip++;
      int *target = lohi_to_ptr<int>(ptrlo, ptrhi);
      frames.push_back(eip);
      int ptr = (stack.back().begin() - memory.begin()) +
                stack.back().size();
      stack.emplace_back(memory, ptr, 0);
      eip = target;
      dprintf("%p: call %p\n", oldeip, eip);
    } break;
    case Opc::ret: {
      assert(stack.size() >= 1);
      stack.pop_back();
      eip = frames.back();
      frames.pop_back();
      dprintf("%p: ret %p\n", oldeip, eip);
    } break;
    case Opc::alloca: {
      int size = *eip++;
      dprintf("%p: alloca %d\n", oldeip, size);
      if (stack.empty()) stack.emplace_back(memory, 0, 0);

      int ptr = stack.back().begin() - memory.begin();
      size_t newMemSize = ptr + size;
      if (newMemSize >= memory_limit) {
        exception = Exception::OOM;
        return -1;
      } else if (newMemSize > memory.size()) {
        memory.resize(newMemSize);
      }
      stack.back().resize(size);
    } break;
    case Opc::mfcr: {
      int to = *eip++;
      int from = *eip++;
      switch (from) {
      case CR_COUNT:
        stack.back().at(to) = ctrl_regs.at(from);
        dprintf("%p: mfcr %d, cr_count\n", oldeip, to);
        break;
      case CR_RET:
        stack.back().at(to) = ctrl_regs.at(from);
        dprintf("%p: mfcr %d, cr_ret\n", oldeip, to);
        break;
      case CR_SERIAL:
        stack.back().at(to) = io.read();
        dprintf("%p: mfcr %d, cr_serial\n", oldeip, to);
        break;
      case CR_ARG:
        if (args.size()) {
          stack.back().at(to) = args.back();
          args.pop_back();
        }
        dprintf("%p: mfcr %d, cr_arg\n", oldeip, to);
        break;
      default: abort();
      }
    } break;
    case Opc::mtcr: {
      int to = *eip++;
      int from = *eip++;
      switch (to) {
      case CR_COUNT:
        dprintf("%p: mtcr cr_count, %d\n", oldeip, from);
        ctrl_regs.at(to) = stack.back().at(from);
        break;
      case CR_RET:
        ctrl_regs.at(to) = stack.back().at(from);
        dprintf("%p: mtcr cr_ret, %d\n", oldeip, from);
        break;
      case CR_SERIAL:
        io.write(stack.back().at(from));
        dprintf("%p: mtcr cr_serial, %d\n", oldeip, from);
        break;
      case CR_ARG:
        args.push_back(stack.back().at(from));
        dprintf("%p: mtcr cr_arg, %d\n", oldeip, from);
        break;
      default: abort();
      }
    } break;
    case Opc::mark:
      ctrl_regs[CR_COUNT]++;
      dprintf("%p: mark\n", oldeip);
      if (ctrl_regs[CR_COUNT] >= insts_limit) {
        exception = Exception::TIMEOUT;
        return -1;
      }
      break;
    case Opc::quit: return stack.back().at(*eip++);
    default: exception = Exception::INVOP; return -1;
    }
  }
  return 0;
}

int Compiler::primary_exp(
    Program *prog, const std::string &tok, int to) {
  if (tok[0] == '#') {
    if (to == INT_MAX) to = newTemp();
    long long value;
    sscanf(&tok[1], "%lld", &value);
    prog->gen_inst(Opc::li, to, (int)value);
    return to;
  } else if (tok[0] == '&') {
    auto var = getVar(&tok[1]);
    if (to == INT_MAX) to = newTemp();
    prog->gen_inst(Opc::la, to, var);
    return to;
  } else if (tok[0] == '*') {
    auto var = getVar(&tok[1]);
    if (to == INT_MAX) to = newTemp();
    prog->gen_inst(Opc::ld, to, var);
    return to;
  } else {
    if (to != INT_MAX) {
      prog->gen_inst(Opc::mov, to, getVar(tok));
      return to;
    } else {
      return getVar(tok);
    }
  }
}

/* stmt label */
bool Compiler::handle_label(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(R"(^\s*LABEL\s+(\S+)\s*:\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);

  if (it == std::sregex_token_iterator()) return false;

  auto label = *it++;
  auto label_ptr = prog->get_textptr();
  labels[label] = label_ptr;
  dprintf(
      "add label %s, %p\n", label.str().c_str(), label_ptr);
  for (auto *ptr : backfill_labels[label]) {
    ptr[0] = ptr_lo(label_ptr);
    ptr[1] = ptr_hi(label_ptr);
  }
  backfill_labels.erase(label);
  return true;
}

/* stmt func */
bool Compiler::handle_func(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(R"(^\s*FUNCTION\s+(\S+)\s*:\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) return false;

  auto f = *it++;

  prog->gen_inst(
      Opc::abort); // last function should manually ret
  funcs[f] = prog->get_textptr();

  if (prog->curf[0] == (int)Opc::alloca) {
    dprintf("alloca %d\n", stack_size + 1);
    prog->curf[1] = stack_size + 1;
    clear_env();
  }
  prog->curf = prog->gen_inst(Opc::alloca, 0);
  return true;
}

bool Compiler::handle_assign(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*([^*]\S*)\s*:=\s*(#[\+\-]?\d+|[&\*]?\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m2);
  if (it == std::sregex_token_iterator()) return false;

  auto x = getVar(*it++);
  primary_exp(prog, *it++, x);
  return true;
}

bool Compiler::handle_arith(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*(\S+)\s*:=\s*(#[\+\-]?\d+|[&\*]?\S+)\s*(\+|\-|\*|\/)\s*(#[\+\-]?\d+|[&\*]?\S+)\s*$)");

  static std::map<std::string, Opc> m{
      {"+", Opc::add},
      {"-", Opc::sub},
      {"*", Opc::mul},
      {"/", Opc::div},
  };

  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m4);
  if (it == std::sregex_token_iterator()) return false;

  auto x = getVar(*it++);
  auto y = primary_exp(prog, *it++);
  auto op = *it++;
  auto z = primary_exp(prog, *it++);

  prog->gen_inst(m[op], x, y, z);
  return true;
}

bool Compiler::handle_takeaddr(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*(\S+)\s*:=\s*&\s*(\S+)\s*$)");
  /* stmt assign */
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m2);
  if (it == std::sregex_token_iterator()) return false;
  auto x = *it++;
  auto y = *it++;
  prog->gen_inst(Opc::la, getVar(x), getVar(y));
  return true;
}

bool Compiler::handle_deref(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*([^*]\S+)\s*:=\s*\*\s*(\S+)\s*$)");
  /* stmt assign */
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m2);
  if (it == std::sregex_token_iterator()) return false;
  auto x = getVar(*it++);
  auto y = getVar(*it++);
  auto tmp = newTemp();
  prog->gen_inst(Opc::la, tmp, y);
  prog->gen_inst(Opc::ld, x, y);
  return true;
}

bool Compiler::handle_deref_assign(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*\*(\S+)\s+:=\s+(#[\+\-]?\d+|[&\*]?\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m2);
  if (it == std::sregex_token_iterator()) return false;

  auto x = getVar(*it++);
  auto y = primary_exp(prog, *it++);
  prog->gen_inst(Opc::st, x, y);
  return true;
}

bool Compiler::handle_goto_(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(R"(^\s*GOTO\s+(\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) { return false; }

  auto label = *it++;
  auto label_ptr = labels[label];
  auto code = prog->gen_jmp(label_ptr);
  if (!label_ptr) {
    backfill_labels[label].push_back(code + 1);
  }
  return true;
}

bool Compiler::handle_branch(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*IF\s+(#[\+\-]?\d+|[&\*]?\S+)\s*(<|>|<=|>=|==|!=)\s*(#[\+\-]?\d+|[&\*]?\S+)\s+GOTO\s+(\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m4);
  if (it == std::sregex_token_iterator()) return false;

  auto x = primary_exp(prog, *it++);
  auto opc = *it++;
  auto y = primary_exp(prog, *it++);

  auto label = *it++;
  auto label_ptr = labels[label];

  static std::map<std::string, Opc> s2op{
      {"<", Opc::slt},
      {">", Opc::sgt},
      {"<=", Opc::sle},
      {">=", Opc::sge},
      {"==", Opc::seq},
      {"!=", Opc::sne},
  };

  auto tmp = newTemp();
  prog->gen_inst(s2op[opc], tmp, x, y);
  auto code = prog->gen_br(tmp, label_ptr);
  if (!label_ptr) {
    backfill_labels[label].push_back(code + 2);
  }
  return true;
}

bool Compiler::handle_ret(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*RETURN\s+(#[\+\-]?\d+|[&\*]?\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) return false;

  auto x = primary_exp(prog, *it++);
  prog->gen_inst(Opc::mtcr, CR_RET, x);
  prog->gen_inst(Opc::ret);
  return true;
}

bool Compiler::handle_dec(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(R"(^\s*DEC\s+(\S+)\s+(\d+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m2);
  if (it == std::sregex_token_iterator()) return false;

  auto x = *it++;
  auto size = stoi(*it++);
  getVar(x, (size + 3) / 4);
  return true;
}

bool Compiler::handle_arg(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*ARG\s+(#[\+\-]?\d+|[&\*]?\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) return false;

  auto tmp = primary_exp(prog, *it++);
  prog->gen_inst(Opc::mtcr, CR_ARG, tmp);
  return true;
}

bool Compiler::handle_call(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*(\S+)\s*:=\s*CALL\s+(\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m2);
  if (it == std::sregex_token_iterator()) return false;

  auto to = *it++;
  auto f = *it++;

  prog->gen_call(funcs[f]);
  prog->gen_inst(Opc::mfcr, getVar(to), CR_RET);
  return true;
}

bool Compiler::handle_param(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(R"(^\s*PARAM\s+(\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) return false;
  prog->gen_inst(Opc::mfcr, getVar(*it++), CR_ARG);
  return true;
}

bool Compiler::handle_read(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(R"(^\s*READ\s+(\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) return false;

  auto x = *it++;
  prog->gen_inst(Opc::mfcr, getVar(x), CR_SERIAL);
  return true;
}

bool Compiler::handle_write(
    Program *prog, const std::string &line) {
  dprintf("at %s, %d\n", __func__, __LINE__);
  static std::regex pat(
      R"(^\s*WRITE\s+(#[\+\-]?\d+|[&\*]?\S+)\s*$)");
  auto it = std::sregex_token_iterator(
      line.begin(), line.end(), pat, m1);
  if (it == std::sregex_token_iterator()) return false;

  auto x = primary_exp(prog, *it++);
  prog->gen_inst(Opc::mtcr, CR_SERIAL, x);
  return true;
}

void log_curir(int *eip) {
  const char *s = lohi_to_ptr<char>(eip[0], eip[1]);
  fprintf(stdout, "IR:%03d> %s\n", eip[2], s);
}

std::unique_ptr<Program> Compiler::compile(
    std::istream &is) {
#ifdef LOGIR
  static std::vector<std::unique_ptr<char[]>> lines;
#endif
  auto prog = std::make_unique<Program>();
  unsigned lineno = 0;
  while ((is.peek(), is.good())) {
    lineno++;
    std::string line;
    std::getline(is, line);

    prog->gen_inst(Opc::mark);

#ifdef LOGIR
    char *ir = new char[line.size() + 1];
    strcpy(ir, line.c_str());
    lines.push_back(std::unique_ptr<char[]>(ir));
    prog->gen_inst(Opc::helper, ptr_lo((void *)log_curir),
        ptr_hi((void *)log_curir), 3, ptr_lo(ir),
        ptr_hi(ir), lineno);
#endif

    dprintf("compile %s\n", line.c_str());

    if (line.find_first_not_of("\r\n\v\f\t ") ==
        line.npos) {
      continue;
    }

    clearTemps();
    std::vector<std::string> tokens = splitTokens(line);
    bool suc = false;
    if (tokens.size() == 0) {
      suc = true;
    } else if (tokens[0] == "LABEL") {
      suc = handle_label(&*prog, line);
    } else if (tokens[0] == "FUNCTION") {
      suc = handle_func(&*prog, line);
    } else if (tokens[0] == "GOTO") {
      suc = handle_goto_(&*prog, line);
    } else if (tokens[0] == "IF") {
      suc = handle_branch(&*prog, line);
    } else if (tokens[0] == "RETURN") {
      suc = handle_ret(&*prog, line);
    } else if (tokens[0] == "DEC") {
      suc = handle_dec(&*prog, line);
    } else if (tokens[0] == "ARG") {
      suc = handle_arg(&*prog, line);
    } else if (tokens.size() == 4 && tokens[1] == ":=" &&
             tokens[2] == "CALL") {
      suc = handle_call(&*prog, line);
    } else if (tokens[0] == "PARAM") {
      suc = handle_param(&*prog, line);
    } else if (tokens[0] == "READ") {
      suc = handle_read(&*prog, line);
    } else if (tokens[0] == "WRITE") {
      suc = handle_write(&*prog, line);
    } else {
      std::vector<bool (Compiler::*)(
          Program *, const std::string &)>
          handlers{
              &Compiler::handle_assign,
              &Compiler::handle_arith,
              &Compiler::handle_takeaddr,
              &Compiler::handle_deref,
              &Compiler::handle_deref_assign,
          };

      for (auto &h : handlers) {
        suc = (this->*h)(&*prog, line);
        if (suc) break;
      }
    }

    if (suc) continue;

    fprintf(stderr,
        "[IGNORED] syntax error at line %d: '%s'\n", lineno,
        line.c_str());
    /* IGNORED and continue */
  }

  if (prog->curf[0] == (int)Opc::alloca) {
    dprintf("alloca %d\n", stack_size + 2);
    prog->curf[1] = stack_size + 2;
  }
  prog->gen_inst(Opc::abort);
  return prog;
}

} // namespace irsim
