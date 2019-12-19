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

namespace bx {

using namespace llvm;

class InstrCompiler : public ssa::InstrVisitor {
private:
  source::Program::GlobalVarTable const &global_vars;

  std::string funcname, exit_label;

  LlvmProgram body{};

  void append(std::shared_ptr<Llvm> line) { body.push_back(std::move(line)); }

public:

  InstrCompiler(source::Program::GlobalVarTable const &global_vars,
                std::string funcname)
      : global_vars{global_vars}, funcname{funcname} {
    exit_label = ".L" + funcname + ".exit";
  }

  AsmProgram finalize() {
    AsmProgram prog;
    prog.push_back(Asm::directive(".globl " + funcname));
    prog.push_back(Asm::directive(".section .text"));
    prog.push_back(Asm::set_label(funcname));
    if (rmap.size() > 0) {
      prog.push_back(Asm::pushq(Pseudo{reg::rbp}));
      prog.push_back(Asm::movq(Pseudo{reg::rsp}, Pseudo{reg::rbp}));
      prog.push_back(Asm::subq(rmap.size() * 8, Pseudo{reg::rsp}));
    }
    for (auto i = body.begin(), e = body.end(); i != e; i++)
      prog.push_back(std::move(*i));
    prog.push_back(Asm::set_label(exit_label));
    prog.push_back(Asm::ret());
    return prog;
  }

  void visit(rtl::Label const &, ertl::Newframe const &nf) override {
    // do nothing
    append(Asm::jmp(label_translate(nf.succ)));
  }

  void visit(rtl::Label const &, ertl::Delframe const &df) override {
    if (rmap.size() > 0) {
      append(Asm::movq(Pseudo{reg::rbp}, Pseudo{reg::rsp}));
      append(Asm::popq(Pseudo{reg::rbp}));
    }
    append(Asm::jmp(label_translate(df.succ)));
  }

  void visit(rtl::Label const &, ertl::Move const &mv) override {
    int64_t src = mv.source;
    if (src < INT32_MIN || src > INT32_MAX)
      append(Asm::movabsq(src, lookup(mv.dest)));
    else
      append(Asm::movq(src, lookup(mv.dest)));
    append(Asm::jmp(label_translate(mv.succ)));
  }

  void visit(rtl::Label const &, ertl::Copy const &cp) override {
    append(Asm::movq(lookup(cp.src), Pseudo{reg::rax}));
    append(Asm::movq(Pseudo{reg::rax}, lookup(cp.dest)));
    append(Asm::jmp(label_translate(cp.succ)));
  }

  void visit(rtl::Label const &, ertl::SetMach const &sm) override {
    Pseudo dest{ertl::to_string(sm.dest)};
    if (sm.src == rtl::discard_pr)
      append(Asm::xorq(dest, dest));
    else
      append(Asm::movq(lookup(sm.src), dest));
    append(Asm::jmp(label_translate(sm.succ)));
  }

  void visit(rtl::Label const &, ertl::GetMach const &gm) override {
    append(Asm::movq(Pseudo{ertl::to_string(gm.src)}, lookup(gm.dest)));
    append(Asm::jmp(label_translate(gm.succ)));
  }

  void visit(rtl::Label const &, ertl::Load const &ld) override {
    append(Asm::movq_mem2reg(ld.src, Pseudo{reg::r11}));
    append(Asm::movq(Pseudo{reg::r11}, lookup(ld.dest)));
    append(Asm::jmp(label_translate(ld.succ)));
  }

  void visit(rtl::Label const &, ertl::Store const &st) override {
    append(Asm::movq(lookup(st.src), Pseudo{reg::r11}));
    append(Asm::movq_reg2mem(Pseudo{reg::r11}, st.dest));
    append(Asm::jmp(label_translate(st.succ)));
  }

  void visit(rtl::Label const &, ertl::LoadParam const &lp) override {
    append(Asm::movq_addr2reg(16 + lp.slot * 8, Pseudo{reg::rbp},
                              Pseudo{reg::r11}));
    append(Asm::movq(Pseudo{reg::r11}, lookup(lp.dest)));
    append(Asm::jmp(label_translate(lp.succ)));
  }

  void visit(rtl::Label const &, ertl::Push const &p) override {
    append(Asm::pushq(lookup(p.arg)));
    append(Asm::jmp(label_translate(p.succ)));
  }

  void visit(rtl::Label const &, ertl::Pop const &p) override {
    if (p.arg == rtl::discard_pr)
      append(Asm::addq(8UL, Pseudo{reg::rsp}));
    else
      append(Asm::pushq(lookup(p.arg)));
    append(Asm::jmp(label_translate(p.succ)));
  }

  void visit(rtl::Label const &, ertl::Binop const &bo) override {
    auto src = lookup(bo.src);
    auto dest = lookup(bo.dest);
    append(Asm::movq(dest, Pseudo{reg::rax}));
    switch (bo.opcode) {
    case rtl::Binop::ADD:
      append(Asm::addq(src, Pseudo{reg::rax}));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::SUB:
      append(Asm::subq(src, Pseudo{reg::rax}));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::AND:
      append(Asm::andq(src, Pseudo{reg::rax}));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::OR:
      append(Asm::orq(src, Pseudo{reg::rax}));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::XOR:
      append(Asm::xorq(src, Pseudo{reg::rax}));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::MUL:
      append(Asm::imulq(src));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::DIV:
      append(Asm::cqo());
      append(Asm::idivq(src));
      append(Asm::movq(Pseudo{reg::rax}, dest));
      break;
    case rtl::Binop::REM:
      append(Asm::cqo());
      append(Asm::idivq(src));
      append(Asm::movq(Pseudo{reg::rdx}, dest));
      break;
    case rtl::Binop::SAL:
      append(Asm::movq(src, Pseudo{reg::rcx}));
      append(Asm::salq(dest));
      break;
    case rtl::Binop::SAR:
      append(Asm::movq(src, Pseudo{reg::rcx}));
      append(Asm::sarq(dest));
      break;
    }
    append(Asm::jmp(label_translate(bo.succ)));
  }

  void visit(rtl::Label const &, ertl::Unop const &uo) override {
    Pseudo arg = lookup(uo.arg);
    switch (uo.opcode) {
    case rtl::Unop::NEG:
      append(Asm::negq(arg));
      break;
    case rtl::Unop::NOT:
      append(Asm::notq(arg));
      break;
    }
    append(Asm::jmp(label_translate(uo.succ)));
  }

  void visit(rtl::Label const &, ertl::Ubranch const &ub) override {
    Pseudo arg = lookup(ub.arg);
    append(Asm::cmpq(0u, arg));
    switch (ub.opcode) {
    case rtl::Ubranch::JZ:
      append(Asm::je(label_translate(ub.succ)));
      break;
    case rtl::Ubranch::JNZ:
      append(Asm::jne(label_translate(ub.succ)));
      break;
    }
    append(Asm::jmp(label_translate(ub.fail)));
  }

  void visit(rtl::Label const &, ertl::Bbranch const &bb) override {
    Pseudo arg1 = lookup(bb.arg1);
    Pseudo arg2 = lookup(bb.arg2);
    append(Asm::movq(arg1, Pseudo{reg::rcx}));
    append(Asm::movq(arg2, Pseudo{reg::rax}));
    append(Asm::cmpq(Pseudo{reg::rax}, Pseudo{reg::rcx}));
    switch (bb.opcode) {
    case rtl::Bbranch::JE:
      append(Asm::jne(label_translate(bb.fail)));
      break;
    case rtl::Bbranch::JNE:
      append(Asm::je(label_translate(bb.fail)));
      break;
    case rtl::Bbranch::JL:
    case rtl::Bbranch::JNGE:
      append(Asm::jge(label_translate(bb.fail)));
      break;
    case rtl::Bbranch::JLE:
    case rtl::Bbranch::JNG:
      append(Asm::jg(label_translate(bb.fail)));
      break;
    case rtl::Bbranch::JG:
    case rtl::Bbranch::JNLE:
      append(Asm::jle(label_translate(bb.fail)));
      break;
    case rtl::Bbranch::JGE:
    case rtl::Bbranch::JNL:
      append(Asm::jl(label_translate(bb.fail)));
      break;
    }
    append(Asm::jmp(label_translate(bb.succ)));
  }

  void visit(rtl::Label const &, ertl::Call const &c) override {
    append(Asm::call(std::string{c.func}));
    append(Asm::jmp(label_translate(c.succ)));
  }

  void visit(rtl::Label const &, ertl::Return const &) override {
    append(Asm::jmp(exit_label));
  }

  void visit(rtl::Label const &, ertl::Goto const &go) override {
    append(Asm::jmp(label_translate(go.succ)));
  }
};

AsmProgram asm_generate(source::Program::GlobalVarTable const &global_vars,
                        ertl::Program const &prog) {
  AsmProgram asm_prog;
  for (auto const &v : global_vars) {
    asm_prog.push_back(Asm::directive(".globl " + v.first));
    asm_prog.push_back(Asm::directive(".section .data"));
    asm_prog.push_back(Asm::directive(".align 8"));
    asm_prog.push_back(Asm::set_label(v.first));
    switch (v.second->ty) {
    case source::Type::BOOL: {
      auto *bc =
          dynamic_cast<source::BoolConstant const *>(v.second->init.get());
      asm_prog.push_back(Asm::directive(bc->value ? ".quad 0" : ".quad 1"));
    } break;
    case source::Type::INT64: {
      auto *ic =
          dynamic_cast<source::IntConstant const *>(v.second->init.get());
      asm_prog.push_back(Asm::directive(".quad " + std::to_string(ic->value)));
    } break;
    default:
      throw std::runtime_error("Invalid global variable");
    }
  }
  for (auto const &cbl : prog) {
    InstrCompiler icomp{global_vars, cbl.name};
    for (auto const &l : cbl.schedule) {
      icomp.append_label(l);
      cbl.body.at(l)->accept(l, icomp);
    }
    for (auto &i : icomp.finalize())
      asm_prog.push_back(std::move(i));
  }
  return asm_prog;
}

} // namespace bx
