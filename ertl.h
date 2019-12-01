#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "ast.h"
#include "rtl.h"

/** This defines the ERTL intermediate language */

#ifndef CONSTRUCTOR
#define CONSTRUCTOR(Cls, ...)                                                  \
  template <typename... Args>                                                  \
  static std::unique_ptr<Cls const> make(Args &&... args) {                    \
    return std::unique_ptr<Cls>{new Cls(std::forward<Args>(args)...)};         \
  }                                                                            \
                                                                               \
private:                                                                       \
  explicit Cls(__VA_ARGS__)
#endif

namespace bx {

namespace ertl {

using Label = bx::rtl::Label;
using Pseudo = bx::rtl::Pseudo;
enum class Mach : int8_t {
  // clang-format off
  RAX, RBX, RCX, RDX, RBP, RDI, RSI, RSP,
  R8, R9, R10, R11, R12, R13, R14, R15
  // clang-format on
};
char const *to_string(Mach m);
std::ostream &operator<<(std::ostream &out, Mach m);
extern const ertl::Mach callee_saves[5];
extern const ertl::Mach input_regs[6];

struct Instr;
using InstrPtr = std::unique_ptr<Instr const>;

struct Move;
struct Copy;
struct GetMach;
struct SetMach;
struct Load;
struct LoadParam;
struct Store;
struct Binop;
struct Unop;
struct Bbranch;
struct Ubranch;
struct Goto;
struct Push;
struct Pop;
struct Call;
struct Return;
struct Newframe;
struct Delframe;

struct InstrVisitor {
  virtual ~InstrVisitor() = default;
#define VISIT_FUNCTION(caseclass)                                              \
  virtual void visit(Label const &, caseclass const &) = 0
  VISIT_FUNCTION(Move);
  VISIT_FUNCTION(Copy);
  VISIT_FUNCTION(GetMach);
  VISIT_FUNCTION(SetMach);
  VISIT_FUNCTION(Load);
  VISIT_FUNCTION(LoadParam);
  VISIT_FUNCTION(Store);
  VISIT_FUNCTION(Binop);
  VISIT_FUNCTION(Unop);
  VISIT_FUNCTION(Bbranch);
  VISIT_FUNCTION(Ubranch);
  VISIT_FUNCTION(Goto);
  VISIT_FUNCTION(Push);
  VISIT_FUNCTION(Pop);
  VISIT_FUNCTION(Call);
  VISIT_FUNCTION(Return);
  VISIT_FUNCTION(Newframe);
  VISIT_FUNCTION(Delframe);
#undef VISIT_FUNCTION
};

struct Instr {
  virtual ~Instr() = default;
  virtual std::ostream &print(std::ostream &out) const = 0;
  virtual void accept(Label const &lab, InstrVisitor &vis) const = 0;
};

inline std::ostream &operator<<(std::ostream &out, Instr const &i) {
  return i.print(out);
}

#define MAKE_VISITABLE                                                         \
  void accept(Label const &lab, InstrVisitor &vis) const final {               \
    vis.visit(lab, *this);                                                     \
  }

struct Move : public Instr {
  int64_t source;
  Pseudo dest;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "move " << source << ", " << dest << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Move, int64_t source, Pseudo dest, Label succ)
      : source{source}, dest{dest}, succ{succ} {}
};

struct Copy : public Instr {
  Pseudo src, dest;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "copy " << src << ", " << dest << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Copy, Pseudo src, Pseudo dest, Label succ)
      : src{src}, dest{dest}, succ{succ} {}
};

struct GetMach : public Instr {
  Mach src;
  Pseudo dest;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "copy " << src << ", " << dest << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(GetMach, Mach src, Pseudo dest, Label succ)
      : src{src}, dest{dest}, succ{succ} {}
};

struct SetMach : public Instr {
  Pseudo src;
  Mach dest;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "copy " << src << ", " << dest << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(SetMach, Pseudo src, Mach dest, Label succ)
      : src{src}, dest{dest}, succ{succ} {}
};

struct Load : public Instr {
  std::string src;
  int offset;
  Pseudo dest;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "load " << src << '+' << offset << ", " << dest << "  --> "
               << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Load, std::string const &src, int offset, Pseudo dest, Label succ)
      : src{src}, offset{offset}, dest{dest}, succ{succ} {}
};

struct LoadParam : public Instr {
  int slot;
  Pseudo dest;
  Label succ;
  std::ostream &print(std::ostream &out) const override {
    return out << "load_param " << slot << ", " << dest << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(LoadParam, int slot, Pseudo dest, Label succ)
      : slot{slot}, dest{dest}, succ{succ} {}
};

struct Store : public Instr {
  Pseudo src;
  std::string dest;
  int offset;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "store " << src << ", " << dest << '+' << offset << "  --> "
               << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Store, Pseudo src, std::string const &dest, int offset,
              Label succ)
      : src{src}, dest{dest}, offset{offset}, succ{succ} {}
};

struct Unop : public Instr {
  using Code = rtl::Unop::Code;
  Code opcode;
  Pseudo arg;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "unop " << code_map.at(opcode) << ", " << arg << "  --> "
               << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Unop, Code opcode, Pseudo arg, Label succ)
      : opcode{opcode}, arg{arg}, succ{succ} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Binop : public Instr {
  using Code = rtl::Binop::Code;

  Code opcode;
  Pseudo src, dest;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "binop " << code_map.at(opcode) << ", " << src << ", " << dest
               << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Binop, Code opcode, Pseudo src, Pseudo dest, Label succ)
      : opcode{opcode}, src{src}, dest{dest}, succ{succ} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Ubranch : public Instr {
  using Code = rtl::Ubranch::Code;

  Code opcode;
  Pseudo arg;
  Label succ, fail;

  std::ostream &print(std::ostream &out) const override {
    return out << "ubranch " << code_map.at(opcode) << ", " << arg << "  --> "
               << succ << ", " << fail;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Ubranch, Code opcode, Pseudo arg, Label succ, Label fail)
      : opcode{opcode}, arg{arg}, succ{succ}, fail{fail} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Bbranch : public Instr {
  using Code = rtl::Bbranch::Code;

  Code opcode;
  Pseudo arg1, arg2;
  Label succ, fail;

  std::ostream &print(std::ostream &out) const override {
    return out << "bbranch " << code_map.at(opcode) << ", " << arg1 << ", "
               << arg2 << "  --> " << succ << ", " << fail;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Bbranch, Code opcode, Pseudo arg1, Pseudo arg2, Label succ,
              Label fail)
      : opcode{opcode}, arg1{arg1}, arg2{arg2}, succ{succ}, fail{fail} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Goto : public Instr {
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "goto  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Goto, Label succ) : succ{succ} {}
};

struct Push : public Instr {
  Pseudo arg;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "push " << arg << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Push, Pseudo arg, Label succ) : arg{arg}, succ{succ} {}
};

struct Pop : public Instr {
  Pseudo arg;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "pop " << arg << "  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Pop, Pseudo arg, Label succ) : arg{arg}, succ{succ} {}
};

struct Call : public Instr {
  std::string func;
  uint8_t num_reg;
  Label succ;

  std::ostream &print(std::ostream &out) const override {
    return out << "call " << func << '(' << static_cast<int>(num_reg)
               << ")  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Call, std::string func, uint8_t num_reg, Label succ)
      : func{func}, num_reg{num_reg}, succ{succ} {}
};

struct Return : public Instr {
  std::ostream &print(std::ostream &out) const override {
    return out << "return";
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Return) {}
};

struct Newframe : public Instr {
  Label succ;
  std::ostream &print(std::ostream &out) const override {
    return out << "newframe  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Newframe, Label succ) : succ{succ} {}
};

struct Delframe : public Instr {
  Label succ;
  std::ostream &print(std::ostream &out) const override {
    return out << "delframe  --> " << succ;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Delframe, Label succ) : succ{succ} {}
};
#undef MAKE_VISITABLE

struct Callable {
  std::string name;
  Label enter, leave;
  std::vector<std::pair<Mach, Pseudo>> callee_saves;
  rtl::LabelMap<InstrPtr> body;
  std::vector<Label> schedule; // the order in which the labels are scheduled
  explicit Callable(std::string name) : name{name} {}
  void add_instr(Label lab, InstrPtr instr) {
    if (body.find(lab) != body.end()) {
      std::cerr << "Repeated in-label: " << lab.id << '\n';
      std::cerr << "Trying: " << lab << ": " << *instr << '\n';
      throw std::runtime_error("repeated in-label");
    }
    schedule.push_back(lab);
    body.insert_or_assign(lab, std::move(instr));
  }
};
std::ostream &operator<<(std::ostream &out, Callable const &cbl);

using Program = std::vector<Callable>;

} // namespace ertl

} // namespace bx

#undef CONSTRUCTOR