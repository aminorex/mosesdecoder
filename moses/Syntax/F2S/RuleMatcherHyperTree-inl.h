#pragma once

namespace Moses
{
namespace Syntax
{
namespace F2S
{

template<typename Callback>
RuleMatcherHyperTree<Callback>::RuleMatcherHyperTree(const HyperTree &ruleTrie)
    : m_ruleTrie(ruleTrie)
{
}

template<typename Callback>
void RuleMatcherHyperTree<Callback>::EnumerateHyperedges(
    const Forest::Vertex &v, Callback &callback)
{
  const HyperTree::Node &root = m_ruleTrie.GetRootNode();
  HyperPath::NodeSeq nodeSeq(1, v.pvertex.symbol[0]->GetId());
  const HyperTree::Node *child = root.GetChild(nodeSeq);
  if (!child) {
    return;
  }

  m_hyperedge.head = const_cast<PVertex*>(&v.pvertex);

  FNS fns(1, &v);
  FP fp(fns, child);
  m_queue.push(fp);
  while (!m_queue.empty()) {
    FP fp = m_queue.front();
    m_queue.pop();
    if (fp.second->HasRules()) {
      m_hyperedge.tail.clear();
      for (FNS::const_iterator p = fp.first.begin(); p != fp.first.end(); ++p) {
        const Forest::Vertex *v = *p;
        m_hyperedge.tail.push_back(const_cast<PVertex *>(&(v->pvertex)));
      }
      m_hyperedge.translations = &(fp.second->GetTargetPhraseCollection());
      callback(m_hyperedge);
    }
    PropagateNextLexel(fp);
  }
}

template<typename Callback>
void RuleMatcherHyperTree<Callback>::PropagateNextLexel(const FP &fp)
{
  std::vector<FNS> tfns;
  std::vector<FNS> rfns;
  std::vector<FNS> rfns2;

  const HyperTree::Node &trieNode = *(fp.second);
  const HyperTree::Node::Map &map = trieNode.GetMap();

  for (HyperTree::Node::Map::const_iterator p = map.begin();
       p != map.end(); ++p) {
    const HyperPath::NodeSeq &edgeLabel = p->first;
    const HyperTree::Node &child = p->second;

    const int numSubSeqs = CountCommas(edgeLabel) + 1;

    std::size_t pos = 0;
    for (int i = 0; i < numSubSeqs; ++i) {
      tfns.clear();
      if (edgeLabel[pos] == HyperPath::kEpsilon) {
        tfns.push_back(FNS(1, fp.first[i]));
        pos += 2;
      } else {
        const int subSeqLength = SubSeqLength(edgeLabel, pos);
        const std::vector<Forest::Hyperedge*> &incoming = fp.first[i]->incoming;
        for (std::vector<Forest::Hyperedge *>::const_iterator q =
             incoming.begin(); q != incoming.end(); ++q) {
          const Forest::Hyperedge &edge = **q;
          if (MatchChildren(edge.tail, edgeLabel, pos, subSeqLength)) {
            tfns.resize(tfns.size()+1);
            tfns.back().assign(edge.tail.begin(), edge.tail.end());
          }
        }
        pos += subSeqLength + 1;
      }
      if (tfns.empty()) {
        rfns.clear();
        break;
      } else if (i == 0) {
        rfns.swap(tfns);
      } else {
        CartesianProduct(rfns, tfns, rfns2);
        rfns.swap(rfns2);
      }
    }

    for (std::vector<FNS>::const_iterator q = rfns.begin();
         q != rfns.end(); ++q) {
      m_queue.push(FP(*q, &child));
    }
  }
}

template<typename Callback>
void RuleMatcherHyperTree<Callback>::CartesianProduct(const std::vector<FNS> &x,
                                                      const std::vector<FNS> &y,
                                                      std::vector<FNS> &z)
{
  z.clear();
  z.reserve(x.size() * y.size());
  for (std::vector<FNS>::const_iterator p = x.begin(); p != x.end(); ++p) {
    const FNS &a = *p;
    for (std::vector<FNS>::const_iterator q = y.begin(); q != y.end(); ++q) {
      const FNS &b = *q;
      z.resize(z.size()+1);
      FNS &c = z.back();
      c.reserve(a.size() + b.size());
      c.assign(a.begin(), a.end());
      c.insert(c.end(), b.begin(), b.end());
    }
  }
}

template<typename Callback>
bool RuleMatcherHyperTree<Callback>::MatchChildren(
    const std::vector<Forest::Vertex *> &children,
    const HyperPath::NodeSeq &edgeLabel,
    std::size_t pos,
    std::size_t subSeqSize)
{
  if (children.size() != subSeqSize) {
    return false;
  }
  for (int i = 0; i < subSeqSize; ++i) {
    if (edgeLabel[pos+i] != children[i]->pvertex.symbol[0]->GetId()) {
      return false;
    }
  }
  return true;
}

template<typename Callback>
int RuleMatcherHyperTree<Callback>::CountCommas(const HyperPath::NodeSeq &seq)
{
  int count = 0;
  for (std::vector<std::size_t>::const_iterator p = seq.begin();
       p != seq.end(); ++p) {
    if (*p == HyperPath::kComma) {
      ++count;
    }
  }
  return count;
}

template<typename Callback>
int RuleMatcherHyperTree<Callback>::SubSeqLength(const HyperPath::NodeSeq &seq,
                                                 int pos)
{
  int length = 0;
  while (pos != seq.size() && seq[pos] != HyperPath::kComma) {
    ++pos;
    ++length;
  }
  return length;
}

}  // namespace F2S
}  // namespace Syntax
}  // namespace Moses
