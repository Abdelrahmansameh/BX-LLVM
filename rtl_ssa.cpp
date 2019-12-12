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
#include "rtl_ssa.h"
#include "rtl.h"

namespace bx {

class Blocker : public rtl::InstrVisitor {
private:
  source::Program::GlobalVarTable const &global_vars;
  rtl::Callable const &rtl_cbl;
  std::vector<rtl::Label> leaders;
  std::unordered_map<int, int> latest_version;
  std::vector<rtl::Label> outlabels;
  std::vector<ssa::InstrPtr> body;

public:
  ssa::Callable ssa_cbl;
  Blocker(source::Program::GlobalVarTable const &global_vars,
                rtl::Callable const &rtl_cbl,
                std::vector<rtl::Label> leaders,
                std::unordered_map<int, int> latest_version)
        :global_vars{global_vars}, rtl_cbl{rtl_cbl},
         leaders{leaders}, 
         latest_version{latest_version}, 
         ssa_cbl{rtl_cbl.name}{
    for (auto &l : leaders){
        rtl_cbl.body.at(l)->accept(l, *this);
        for (auto ps : this->latest_version){
          std::vector<ssa::Pseudo> args;
          body.insert(body.begin(), ssa::Phi::make(args, ssa::Pseudo{ps.first, ps.second+1}));
          this->latest_version[ps.first] = this->latest_version[ps.first] + 1;
        }
        ssa_cbl.add_block(l, ssa::BBlock::make(outlabels, body));
        body.clear();
        outlabels.clear();
    }
  }

  void visit(rtl::Label const &, rtl::Move const &mv) override {
    ssa::Pseudo dest{mv.dest.id,  latest_version[mv.dest.id]};
    latest_version[mv.dest.id] = latest_version[mv.dest.id] + 1;
    body.push_back(ssa::Move::make(mv.source, dest));
    auto l = mv.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Copy const &cp) override {
    ssa::Pseudo src{cp.source.id, 0};
    ssa::Pseudo dst{cp.dest.id, latest_version[cp.dest.id]};
    latest_version[cp.dest.id] = latest_version[cp.dest.id] + 1;
    body.push_back(ssa::Copy::make(src, dst));
    auto l = cp.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Load const &ld) override {
    ssa::Pseudo dst{ld.dest.id, latest_version[ld.dest.id]};
    latest_version[ld.dest.id] = latest_version[ld.dest.id] + 1;
    body.push_back(ssa::Load::make(ld.source, ld.offset, dst));
    auto l = ld.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Store const &st) override {
    ssa::Pseudo src{st.source.id, 0};
    body.push_back(ssa::Store::make(src, st.dest, st.offset)); 
    auto l = st.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Binop const &bo) override {
    ssa::Pseudo src{bo.source.id, 0};
    ssa::Pseudo dest{bo.dest.id, latest_version[bo.dest.id]};
    latest_version[bo.dest.id] = latest_version[bo.dest.id] + 1;
    body.push_back(ssa::Binop::make(bo.opcode, src, dest)); 
    auto l = bo.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Unop const &uo) override {
    ssa::Pseudo arg{uo.arg.id, latest_version[uo.arg.id]};
    latest_version[uo.arg.id] = latest_version[uo.arg.id] + 1;
    body.push_back(ssa::Unop::make(uo.opcode, arg)); 
    auto l = uo.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Ubranch const &ub) override {
    ssa::Pseudo arg{ub.arg.id, 0};
    body.push_back(ssa::Ubranch::make(ub.opcode, arg));
    outlabels.push_back(ub.succ);
    outlabels.push_back(ub.fail);
  }

  void visit(rtl::Label const &, rtl::Bbranch const &bb) override {
    ssa::Pseudo arg1{bb.arg1.id, 0};
    ssa::Pseudo arg2{bb.arg2.id, 0};
    body.push_back(ssa::Bbranch::make(bb.opcode, arg1, arg2));
    outlabels.push_back(bb.succ);
    outlabels.push_back(bb.fail);
  }

  void visit(rtl::Label const &, rtl::Call const &c) override {
    std::vector<ssa::Pseudo> args;
    for (auto &a : c.args){
      ssa::Pseudo sarg{a.id, 0};
      args.push_back(sarg);
    }
    ssa::Pseudo sret{c.ret.id, latest_version[c.ret.id]};
    latest_version[c.ret.id] = latest_version[c.ret.id] + 1;
    body.push_back(ssa::Call::make(c.func, args ,sret));
    auto l = c.succ;
    rtl_cbl.body.at(l)->accept(l, *this);
  }

  void visit(rtl::Label const &, rtl::Return const &r) override {
    ssa::Pseudo arg{r.arg.id, 0};
    body.push_back(ssa::Return::make(arg));
  }

  void visit(rtl::Label const &, rtl::Goto const &go) override {
    outlabels.push_back(go.succ);
  }
  
};

ssa::Program blocks_generate(source::Program::GlobalVarTable const &global_vars,
                        rtl::Program &prog) {
  ssa::Program ret;
  for (auto &cbl : prog) {
    std::vector<rtl::Label> leaders;
    leaders.push_back(cbl.enter);
    std::cout << cbl.name << std::endl;
    std::unordered_map<int, int> latest_version;
    for (auto &l : cbl.schedule) {
      auto i = cbl.body.at(l);
      for (auto ps : i->getPseudos()){
        latest_version.insert_or_assign(ps.id, 0);
      }
      if (auto bb = std::dynamic_pointer_cast<const rtl::Bbranch>(i)){
        if (std::find(leaders.begin(), leaders.end(), bb->succ) == leaders.end()) {
          leaders.push_back(bb->succ);
        }
        if (std::find(leaders.begin(), leaders.end(), bb->fail) == leaders.end()) {
          leaders.push_back(bb->fail);
        }
      }
      if (auto ub = std::dynamic_pointer_cast<const rtl::Ubranch>(i)){
        if (std::find(leaders.begin(), leaders.end(), ub->succ) == leaders.end()) {
          leaders.push_back(ub->succ);
        }
        if (std::find(leaders.begin(), leaders.end(), ub->fail) == leaders.end()) {
          leaders.push_back(ub->fail);
        }
      }
      if (auto gt = std::dynamic_pointer_cast<const rtl::Goto>(i)){
        if (std::find(leaders.begin(), leaders.end(), gt->succ) == leaders.end()) {
          leaders.push_back(gt->succ);
        }
      }
    }
    for (auto &l : leaders){
      std::cout << l << std::endl;
    }
    Blocker blocker{global_vars, cbl, leaders, latest_version};
    ret.push_back(blocker.ssa_cbl);
  } 
  return ret;
}
} // namespace bx
