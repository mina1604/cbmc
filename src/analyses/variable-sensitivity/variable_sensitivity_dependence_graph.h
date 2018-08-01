/*******************************************************************\

 Module: analyses variable-sensitivity variable-sensitivity-dependence-graph

 Author: Diffblue Ltd.

\*******************************************************************/
#ifndef CPROVER_ANALYSES_VARIABLE_SENSITIVITY_VARIABLE_SENSITIVITY_DEPENDENCE_GRAPH_H
#define CPROVER_ANALYSES_VARIABLE_SENSITIVITY_VARIABLE_SENSITIVITY_DEPENDENCE_GRAPH_H

#include "variable_sensitivity_domain.h"

#include <analyses/cfg_dominators.h>
#include <util/graph.h>

#include <ostream>

class variable_sensitivity_dependence_grapht;

class vs_dep_edget
{
public:
  enum class kindt { NONE, CTRL, DATA, BOTH };

  void add(kindt _kind)
  {
    switch(kind)
    {
      case kindt::NONE:
        kind=_kind;
        break;
      case kindt::DATA:
      case kindt::CTRL:
        if(kind!=_kind)
          kind=kindt::BOTH;
        break;
      case kindt::BOTH:
        break;
    }
  }

  kindt get() const
  {
    return kind;
  }

protected:
  kindt kind;
};

struct vs_dep_nodet:public graph_nodet<vs_dep_edget>
{
  typedef graph_nodet<vs_dep_edget>::edget edget;
  typedef graph_nodet<vs_dep_edget>::edgest edgest;

  goto_programt::const_targett PC;
};

class variable_sensitivity_dependence_domaint:
  public variable_sensitivity_domaint
{
public:
  typedef grapht<vs_dep_nodet>::node_indext node_indext;

  explicit variable_sensitivity_dependence_domaint():
    node_id(std::numeric_limits<node_indext>::max()),
    has_values(false),
    has_changed(false)
  {}

  void transform(
    locationt from,
    locationt to,
    ai_baset &ai,
    const namespacet &ns) override;

  virtual void make_bottom() override
  {
    variable_sensitivity_domaint::make_bottom();
    has_values = tvt(false);
    has_changed = false;
    domain_data_deps.clear();
    control_deps.clear();
    control_dep_candidates.clear();
    control_dep_calls.clear();
  }

  virtual void make_top() override
  {
    variable_sensitivity_domaint::make_top();
    has_values = tvt(true);
    has_changed = false;
    domain_data_deps.clear();
    control_deps.clear();
    control_dep_candidates.clear();
    control_dep_calls.clear();
  }

  virtual void make_entry() override
  {
    make_top();
  }

  bool is_bottom() const override
  {
    return variable_sensitivity_domaint::is_bottom() && has_values.is_false();
  }

  bool is_top() const override
  {
    return variable_sensitivity_domaint::is_top() && has_values.is_true();
  }

  virtual bool merge(
    const variable_sensitivity_domaint &b,
    locationt from,
    locationt to) override;

  virtual void merge_three_way_function_return(
    const ai_domain_baset &function_call,
    const ai_domain_baset &function_start,
    const ai_domain_baset &function_end,
    const namespacet &ns) override;


  void output(
    std::ostream &out,
    const ai_baset &ai,
    const namespacet &ns) const override;

  jsont output_json(
    const ai_baset &ai,
    const namespacet &ns) const override;

  void populate_dep_graph(
    variable_sensitivity_dependence_grapht &,
    goto_programt::const_targett) const;

  void set_node_id(node_indext id)
  {
    node_id=id;
  }

  node_indext get_node_id() const
  {
    assert(node_id!=std::numeric_limits<node_indext>::max());
    return node_id;
  }

private:
  node_indext node_id;
  tvt has_values;
  bool has_changed;

  class dependency_ordert
  {
  public:
    bool operator()(
      const goto_programt::const_targett &a,
      const goto_programt::const_targett &b) const
    {
      return a->location_number < b->location_number;
    }
  };
  typedef std::map<
    goto_programt::const_targett,
    std::set<exprt>, dependency_ordert> data_depst;
  data_depst domain_data_deps;

  typedef std::map<goto_programt::const_targett, tvt> control_depst;
  control_depst control_deps;

  typedef std::set<goto_programt::const_targett> control_dep_candidatest;
  control_dep_candidatest control_dep_candidates;

  typedef std::set<goto_programt::const_targett> control_dep_callst;
  control_dep_callst control_dep_calls;

  void eval_data_deps(
    const exprt &expr, const namespacet &ns, data_depst &deps) const;

  void control_dependencies(
    goto_programt::const_targett from,
    goto_programt::const_targett to,
    variable_sensitivity_dependence_grapht &dep_graph);

  void data_dependencies(
    goto_programt::const_targett from,
    goto_programt::const_targett to,
    variable_sensitivity_dependence_grapht &dep_graph,
    const namespacet &ns);

  bool merge_control_dependencies(
    const control_depst &other_control_deps,
    const control_dep_candidatest &other_control_dep_candidates,
    const control_dep_callst &other_control_dep_calls);
};

class variable_sensitivity_dependence_grapht:
  public ait<variable_sensitivity_dependence_domaint>,
  public grapht<vs_dep_nodet>
{
public:
  using ait<variable_sensitivity_dependence_domaint>::operator[];
  using grapht<vs_dep_nodet>::operator[];

  friend class variable_sensitivity_dependence_domaint;

  typedef std::map<irep_idt, cfg_post_dominatorst> post_dominators_mapt;

  explicit variable_sensitivity_dependence_grapht(
    const goto_functionst &goto_functions,
    const namespacet &_ns,
    const ai_configt &config = {})
    : ait<variable_sensitivity_dependence_domaint>(config),
      goto_functions(goto_functions),
      ns(_ns)
  {
  }

  void initialize(const goto_functionst &goto_functions)
  {
    ait<variable_sensitivity_dependence_domaint>::initialize(goto_functions);
  }

  void initialize(const goto_programt &goto_program)
  {
    ait<variable_sensitivity_dependence_domaint>::initialize(goto_program);
  }

  void finalize()
  {
    for(const auto &location_state : state_map)
    {
      location_state.second.populate_dep_graph(*this, location_state.first);
    }
  }



  void add_dep(
    vs_dep_edget::kindt kind,
    goto_programt::const_targett from,
    goto_programt::const_targett to);

  virtual statet &get_state(goto_programt::const_targett l)
  {
    std::pair<state_mapt::iterator, bool> entry=
      state_map.insert(
        std::make_pair(l, variable_sensitivity_dependence_domaint()));

    if(entry.second)
    {
      const node_indext node_id=add_node();
      entry.first->second.set_node_id(node_id);
      nodes[node_id].PC=l;
    }

    return entry.first->second;
  }

  post_dominators_mapt &cfg_post_dominators()
  {
    return post_dominators;
  }

protected:
  const goto_functionst &goto_functions;
  const namespacet &ns;

  post_dominators_mapt post_dominators;
};

// NOLINTNEXTLINE(whitespace/line_length)
#endif // CPROVER_ANALYSES_VARIABLE_SENSITIVITY_VARIABLE_SENSITIVITY_DEPENDENCE_GRAPH_H