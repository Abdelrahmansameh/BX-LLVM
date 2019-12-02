/**
 * This file generates the SSA form of ERTL
 *
 * Classes:
 *
 *     bx::Ssaer:
 *         A visitor that compiles bx::ertl::Instr one by one
 *
 *  Functions
 *
 *     ssa::Program ssa_generate(global_vars, prog)
 *         The main compilation function
 */

#include <stdexcept>
#include <unordered_map>

#include "ssa.h"
#include "ertl.h"
#include "ertl_ssa.h"
#include "rtl.h"

namespace bx {


class Ssaer : public ertl::InstrVisitor {
private:
  source::Program::GlobalVarTable const &global_vars;
  ertl::Callable const &ertl_cbl;
  ssa::Callable ssa_cbl;
  std::vector<rtl::Label> leaders;
  ssa::BBlock cur_block;
public:
  Ssaer(source::Program::GlobalVarTable const &global_vars,
                ertl::Callable const &ertl_cbl,
                std::vector<rtl::Label> leaders)
      : global_vars{global_vars}, ertl_cbl{ertl_cbl},
        ssa_cbl{ertl_cbl.name}, leaders{leaders}{

    for (auto &l : leaders){
        ertl_cbl.body.at(l)->accept(l, *this);
        //ssa_cbl.add_block(l, cur_block);
      }
  }

  void visit(rtl::Label const &, ertl::Newframe const &nf) override {
    cur_block.body.push_back(ssa::Newframe::make());
    auto l = nf.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Delframe const &df) override {
    cur_block.body.push_back(ssa::Delframe::make());
    auto l = df.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Move const &mv) override {
    ssa::Pseudo dest{mv.dest.id,  0};
    cur_block.body.push_back(ssa::Move::make(mv.source, dest));
    auto l = mv.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Copy const &cp) override {
    ssa::Pseudo src{cp.src.id, 0};
    ssa::Pseudo dst{cp.dest.id, 0};
    cur_block.body.push_back(ssa::Copy::make(src, dst));
    auto l = cp.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::SetMach const &sm) override {
    ssa::Pseudo src{sm.src.id, 0};
    cur_block.body.push_back(ssa::SetMach::make(src, sm.dest));
    auto l = sm.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Load const &ld) override {
    ssa::Pseudo dst{ld.dest.id, 0};
    cur_block.body.push_back(ssa::Load::make(ld.src, ld.offset, dst));
    auto l = ld.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Store const &st) override {
    ssa::Pseudo src{st.src.id, 0};
    cur_block.body.push_back(ssa::Store::make(src, st.dest, st.offset)); 
    auto l = st.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::LoadParam const &lp) override {
    ssa::Pseudo dst{lp.dest.id, 0};
    cur_block.body.push_back(ssa::LoadParam::make(lp.slot, dst)); 
    auto l = lp.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Push const &p) override {
    ssa::Pseudo arg{p.arg.id, 0};
    cur_block.body.push_back(ssa::Push::make(arg)); 
    auto l = p.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Pop const &p) override {
    ssa::Pseudo arg{p.arg.id, 0};
    cur_block.body.push_back(ssa::Pop::make(arg)); 
    auto l = p.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Binop const &bo) override {
    ssa::Pseudo src{bo.src.id, 0};
    ssa::Pseudo dest{bo.dest.id, 0};
    cur_block.body.push_back(ssa::Binop::make(bo.opcode, src, dest)); 
    auto l = bo.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Unop const &uo) override {
    ssa::Pseudo arg{uo.arg.id, 0};
    cur_block.body.push_back(ssa::Unop::make(uo.opcode, arg)); 
    auto l = uo.succ;
    ertl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, ertl::Ubranch const &ub) override {
  // Ubranch
  }

  void visit(rtl::Label const &, ertl::Bbranch const &bb) override {

  }

  void visit(rtl::Label const &, ertl::Call const &c) override {

  }

  void visit(rtl::Label const &, ertl::Return const &) override {

  }

  void visit(rtl::Label const &, ertl::Goto const &go) override {

  }
};


} // namespace bx
