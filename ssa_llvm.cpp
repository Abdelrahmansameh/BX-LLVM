/**
 * This file transforms RTL to AMD64 assembly
 *
 * Classes:
 *
 *     bx::InstrCompiler:
 *         A visitor that compiles bx::ertl::Instr one by one
 *
 *  Functions
 *
 *     AsmProgram bx::asm_generate(global_vars, prog)
 *         The main compilation function
 */

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

#include "llvm.h"
#include "ssa.h"
#include "ssa_llvm.h"
#include "rtl.h"

int condCounter = 0;

int Counter = 0;

std::map<std::string, std::string> type_table;


namespace bx {

using namespace llvm;

class InstrCompiler : public ssa::InstrVisitor {
private:
  source::Program::GlobalVarTable const &global_vars;

  std::string funcname;

  LlvmProgram body{};

  void append(std::shared_ptr<Llvm> line) { body.push_back(std::move(line)); }

  
public:

  std::vector<rtl::Label> outlabels;

  std::vector<ssa::Pseudo> args;

  std::string type;

  ssa::PseudoMap<std::string> translation;

  std::string translate(ssa::Pseudo const ps){
    if (translation.find(ps) == translation.end()){
      translation[ps] = "x" + std::to_string(Counter);
      Counter++;
      return translation[ps];
    }
    else{
      return translation[ps];
    }
  }

  void append_label(rtl::Label const &rtl_lab) {
    append(Llvm::set_label(rtl_lab.id));
  }

  
  InstrCompiler(source::Program::GlobalVarTable const &global_vars,
                std::string funcname, std::string type)
      : global_vars{global_vars}, funcname{funcname}, type{type} {

  }

  LlvmProgram finalize() {
    LlvmProgram prog;
    std::string sargs = "(";
    for (auto it = args.cbegin(); it != args.cend(); it++) {
      sargs += "i64 %";
      sargs += translate(*it);
      if (it + 1 != args.cend())
        sargs += ", ";
    }
    sargs += ") ";
    prog.push_back(Llvm::directive("define " + type + " @" +funcname + sargs + "{"));
    for (auto i = body.begin(), e = body.end(); i != e; i++)
      prog.push_back(std::move(*i));
    prog.push_back(Llvm::directive("}"));
    return prog;
  }

  void visit(rtl::Label const &, ssa::Move const &mv) override {
    std::string dest = translate(mv.dest);
    append(Llvm::addq(dest, "i64", 0, mv.source));
  }

  void visit(rtl::Label const &, ssa::Copy const &cp) override {
    std::string dest = translate(cp.dest);
    std::string src = translate(cp.src);
    append(Llvm::addq(dest, "i64", src, 0));
  }

  void visit(rtl::Label const &, ssa::Load const &ld) override {
    std::string dest = translate(ld.dest);
    append(Llvm::load(dest, "i64", "i64", ld.src));
  }

  void visit(rtl::Label const &, ssa::Store const &st) override {
    std::string src = translate(st.src);
    append(Llvm::load(st.dest, "i64", "i64", src));
  }

  void visit(rtl::Label const &, ssa::Binop const &bo) override {
    std::string src1 = translate(bo.src1);
    std::string src2 = translate(bo.src2);
    std::string dest = translate(bo.dest);
    switch (bo.opcode) {
    case rtl::Binop::ADD:
      append(Llvm::addq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::SUB:
      append(Llvm::subq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::AND:
      append(Llvm::andq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::OR:
      append(Llvm::orq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::XOR:
      append(Llvm::xorq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::MUL:
      append(Llvm::mulq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::DIV:
      append(Llvm::udivq(dest, "i64", src1, src2));
      break;
    case rtl::Binop::REM:
      break;
    case rtl::Binop::SAL:
      break;
    case rtl::Binop::SAR:
      break;
    }
  }

  void visit(rtl::Label const &, ssa::Unop const &uo) override {
    std::string dest = translate(uo.dest);
    std::string arg = translate(uo.arg);
    switch (uo.opcode) {
    case rtl::Unop::NEG:
      append(Llvm::mulq(dest, "i64", arg, -1));
      break;
    case rtl::Unop::NOT:
      append(Llvm::xorq(dest, "i64", arg, 1));
      break;
    }
  }

  void visit(rtl::Label const &, ssa::Ubranch const &ub) override {
    std::string arg = translate(ub.arg);
    std::string cond = "x" + std::to_string(Counter);
    Counter++;
    condCounter++;
    switch (ub.opcode) {
    case rtl::Ubranch::JZ:
      append(Llvm::eqq(cond, "i64", arg, 1));
      break;
    case rtl::Ubranch::JNZ:
      append(Llvm::neq(cond, "i64", arg, 1));
      break;
    }
    append(Llvm::br_cond(cond, "L" + std::to_string(outlabels[0].id), 
          std::to_string(outlabels[1].id)));
  }

  void visit(rtl::Label const &, ssa::Bbranch const &bb) override {
    std::string arg1 = translate(bb.arg1);
    std::string arg2 = translate(bb.arg2);
    std::string cond = "x" + std::to_string(Counter);
    Counter++;
    switch (bb.opcode) {
    case rtl::Bbranch::JE:
      append(Llvm::eqq(cond, "i64", arg1, arg2));
      break;
    case rtl::Bbranch::JNE:
      append(Llvm::neq(cond, "i64", arg1, arg2));
      break;
    case rtl::Bbranch::JL:
    case rtl::Bbranch::JNGE:
      append(Llvm::sltq(cond, "i64", arg1, arg2));
      break;
    case rtl::Bbranch::JLE:
    case rtl::Bbranch::JNG:
      append(Llvm::sleq(cond, "i64", arg1, arg2));
      break;
    case rtl::Bbranch::JG:
    case rtl::Bbranch::JNLE:
      append(Llvm::sgtq(cond, "i64", arg1, arg2));
      break;
    case rtl::Bbranch::JGE:
    case rtl::Bbranch::JNL:
      append(Llvm::sgeq(cond, "i64", arg1, arg2));
      break;
    }
    append(Llvm::br_cond(cond, "L" + std::to_string(outlabels[0].id), 
      "L" + std::to_string(outlabels[1].id)));
  }

  void visit(rtl::Label const &, ssa::Call const &c) override {
    std::vector<std::vector<std::string> > args;
    for (auto const &parg : c.args){
      std::string arg = translate(parg);
      std::vector<std::string> foo{"i64", arg};
      args.push_back(foo);
    }
    append(Llvm::call(c.func, type_table[c.func], args));
  }

  void visit(rtl::Label const &, ssa::Return const &r) override {
    if (r.arg.id == -1){
      append(Llvm::ret_void());
    }
    else{
      std::string arg = translate(r.arg);
      append(Llvm::ret_type("i64", arg));
    }
  }

  void visit(rtl::Label const &, ssa::Goto const &) override {
    append(Llvm::br_uncond("L" + std::to_string(outlabels[0].id)));
  }

  void visit(rtl::Label const &, ssa::Phi const &phi) override{
    std::vector<std::vector<std::string>> args;
    for (int i = 0; i < phi.args.size(); ++i){
      auto parg = phi.args[i];
      auto pred = phi.preds[i];
      std::string arg = translate(parg);
      std::vector<std::string> foo{arg, "L" + std::to_string(pred.id)};
      args.push_back(foo);
    }
    std::string dest = translate(phi.dest);
    append(Llvm::phi(dest, "i64" , args));
  }
};

LlvmProgram llvm_generate(source::Program::GlobalVarTable const &global_vars,
                        ssa::Program const &prog) {
  LlvmProgram llvm_prog;
  for (auto const &v : global_vars) {
    switch (v.second->ty) {
    case source::Type::BOOL: {
      auto *bc =
          dynamic_cast<source::BoolConstant const *>(v.second->init.get());
          llvm_prog.push_back(Llvm::global_with_value(v.second->name, "i64", bc->value ));
    } break;
    case source::Type::INT64: {
      auto *ic =
          dynamic_cast<source::IntConstant const *>(v.second->init.get());
          llvm_prog.push_back(Llvm::global_with_value(v.second->name, "i64", ic->value ));

    } break;
    default:
      throw std::runtime_error("Invalid global variable");
    }
  }
  type_table["bx_print_int"] = "void";
  for (auto const &cbl : prog) {
    type_table[cbl.name] = cbl.type;
  }
  for (auto const &cbl : prog) {
    InstrCompiler icomp{global_vars, cbl.name, cbl.type};
    icomp.args = cbl.input_regs;
    for (auto const &l : cbl.schedule) {
      icomp.append_label(l);
      icomp.outlabels = cbl.body.at(l)->outlabels;
      for (auto &instr : cbl.body.at(l)->body){
        instr->accept(l, icomp);
      }
    }
    for (auto &i : icomp.finalize())
      llvm_prog.push_back(std::move(i));
  }
  return llvm_prog;
}

} // namespace bx
