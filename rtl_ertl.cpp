#include <algorithm>
#include <vector>

#include "rtl.h"
#include "rtl_ertl.h"

namespace bx {

class Explicator : public rtl::InstrVisitor {
private:
  source::Program::GlobalVarTable const &global_vars;
  rtl::Callable const &rtl_cbl;
  ertl::Callable ertl_cbl;

public:
  Explicator(source::Program::GlobalVarTable const &global_vars,
             rtl::Callable const &rtl_cbl)
      : global_vars{global_vars}, rtl_cbl{rtl_cbl}, ertl_cbl{rtl_cbl.name} {
    ertl_cbl.enter = rtl::fresh_label();
    auto cur = ertl_cbl.enter;
    // add the newframe instruction
    {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur, ertl::Newframe::make(next));
      cur = next;
    }
    // save the callee-save registers
    for (auto mach_reg : ertl::callee_saves) {
      auto next = rtl::fresh_label();
      auto pseudo = rtl::fresh_pseudo();
      ertl_cbl.add_instr(cur, ertl::GetMach::make(mach_reg, pseudo, next));
      ertl_cbl.callee_saves.push_back({mach_reg, pseudo});
      cur = next;
    }
    // load the input argument registers
    for (auto i = 0u; i < 6u && i < rtl_cbl.input_regs.size(); i++) {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur, ertl::GetMach::make(ertl::input_regs[i],
                                                  rtl_cbl.input_regs[i], next));
      cur = next;
    }
    // load the remaining arguments from the stack slots
    for (auto i = 6u; i < rtl_cbl.input_regs.size(); i++) {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(
          cur, ertl::LoadParam::make(i - 6u, rtl_cbl.input_regs[i], next));
      cur = next;
    }
    // go to the first instruction
    ertl_cbl.add_instr(cur, ertl::Goto::make(rtl_cbl.enter));
    // iterate over the body with the same schedule
    for (auto const &lab : rtl_cbl.schedule)
      rtl_cbl.body.at(lab)->accept(lab, *this);
  }

  ertl::Callable &&deliver() { return std::move(ertl_cbl); }

  void visit(rtl::Label const &lab, rtl::Move const &mv) override {
    ertl_cbl.add_instr(lab, ertl::Move::make(mv.source, mv.dest, mv.succ));
  }

  void visit(rtl::Label const &lab, rtl::Copy const &cp) override {
    ertl_cbl.add_instr(lab, ertl::Copy::make(cp.source, cp.dest, cp.succ));
  }

  void visit(rtl::Label const &lab, rtl::Load const &ld) override {
    ertl_cbl.add_instr(
        lab, ertl::Load::make(ld.source, ld.offset, ld.dest, ld.succ));
  }

  void visit(rtl::Label const &lab, rtl::Store const &st) override {
    ertl_cbl.add_instr(
        lab, ertl::Store::make(st.source, st.dest, st.offset, st.succ));
  }

  void visit(rtl::Label const &lab, rtl::Unop const &uo) override {
    ertl_cbl.add_instr(lab, ertl::Unop::make(uo.opcode, uo.arg, uo.succ));
  }

  void visit(rtl::Label const &lab, rtl::Binop const &bo) override {
    ertl_cbl.add_instr(
        lab, ertl::Binop::make(bo.opcode, bo.source, bo.dest, bo.succ));
  }

  void visit(rtl::Label const &lab, rtl::Ubranch const &ub) override {
    ertl_cbl.add_instr(
        lab, ertl::Ubranch::make(ub.opcode, ub.arg, ub.succ, ub.fail));
  }

  void visit(rtl::Label const &lab, rtl::Bbranch const &bb) override {
    ertl_cbl.add_instr(lab, ertl::Bbranch::make(bb.opcode, bb.arg1, bb.arg2,
                                                bb.succ, bb.fail));
  }

  void visit(rtl::Label const &lab, rtl::Goto const &go) override {
    ertl_cbl.add_instr(lab, ertl::Goto::make(go.succ));
  }

  void visit(rtl::Label const &lab, rtl::Call const &ca) override {
    rtl::Label cur = lab;
    // store the first 6 args to the input regs
    for (auto i = 0u; i < 6u && i < ca.args.size(); i++) {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(
          cur, ertl::SetMach::make(ca.args[i], ertl::input_regs[i], next));
      cur = next;
    }
    // push the last N - 6 args to the stack
    for (auto starg = ca.args.crbegin(); starg < ca.args.crend() - 6; starg++) {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur, ertl::Push::make(*starg, next));
      cur = next;
    }
    // Make the call and save the result
    {
      auto next = rtl::fresh_label();
      uint8_t n = ca.args.size() > 6u ? 6u : ca.args.size();
      ertl_cbl.add_instr(cur, ertl::Call::make(ca.func, n, next));
      cur = next;
    }
    // pop the last N - 6 args from the stack
    for (auto starg = ca.args.crbegin(); starg < ca.args.crend() - 6; starg++) {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur, ertl::Pop::make(rtl::discard_pr, next));
      cur = next;
    }
    // save the result
    if (ca.ret != rtl::discard_pr)
      ertl_cbl.add_instr(cur,
                         ertl::GetMach::make(ertl::Mach::RAX, ca.ret, ca.succ));
    else
      ertl_cbl.add_instr(cur, ertl::Goto::make(ca.succ));
  }

  void visit(rtl::Label const &lab, rtl::Return const &ret) override {
    auto cur = lab;
    // write the return reg
    {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur,
                         ertl::SetMach::make(ret.arg, ertl::Mach::RAX, next));
      cur = next;
    }
    // restore the callee saves
    for (auto mr = ertl_cbl.callee_saves.crbegin();
         mr != ertl_cbl.callee_saves.crend(); mr++) {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur, ertl::SetMach::make(mr->second, mr->first, next));
      cur = next;
    }
    // delete the frame
    {
      auto next = rtl::fresh_label();
      ertl_cbl.add_instr(cur, ertl::Delframe::make(next));
      cur = next;
    }
    // Create the return instruction
    ertl_cbl.add_instr(cur, ertl::Return::make());
    ertl_cbl.leave = cur;
  }
};

ertl::Program make_explicit(source::Program::GlobalVarTable const &global_vars,
                            rtl::Program const &prog) {
  ertl::Program ertl_prog;
  for (auto const &rtl_cbl : prog) {
    Explicator expl{global_vars, rtl_cbl};
    ertl_prog.push_back(expl.deliver());
  }
  return ertl_prog;
}

} // namespace bx