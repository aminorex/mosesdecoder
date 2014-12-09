#pragma once

#include "moses/DecodeGraph.h"
#include "moses/StaticData.h"
#include "moses/Syntax/BoundedPriorityContainer.h"
#include "moses/Syntax/CubeQueue.h"
#include "moses/Syntax/PHyperedge.h"
#include "moses/Syntax/RuleTable.h"
#include "moses/Syntax/RuleTableFF.h"
#include "moses/Syntax/SHyperedgeBundle.h"
#include "moses/Syntax/SVertex.h"
#include "moses/Syntax/SVertexRecombinationOrderer.h"
#include "moses/Syntax/SymbolEqualityPred.h"
#include "moses/Syntax/SymbolHasher.h"

#include "DerivationWriter.h"
#include "GlueRuleSynthesizer.h"
#include "InputTreeBuilder.h"
#include "RuleMatcherCallback.h"
#include "RuleTrie.h"

namespace Moses
{
namespace Syntax
{
namespace T2S
{

template<typename RuleMatcher>
Manager<RuleMatcher>::Manager(const TreeInput &source)
    : BaseManager(source)
    , m_treeSource(source)
{
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::InitializeRuleMatchers()
{
  const std::vector<RuleTableFF*> &ffs = RuleTableFF::Instances();
  for (std::size_t i = 0; i < ffs.size(); ++i) {
    RuleTableFF *ff = ffs[i];
    // This may change in the future, but currently we assume that every
    // RuleTableFF is associated with a static, file-based rule table of
    // some sort and that the table should have been loaded into a RuleTable
    // by this point.
    const RuleTable *table = ff->GetTable();
    assert(table);
    RuleTable *nonConstTable = const_cast<RuleTable*>(table);
    RuleTrie *trie = dynamic_cast<RuleTrie*>(nonConstTable);
    assert(trie);
    boost::shared_ptr<RuleMatcher> p(new RuleMatcher(m_inputTree, *trie));
    m_ruleMatchers.push_back(p);
  }

  // Create an additional rule trie + matcher for glue rules (which are
  // synthesized on demand).
  // FIXME Add a hidden RuleTableFF for the glue rule trie(?)
  m_glueRuleTrie.reset(new RuleTrie(ffs[0]));
  boost::shared_ptr<RuleMatcher> p(new RuleMatcher(m_inputTree, *m_glueRuleTrie));
  m_ruleMatchers.push_back(p);
  m_glueRuleMatcher = p.get();
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::InitializeStacks()
{
  // Check that m_inputTree has been initialized.
  assert(!m_inputTree.nodes.empty());

  for (std::vector<InputTree::Node>::const_iterator p =
       m_inputTree.nodes.begin(); p != m_inputTree.nodes.end(); ++p) {
    const InputTree::Node &node = *p;

    // Create an empty stack.
    SVertexStack &stack = m_stackMap[&(node.pvertex)];

    // For terminals only, add a single SVertex.
    if (node.children.empty()) {
      boost::shared_ptr<SVertex> v(new SVertex());
      v->best = 0;
      v->pvertex = &(node.pvertex);
      stack.push_back(v);
    }
  }
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::Decode()
{
  const StaticData &staticData = StaticData::Instance();

  // Get various pruning-related constants.
  const std::size_t popLimit = staticData.GetCubePruningPopLimit();
  const std::size_t ruleLimit = staticData.GetRuleLimit();
  const std::size_t stackLimit = staticData.GetMaxHypoStackSize();

  // Construct the InputTree.
  InputTreeBuilder builder;
  builder.Build(m_treeSource, "Q", m_inputTree);

  // Initialize the stacks.
  InitializeStacks();

  // Initialize the rule matchers.
  InitializeRuleMatchers();

  // Create a callback to process the PHyperedges produced by the rule matchers.
  RuleMatcherCallback callback(m_stackMap, ruleLimit);

  // Create a glue rule synthesizer.
  GlueRuleSynthesizer glueRuleSynthesizer(*m_glueRuleTrie);

  // Visit each node of the input tree in post-order.
  for (std::vector<InputTree::Node>::const_iterator p =
       m_inputTree.nodes.begin(); p != m_inputTree.nodes.end(); ++p) {

    const InputTree::Node &node = *p;

    // Skip terminal nodes.
    if (node.children.empty()) {
      continue;
    }

    // Call the rule matchers to generate PHyperedges for this node and
    // convert each one to a SHyperedgeBundle (via the callback).  The
    // callback prunes the SHyperedgeBundles and keeps the best ones (up
    // to ruleLimit).
    callback.ClearContainer();
    for (typename std::vector<boost::shared_ptr<RuleMatcher> >::iterator
         q = m_ruleMatchers.begin(); q != m_ruleMatchers.end(); ++q) {
      (*q)->EnumerateHyperedges(node, callback);
    }

    // Retrieve the (pruned) set of SHyperedgeBundles from the callback.
    const BoundedPriorityContainer<SHyperedgeBundle> &bundles =
        callback.GetContainer();

    // Check if any rules were matched.  If not then synthesize a glue rule
    // that is guaranteed to match.
    if (bundles.Size() == 0) {
      glueRuleSynthesizer.SynthesizeRule(node);
      m_glueRuleMatcher->EnumerateHyperedges(node, callback);
      assert(bundles.Size() == 1);
    }

    // Use cube pruning to extract SHyperedges from SHyperedgeBundles and
    // collect the SHyperedges in a buffer.
    CubeQueue cubeQueue(bundles.Begin(), bundles.End());
    std::size_t count = 0;
    std::vector<SHyperedge*> buffer;
    while (count < popLimit && !cubeQueue.IsEmpty()) {
      SHyperedge *hyperedge = cubeQueue.Pop();
      // FIXME See corresponding code in S2T::Manager
      // BEGIN{HACK}
      hyperedge->head->pvertex = &(node.pvertex);
      // END{HACK}
      buffer.push_back(hyperedge);
      ++count;
    }

    // Recombine SVertices and sort into a stack.
    SVertexStack &stack = m_stackMap[&(node.pvertex)];
    RecombineAndSort(buffer, stack);

    // Prune stack.
    if (stackLimit > 0 && stack.size() > stackLimit) {
      stack.resize(stackLimit);
    }
  }
}

template<typename RuleMatcher>
const SHyperedge *Manager<RuleMatcher>::GetBestSHyperedge() const
{
  const InputTree::Node &rootNode = m_inputTree.nodes.back();
  PVertexToStackMap::const_iterator p = m_stackMap.find(&(rootNode.pvertex));
  assert(p != m_stackMap.end());
  const SVertexStack &stack = p->second;
  assert(!stack.empty());
  return stack[0]->best;
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::ExtractKBest(
    std::size_t k,
    std::vector<boost::shared_ptr<KBestExtractor::Derivation> > &kBestList,
    bool onlyDistinct) const
{
  kBestList.clear();
  if (k == 0 || m_source.GetSize() == 0) {
    return;
  }

  // Get the top-level SVertex stack.
  const InputTree::Node &rootNode = m_inputTree.nodes.back();
  PVertexToStackMap::const_iterator p = m_stackMap.find(&(rootNode.pvertex));
  assert(p != m_stackMap.end());
  const SVertexStack &stack = p->second;
  assert(!stack.empty());

  KBestExtractor extractor;

  if (!onlyDistinct) {
    // Return the k-best list as is, including duplicate translations.
    extractor.Extract(stack, k, kBestList);
    return;
  }

  // Determine how many derivations to extract.  If the k-best list is
  // restricted to distinct translations then this limit should be bigger
  // than k.  The k-best factor determines how much bigger the limit should be,
  // with 0 being 'unlimited.'  This actually sets a large-ish limit in case
  // too many translations are identical.
  const StaticData &staticData = StaticData::Instance();
  const std::size_t nBestFactor = staticData.GetNBestFactor();
  std::size_t numDerivations = (nBestFactor == 0) ? k*1000 : k*nBestFactor;

  // Extract the derivations.
  KBestExtractor::KBestVec bigList;
  bigList.reserve(numDerivations);
  extractor.Extract(stack, numDerivations, bigList);

  // Copy derivations into kBestList, skipping ones with repeated translations.
  std::set<Phrase> distinct;
  for (KBestExtractor::KBestVec::const_iterator p = bigList.begin();
       kBestList.size() < k && p != bigList.end(); ++p) {
    boost::shared_ptr<KBestExtractor::Derivation> derivation = *p;
    Phrase translation = KBestExtractor::GetOutputPhrase(*derivation);
    if (distinct.insert(translation).second) {
      kBestList.push_back(derivation);
    }
  }
}

// TODO Move this function into parent directory (Recombiner class?) and
// TODO share with S2T
template<typename RuleMatcher>
void Manager<RuleMatcher>::RecombineAndSort(
    const std::vector<SHyperedge*> &buffer, SVertexStack &stack)
{
  // Step 1: Create a map containing a single instance of each distinct vertex
  // (where distinctness is defined by the state value).  The hyperedges'
  // head pointers are updated to point to the vertex instances in the map and
  // any 'duplicate' vertices are deleted.
// TODO Set?
  typedef std::map<SVertex *, SVertex *, SVertexRecombinationOrderer> Map;
  Map map;
  for (std::vector<SHyperedge*>::const_iterator p = buffer.begin();
       p != buffer.end(); ++p) {
    SHyperedge *h = *p;
    SVertex *v = h->head;
    assert(v->best == h);
    assert(v->recombined.empty());
    std::pair<Map::iterator, bool> result = map.insert(Map::value_type(v, v));
    if (result.second) {
      continue;  // v's recombination value hasn't been seen before.
    }
    // v is a duplicate (according to the recombination rules).
    // Compare the score of h against the score of the best incoming hyperedge
    // for the stored vertex.
    SVertex *storedVertex = result.first->second;
    if (h->score > storedVertex->best->score) {
      // h's score is better.
      storedVertex->recombined.push_back(storedVertex->best);
      storedVertex->best = h;
    } else {
      storedVertex->recombined.push_back(h);
    }
    h->head->best = 0;
    delete h->head;
    h->head = storedVertex;
  }

  // Step 2: Copy the vertices from the map to the stack.
  stack.clear();
  stack.reserve(map.size());
  for (Map::const_iterator p = map.begin(); p != map.end(); ++p) {
    stack.push_back(boost::shared_ptr<SVertex>(p->first));
  }

  // Step 3: Sort the vertices in the stack.
  std::sort(stack.begin(), stack.end(), SVertexStackContentOrderer());
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::OutputNBest(OutputCollector *collector) const
{
  if (collector) {
    const StaticData &staticData = StaticData::Instance();
    long translationId = m_source.GetTranslationId();

    Syntax::KBestExtractor::KBestVec nBestList;
    ExtractKBest(staticData.GetNBestSize(), nBestList,
                 staticData.GetDistinctNBest());
    OutputNBestList(collector, nBestList, translationId);
  }
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::OutputDetailedTranslationReport(
    OutputCollector *collector) const
{
  const SHyperedge *best = GetBestSHyperedge();
  if (best == NULL || collector == NULL) {
    return;
  }
  long translationId = m_source.GetTranslationId();
  std::ostringstream out;
  DerivationWriter::Write(*best, translationId, out);
  collector->Write(translationId, out.str());
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::OutputUnknowns(OutputCollector *collector) const
{
  if (collector) {
    long translationId = m_source.GetTranslationId();
    std::ostringstream out;
    for (std::set<Moses::Word>::const_iterator p = m_oovs.begin();
         p != m_oovs.end(); ++p) {
      out << *p;
    }
    out << std::endl;
    collector->Write(translationId, out.str());
  }
}

template<typename RuleMatcher>
void Manager<RuleMatcher>::OutputNBestList(OutputCollector *collector,
    const Syntax::KBestExtractor::KBestVec &nBestList,
    long translationId) const
{
  const StaticData &staticData = StaticData::Instance();

  const std::vector<Moses::FactorType> &outputFactorOrder = staticData.GetOutputFactorOrder();

  std::ostringstream out;

  if (collector->OutputIsCout()) {
    // Set precision only if we're writing the n-best list to cout.  This is to
    // preserve existing behaviour, but should probably be done either way.
    FixPrecision(out);
  }

  bool includeWordAlignment =
      staticData.PrintAlignmentInfoInNbest();

  bool PrintNBestTrees = StaticData::Instance().PrintNBestTrees();

  for (Syntax::KBestExtractor::KBestVec::const_iterator p = nBestList.begin();
       p != nBestList.end(); ++p) {
    const Syntax::KBestExtractor::Derivation &derivation = **p;

    // get the derivation's target-side yield
    Phrase outputPhrase = Syntax::KBestExtractor::GetOutputPhrase(derivation);

    // delete <s> and </s>
    UTIL_THROW_IF2(outputPhrase.GetSize() < 2,
        "Output phrase should have contained at least 2 words (beginning and end-of-sentence)");
    outputPhrase.RemoveWord(0);
    outputPhrase.RemoveWord(outputPhrase.GetSize() - 1);

    // print the translation ID, surface factors, and scores
    out << translationId << " ||| ";
    OutputSurface(out, outputPhrase, outputFactorOrder, false);
    out << " ||| ";
    OutputAllFeatureScores(derivation.scoreBreakdown, out);
    out << " ||| " << derivation.score;

    // optionally, print word alignments
    if (includeWordAlignment) {
      out << " ||| ";
      Alignments align;
      OutputAlignmentNBest(align, derivation, 0);
      for (Alignments::const_iterator q = align.begin(); q != align.end();
           ++q) {
        out << q->first << "-" << q->second << " ";
      }
    }

    // optionally, print tree
    if (PrintNBestTrees) {
      TreePointer tree = Syntax::KBestExtractor::GetOutputTree(derivation);
      out << " ||| " << tree->GetString();
    }

    out << std::endl;
  }

  assert(collector);
  collector->Write(translationId, out.str());
}

template<typename RuleMatcher>
std::size_t Manager<RuleMatcher>::OutputAlignmentNBest(
    Alignments &retAlign,
    const Syntax::KBestExtractor::Derivation &derivation,
    std::size_t startTarget) const
{
  const Syntax::SHyperedge &shyperedge = derivation.edge->shyperedge;

  std::size_t totalTargetSize = 0;
  std::size_t startSource = shyperedge.head->pvertex->span.GetStartPos();

  const TargetPhrase &tp = *(shyperedge.translation);

  std::size_t thisSourceSize = CalcSourceSize(derivation);

  // position of each terminal word in translation rule, irrespective of alignment
  // if non-term, number is undefined
  std::vector<std::size_t> sourceOffsets(thisSourceSize, 0);
  std::vector<std::size_t> targetOffsets(tp.GetSize(), 0);

  const AlignmentInfo &aiNonTerm = shyperedge.translation->GetAlignNonTerm();
  std::vector<std::size_t> sourceInd2pos = aiNonTerm.GetSourceIndex2PosMap();
  const AlignmentInfo::NonTermIndexMap &targetPos2SourceInd = aiNonTerm.GetNonTermIndexMap();

  UTIL_THROW_IF2(sourceInd2pos.size() != derivation.subderivations.size(),
                 "Error");

  std::size_t targetInd = 0;
  for (std::size_t targetPos = 0; targetPos < tp.GetSize(); ++targetPos) {
    if (tp.GetWord(targetPos).IsNonTerminal()) {
      UTIL_THROW_IF2(targetPos >= targetPos2SourceInd.size(), "Error");
      std::size_t sourceInd = targetPos2SourceInd[targetPos];
      std::size_t sourcePos = sourceInd2pos[sourceInd];

      const Moses::Syntax::KBestExtractor::Derivation &subderivation =
        *derivation.subderivations[sourceInd];

      // calc source size
      std::size_t sourceSize =
          subderivation.edge->head->svertex.pvertex->span.GetNumWordsCovered();
      sourceOffsets[sourcePos] = sourceSize;

      // calc target size.
      // Recursively look thru child hypos
      std::size_t currStartTarget = startTarget + totalTargetSize;
      std::size_t targetSize = OutputAlignmentNBest(retAlign, subderivation,
                                               currStartTarget);
      targetOffsets[targetPos] = targetSize;

      totalTargetSize += targetSize;
      ++targetInd;
    } else {
      ++totalTargetSize;
    }
  }

  // convert position within translation rule to absolute position within
  // source sentence / output sentence
  ShiftOffsets(sourceOffsets, startSource);
  ShiftOffsets(targetOffsets, startTarget);

  // get alignments from this hypo
  const AlignmentInfo &aiTerm = shyperedge.translation->GetAlignTerm();

  // add to output arg, offsetting by source & target
  AlignmentInfo::const_iterator iter;
  for (iter = aiTerm.begin(); iter != aiTerm.end(); ++iter) {
    const std::pair<std::size_t,std::size_t> &align = *iter;
    std::size_t relSource = align.first;
    std::size_t relTarget = align.second;
    std::size_t absSource = sourceOffsets[relSource];
    std::size_t absTarget = targetOffsets[relTarget];

    std::pair<std::size_t, std::size_t> alignPoint(absSource, absTarget);
    std::pair<Alignments::iterator, bool> ret = retAlign.insert(alignPoint);
    UTIL_THROW_IF2(!ret.second, "Error");
  }

  return totalTargetSize;
}

template<typename RuleMatcher>
std::size_t Manager<RuleMatcher>::CalcSourceSize(
    const Syntax::KBestExtractor::Derivation &d) const
{
  const SHyperedge &shyperedge = d.edge->shyperedge;
  std::size_t ret = shyperedge.head->pvertex->span.GetNumWordsCovered();
  for (std::size_t i = 0; i < shyperedge.tail.size(); ++i) {
    std::size_t childSize =
      shyperedge.tail[i]->pvertex->span.GetNumWordsCovered();
    ret -= (childSize - 1);
  }
  return ret;
}

}  // T2S
}  // Syntax
}  // Moses
