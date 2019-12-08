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
  static std::shared_ptr<Cls> make(Args &&... args) {                    \
    return std::shared_ptr<Cls>{new Cls(std::forward<Args>(args)...)};         \
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
using InstrPtr = std::shared_ptr<Instr const>;

struct BBlock;

struct Move;
struct Copy;
struct Load;
struct Store;
struct Binop;
struct Unop;
struct Bbranch;
struct Ubranch;
struct Goto;
struct Call;
struct Return;

struct InstrVisitor {
  virtual ~InstrVisitor() = default;
#define VISIT_FUNCTION(caseclass)                                              \
  virtual void visit(Label const &, caseclass &) = 0
  VISIT_FUNCTION(Move);
  VISIT_FUNCTION(Copy);
  VISIT_FUNCTION(Load);
  VISIT_FUNCTION(Store);
  VISIT_FUNCTION(Binop);
  VISIT_FUNCTION(Unop);
  VISIT_FUNCTION(Bbranch);
  VISIT_FUNCTION(Ubranch);
  VISIT_FUNCTION(Goto);
  VISIT_FUNCTION(Call);
  VISIT_FUNCTION(Return);
#undef VISIT_FUNCTION
};

struct Instr {
  virtual ~Instr() = default;
  virtual std::ostream &print(std::ostream &out) const = 0;
  virtual void accept(Label const &lab, InstrVisitor &vis) = 0;
};

inline std::ostream &operator<<(std::ostream &out, Instr const &i) {
  return i.print(out);
}

#define MAKE_VISITABLE                                                         \
  void accept(Label const &lab, InstrVisitor &vis) final {               \
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


struct Call : public Instr {
  std::string func;
  std::vector<Pseudo> args;
  Pseudo ret;

  std::ostream &print(std::ostream &out) const override {
    out << "call " << func << "(";
    for (auto it = args.cbegin(); it != args.cend(); it++) {
      out << *it;
      if (it + 1 != args.cend())
        out << ", ";
    }
    return out << "), " << ret;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Call, std::string func, std::vector<Pseudo> args, Pseudo ret)
      : func{func}, args{args}, ret{ret} {}
};

struct Return : public Instr {
  Pseudo arg;

  std::ostream &print(std::ostream &out) const override {
    return out << "return"<< arg;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Return, Pseudo arg) : arg{arg} {}
};
#undef MAKE_VISITABLE

struct BBlock{
    std::vector<Label> outlabels;
    std::vector<InstrPtr> body;
    CONSTRUCTOR(BBlock, std::vector<Label> outlabels, std::vector<InstrPtr> body) {
      this->outlabels = outlabels;
      for (auto i : body){
        this->body.push_back(i);
      }
    }
};
std::ostream &operator<<(std::ostream &out, BBlock const &blc);
using BBlockPtr = std::shared_ptr<BBlock const>;


struct Callable {
  std::string name;
  Label enter, leave;
  std::vector<std::pair<ertl::Mach, Pseudo>> callee_saves;
  rtl::LabelMap<BBlockPtr> body;
  std::vector<Label> schedule; // the order in which the labels are scheduled
  explicit Callable(std::string name) : name{name} {}
  void add_block(Label lab, BBlockPtr block) {
    if (body.find(lab) != body.end()) {
      std::cerr << "Repeated in-label: " << lab.id << '\n';
      std::cerr << "Trying: " << lab << ": " << *block << '\n';
      throw std::runtime_error("repeated in-label");
    }
    schedule.push_back(lab);
    body.insert_or_assign(lab, std::move(block));
  }
};
std::ostream &operator<<(std::ostream &out, Callable const &cbl);

using Program = std::vector<Callable>;

} //namespace ssa

} //namespace bx
