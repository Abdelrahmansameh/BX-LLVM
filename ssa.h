#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "ast.h"
#include "rtl.h"

/** This defines the SSA representation of RTL */

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

struct PseudoHash {
  std::size_t operator()(Pseudo const &ps) const noexcept {
    return std::hash<int>{}(ps.id) ^ std::hash<int>{}(ps.version);
  }
};
struct PseudoEq {
  constexpr bool operator()(Pseudo const &ps1, Pseudo const &ps2) const noexcept {
    return ps1.id == ps2.id && ps1.version == ps2.version;
  }
};
template <typename V>
using PseudoMap = std::unordered_map<Pseudo, V, PseudoHash, PseudoEq>;

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
using InstrPtr = std::shared_ptr<Instr>;

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
struct Phi;

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
  VISIT_FUNCTION(Phi);
#undef VISIT_FUNCTION
};

struct Instr {
  virtual ~Instr() = default;
  virtual std::ostream &print(std::ostream &out) const = 0;
  virtual void accept(Label const &lab, InstrVisitor &vis) = 0;
  virtual std::vector<Pseudo> getPseudos() const = 0;
  virtual void update_reads(std::unordered_map<int, int> table) = 0;
  virtual void update_all(PseudoMap<int> table) = 0;
  virtual Pseudo getDest() = 0;
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

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{dest};
  }

  void update_reads(std::unordered_map<int, int> table){(void)table;}

  void update_all(PseudoMap<int> table){
    if (table.find(dest) != table.end()){
      dest.version = table[dest];
    }
  }

  Pseudo getDest(){
    return dest;
  }

  std::ostream &print(std::ostream &out) const override {
    return out << "move " << source << ", " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Move, int64_t source, Pseudo dest)
      : source{source}, dest{dest} {}
};

struct Copy : public Instr {
  Pseudo src, dest;

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{dest, src};
  }

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(src.id) != table.end()){
      src.version = table[src.id];
    }
  }

  void update_all(PseudoMap<int> table){
    if (table.find(src) != table.end()){
      src.version = table[src];
    }
    if (table.find(dest) != table.end()){
      dest.version = table[dest];
    }
  }

  Pseudo getDest(){
    return dest;
  }

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

  void update_reads(std::unordered_map<int, int> table){(void)table;}

  void update_all(PseudoMap<int> table){
    if (table.find(dest) != table.end()){
      dest.version = table[dest];
    }
  }

  Pseudo getDest(){
    return dest;
  }

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{dest};
  }

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

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(src.id) != table.end()){
      src.version = table[src.id];
    }
  }

  void update_all(PseudoMap<int> table){
    if (table.find(src) != table.end()){
      src.version = table[src];
    }
  }

  Pseudo getDest(){
    return Pseudo{-1, -1};
  }

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{src};
  }

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
  Pseudo dest;

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(arg.id) != table.end()){
      arg.version = table[arg.id];
    }
  }

  void update_all(PseudoMap<int> table){
    if (table.find(arg) != table.end()){
      arg.version = table[arg];
    }
    if (table.find(dest) != table.end()){
      dest.version = table[dest];
    }
  }


  Pseudo getDest(){
    return dest;
  }

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{arg, dest};
  }

  std::ostream &print(std::ostream &out) const override {
    return out << "unop " << code_map.at(opcode) << ", " << arg << " >> " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Unop, Code opcode, Pseudo arg, Pseudo dest)
      : opcode{opcode}, arg{arg}, dest{dest}{}

private:
  static const std::map<Code, char const *> code_map;
};

struct Binop : public Instr {
  using Code = rtl::Binop::Code;

  Code opcode;
  Pseudo src1, src2, dest;

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(src1.id) != table.end()){
      src1.version = table[src1.id];
    }
    if (table.find(src2.id) != table.end()){
      src2.version = table[src2.id];
    }
  }

  void update_all(PseudoMap<int> table){
    if (table.find(src1) != table.end()){
      src1.version = table[src1];
    }
    if (table.find(src2) != table.end()){
      src2.version = table[src2];
    }
    if (table.find(dest) != table.end()){
      dest.version = table[dest];
    }
  }

  Pseudo getDest(){
    return dest;
  }

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{src1, src2, dest};
  }

  std::ostream &print(std::ostream &out) const override {
    return out << "binop " << code_map.at(opcode) << ", " << src1 << ", " << src2 << " >> " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Binop, Code opcode, Pseudo src1, Pseudo src2, Pseudo dest)
      : opcode{opcode}, src1{src1}, src2{src2} ,dest{dest} {}

private:
  static const std::map<Code, char const *> code_map;
};

struct Ubranch : public Instr {
  using Code = rtl::Ubranch::Code;

  Code opcode;
  Pseudo arg;

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(arg.id) != table.end()){
      arg.version = table[arg.id];
    }
  }

  void update_all(PseudoMap<int> table){
    if (table.find(arg) != table.end()){
      arg.version = table[arg];
    }
  }

  Pseudo getDest(){
    return Pseudo{-1, -1};
  }

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{arg};
  }

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

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{arg1, arg2};
  }

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(arg1.id) != table.end()){
      arg1.version = table[arg1.id];
    }
    if (table.find(arg2.id) != table.end()){
      arg2.version = table[arg2.id];
    }
  }

  void update_all(PseudoMap<int> table){
    if (table.find(arg1) != table.end()){
      arg1.version = table[arg1];
    }
    if (table.find(arg2) != table.end()){
      arg2.version = table[arg2];
    }
  }

  Pseudo getDest(){
    return Pseudo{-1, -1};
  }

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
  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{};
  }

  void update_reads(std::unordered_map<int, int> table){(void)table;}

  void update_all(std::unordered_map<int, int> table){
    (void)table;
  }

  Pseudo getDest(){
    return Pseudo{-1, -1};
  }

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

  std::vector<Pseudo> getPseudos() const override{
    std::vector<Pseudo> pseudos;
    for (auto i : args){
      pseudos.push_back(i);
    }
    pseudos.push_back(ret);
    return pseudos;
  }

  void update_reads(std::unordered_map<int, int> table){
    for (auto &arg : args){
      if (table.find(arg.id) != table.end()){
        arg.version = table[arg.id];
      }
    }
  }

  void update_all(PseudoMap<int> table){
    for (auto &arg : args){
      if (table.find(arg) != table.end()){
        arg.version = table[arg];
      }
    }
    if (table.find(ret) != table.end()){
      ret.version = table[ret];
    }
  }

  Pseudo getDest(){
    return ret;
  }

  std::ostream &print(std::ostream &out) const override {
    out << "call " << func << "(";
    for (auto it = args.cbegin(); it != args.cend(); it++) {
      out << *it;
      if (it + 1 != args.cend())
        out << ", ";
    }
    return out << ") >> " << ret;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Call, std::string func, std::vector<Pseudo> args, Pseudo ret)
      : func{func}, args{args}, ret{ret} {}
};

struct Return : public Instr {
  Pseudo arg;

  std::vector<Pseudo> getPseudos() const override{
    return std::vector<Pseudo>{arg};
  }

  void update_reads(std::unordered_map<int, int> table){
    if (table.find(arg.id) != table.end()){
      arg.version = table[arg.id];
    } 
  }

  void update_all(PseudoMap<int> table){
    if (table.find(arg) != table.end()){
      arg.version = table[arg];
    }
  }

  Pseudo getDest(){
    return Pseudo{-1, -1};
  }

  std::ostream &print(std::ostream &out) const override {
    return out << "return"<< arg;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Return, Pseudo arg) : arg{arg} {}
};

struct Phi : public Instr{
  std::vector<Pseudo> args;
  Pseudo dest;

  std::vector<Pseudo> getPseudos() const override{
    std::vector<Pseudo> pseudos;
    for (auto i : args){
      pseudos.push_back(i);
    }
    pseudos.push_back(dest);
    return pseudos;
  }

  void update_reads(std::unordered_map<int, int> table){(void)table;}

  void update_all(PseudoMap<int> table){
    for (auto &arg : args){
      if (table.find(arg) != table.end()){
        arg.version = table[arg];
      }
    }
    if (table.find(dest) != table.end()){
      dest.version = table[dest];
    }
  }

  Pseudo getDest(){
    return dest;
  }

  std::ostream &print(std::ostream &out) const override {
    out << "phi " << "(";
    for (auto it = args.cbegin(); it != args.cend(); it++) {
      out << *it;
      if (it + 1 != args.cend())
        out << ", ";
    }
    return out << ") >> " << dest;
  }
  MAKE_VISITABLE
  CONSTRUCTOR(Phi, std::vector<Pseudo> args, Pseudo dest)
      : args{args}, dest{dest} {}
};
#undef MAKE_VISITABLE

struct BBlock{
  std::vector<Label> outlabels;
  std::vector<InstrPtr> body;
  std::unordered_map<int, int> recent_versions();
  /*std::vector<Pseudo> getPseudos() const {
    std::vector<Pseudo> pseudos;
    for (auto i : body){
      for (auto ps : i->getPseudos()){
        pseudos.push_back(ps);
      }
    }
    return pseudos;
  } */
  CONSTRUCTOR(BBlock, std::vector<Label> outlabels, std::vector<InstrPtr> body) {
    this->outlabels = outlabels;
    for (auto i : body){
      this->body.push_back(i);
    }
  }
};
std::ostream &operator<<(std::ostream &out, BBlock const &blc);
using BBlockPtr = std::shared_ptr<BBlock>;


struct Callable {
  std::string name;
  Label enter, leave;
  //std::vector<std::pair<ertl::Mach, Pseudo>> callee_saves;
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
  void replace_all(PseudoMap<int> table){
    for (auto &blc : body){
      for (auto &i : blc.second->body){
        i->update_all(table);
      }
    }
  }
};
std::ostream &operator<<(std::ostream &out, Callable const &cbl);

using Program = std::vector<Callable>;

} //namespace ssa

} //namespace bx
