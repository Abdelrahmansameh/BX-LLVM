#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "ast.h"
#include "rtl.h"
#include "ertl.h"

/** This defines the SSA representation of ERTL */

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

namespace ssa{

using Label = bx::rtl::Label;

struct Pseudo {
  int id;
  int version;
  bool operator==(Pseudo const &other) const noexcept { return id == other.id; }
  bool operator!=(Pseudo const &other) const noexcept { return id != other.id; }
  bool discard(){return id == -1;}
};
std::ostream &operator<<(std::ostream &out, Pseudo const &r);

/*enum class Mach : int8_t {
  // clang-format off
  RAX, RBX, RCX, RDX, RBP, RDI, RSI, RSP,
  R8, R9, R10, R11, R12, R13, R14, R15
  // clang-format on
};
char const *to_string(Mach m);
std::ostream &operator<<(std::ostream &out, Mach m);
*/
struct Instr;
using InstrPtr = std::unique_ptr<Instr const>;

struct BBlock;

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

  std::ostream &print(std::ostream &out) const override {
    return out << "move " << source << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Move, int64_t source, Pseudo dest)
      : source{source}, dest{dest} {}
};

struct Copy : public Instr {
  Pseudo src, dest;

  std::ostream &print(std::ostream &out) const override {
    return out << "copy " << src << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Copy, Pseudo src, Pseudo dest)
      : src{src}, dest{dest} {}
};

struct GetMach : public Instr {
  ertl::Mach src;
  Pseudo dest;

  std::ostream &print(std::ostream &out) const override {
    return out << "copy " << src << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(GetMach, ertl::Mach src, Pseudo dest)
      : src{src}, dest{dest} {}
};

struct SetMach : public Instr {
  Pseudo src;
  ertl::Mach dest;

  std::ostream &print(std::ostream &out) const override {
    return out << "copy " << src << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(SetMach, Pseudo src, ertl::Mach dest)
      : src{src}, dest{dest} {}
};

struct Load : public Instr {
  std::string src;
  int offset;
  Pseudo dest;

  std::ostream &print(std::ostream &out) const override {
    return out << "load " << src << '+' << offset << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Load, std::string const &src, int offset, Pseudo dest)
      : src{src}, offset{offset}, dest{dest} {}
};

struct LoadParam : public Instr {
  int slot;
  Pseudo dest;

  std::ostream &print(std::ostream &out) const override {
    return out << "load_param " << slot << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(LoadParam, int slot, Pseudo dest)
      : slot{slot}, dest{dest}{}
};

struct Store : public Instr {
  Pseudo src;
  std::string dest;
  int offset;

  std::ostream &print(std::ostream &out) const override {
    return out << "store " << src << ", " << dest << '+' << offset;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Store, Pseudo src, std::string const &dest, int offset)
      : src{src}, dest{dest}, offset{offset} {}
};

struct Unop : public Instr {
  using Code = rtl::Unop::Code;
  Code opcode;
  Pseudo arg;

  std::ostream &print(std::ostream &out) const override {
    return out << "unop " << code_map.at(opcode) << ", " << arg;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Unop, Code opcode, Pseudo arg)
      : opcode{opcode}, arg{arg}{}

private:
  static const std::map<Code, char const *> code_map;
};

struct Binop : public Instr {
  using Code = rtl::Binop::Code;

  Code opcode;
  Pseudo src, dest;

  std::ostream &print(std::ostream &out) const override {
    return out << "binop " << code_map.at(opcode) << ", " << src << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Binop, Code opcode, Pseudo src, Pseudo dest)
      : opcode{opcode}, src{src}, dest{dest} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Ubranch : public Instr {
  using Code = rtl::Ubranch::Code;

  Code opcode;
  Pseudo arg;

  std::ostream &print(std::ostream &out) const override {
    return out << "ubranch " << code_map.at(opcode) << ", " << arg;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Ubranch, Code opcode, Pseudo arg)
      : opcode{opcode}, arg{arg} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Bbranch : public Instr {
  using Code = rtl::Bbranch::Code;

  Code opcode;
  Pseudo arg1, arg2;

  std::ostream &print(std::ostream &out) const override {
    return out << "bbranch " << code_map.at(opcode) << ", " << arg1 << ", "
               << arg2;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Bbranch, Code opcode, Pseudo arg1, Pseudo arg2)
      : opcode{opcode}, arg1{arg1}, arg2{arg2} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Goto : public Instr {

  std::ostream &print(std::ostream &out) const override {
    return out << "goto  --> ";
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Goto);
};

struct Push : public Instr {
  Pseudo arg;

  std::ostream &print(std::ostream &out) const override {
    return out << "push " << arg;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Push, Pseudo arg) : arg{arg} {}
};

struct Pop : public Instr {
  Pseudo arg;

  std::ostream &print(std::ostream &out) const override {
    return out << "pop " << arg;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Pop, Pseudo arg) : arg{arg} {}
};

struct Call : public Instr {
  std::string func;
  uint8_t num_reg;

  std::ostream &print(std::ostream &out) const override {
    return out << "call " << func << '(' << static_cast<int>(num_reg)
               << ")";
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Call, std::string func, uint8_t num_reg)
      : func{func}, num_reg{num_reg} {}
};

struct Return : public Instr {
  std::ostream &print(std::ostream &out) const override {
    return out << "return";
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Return);
};

struct Newframe : public Instr {
  std::ostream &print(std::ostream &out) const override {
    return out << "newframe  --> ";
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Newframe);
};

struct Delframe : public Instr {
  std::ostream &print(std::ostream &out) const override {
    return out << "delframe  --> ";
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Delframe);
};
#undef MAKE_VISITABLE

struct BBlock{
    std::vector<Label> outlabels;
    std::vector<InstrPtr> body;
};
std::ostream &operator<<(std::ostream &out, BBlock const &blc);


struct Callable {
  std::string name;
  Label enter, leave;
  std::vector<std::pair<ertl::Mach, Pseudo>> callee_saves;
  rtl::LabelMap<BBlock> body;
  std::vector<Label> schedule; // the order in which the labels are scheduled
  explicit Callable(std::string name) : name{name} {}
  void add_block(Label lab, BBlock block) {
    /*if (body.find(lab) != body.end()) {
      std::cerr << "Repeated in-label: " << lab.id << '\n';
      std::cerr << "Trying: " << lab << ": " << block << '\n';
      throw std::runtime_error("repeated in-label");
    }*/
    //schedule.push_back(lab);
    //body.insert_or_assign(lab, std::move(block));
  }
};
std::ostream &operator<<(std::ostream &out, Callable const &cbl);

using Program = std::vector<Callable>;

} //namespace ssa

} //namespace bx
