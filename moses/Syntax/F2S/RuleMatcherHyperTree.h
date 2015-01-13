#pragma once

#include "moses/Syntax/PHyperedge.h"

#include "Forest.h"
#include "HyperTree.h"
#include "RuleMatcher.h"

namespace Moses
{
namespace Syntax
{
namespace F2S
{

// Rule matcher based on the algorithm from this paper:
//
//  Hui Zhang, Min Zhang, Haizhou Li, and Chew Lim Tan
//  "Fast Translation Rule Matching for Syntax-based Statistical Machine
//   Translation"
//  In proceedings of EMNLP 2009
//
template<typename Callback>
class RuleMatcherHyperTree : public RuleMatcher<Callback>
{
 public:
  RuleMatcherHyperTree(const HyperTree &);

  ~RuleMatcherHyperTree() {}

  void EnumerateHyperedges(const Forest::Vertex &, Callback &);

 private:
  typedef std::vector<const Forest::Vertex *> FNS;
  typedef std::pair<FNS, const HyperTree::Node *> FP;

  void CartesianProduct(const std::vector<FNS> &, const std::vector<FNS> &,
                        std::vector<FNS> &);

  int CountCommas(const HyperPath::NodeSeq &);

  void Match(const Forest::Vertex &, const HyperTree::Node &, int, Callback &);

  bool MatchChildren(const std::vector<Forest::Vertex *> &,
                     const HyperPath::NodeSeq &, std::size_t, std::size_t);

  void PropagateNextLexel(const FP &);

  int SubSeqLength(const HyperPath::NodeSeq &, int);

  const HyperTree &m_ruleTrie;
  PHyperedge m_hyperedge;
  std::queue<FP> m_queue;  // Called "SFP" in Zhang et al. (2009)
};

}  // namespace F2S
}  // namespace Syntax
}  // namespace Moses

// Implementation
#include "RuleMatcherHyperTree-inl.h"
