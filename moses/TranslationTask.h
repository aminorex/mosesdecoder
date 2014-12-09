#pragma once

#include <boost/smart_ptr/shared_ptr.hpp>
#include "moses/ThreadPool.h"
#include "moses/Manager.h"
#include "moses/HypergraphOutput.h"
#include "moses/IOWrapper.h"
#include "moses/Manager.h"
#include "moses/ChartManager.h"

#include "moses/Syntax/F2S/Manager.h"
#include "moses/Syntax/S2T/Manager.h"
#include "moses/Syntax/T2S/Manager.h"

namespace Moses
{
class InputType;
class OutputCollector;


/** Translates a sentence.
  * - calls the search (Manager)
  * - applies the decision rule
  * - outputs best translation and additional reporting
  **/
class TranslationTask : public Moses::Task
{

public:

  TranslationTask(Moses::InputType* source, Moses::IOWrapper &ioWrapper, int pbOrChart);

  ~TranslationTask();

  /** Translate one sentence
   * gets called by main function implemented at end of this source file */
  void Run();


private:
  int m_pbOrChart; // 1=pb. 2=chart
  Moses::InputType* m_source;
  Moses::IOWrapper &m_ioWrapper;

  void RunPb();
  void RunChart();


  template<typename Parser>
  void DecodeS2T() {
    const std::size_t translationId = m_source->GetTranslationId();
    Syntax::S2T::Manager<Parser> manager(*m_source);
    manager.Decode();
    // 1-best
    const Syntax::SHyperedge *best = manager.GetBestSHyperedge();
    m_ioWrapper.OutputBestHypo(best, translationId);
    // n-best
    manager.OutputNBest(m_ioWrapper.GetNBestOutputCollector());
    // Write 1-best derivation (-translation-details / -T option).
    manager.OutputDetailedTranslationReport(m_ioWrapper.GetDetailedTranslationCollector());
    // Write unknown words file (-output-unknowns option)
    manager.OutputUnknowns(m_ioWrapper.GetUnknownsCollector());
  }

  template<typename RuleMatcher>
  void DecodeT2S() {
    const std::size_t translationId = m_source->GetTranslationId();
    const TreeInput *tree = NULL;
    if (!(tree = dynamic_cast<const TreeInput *>(m_source))) {
      // TODO
    }
    Syntax::T2S::Manager<RuleMatcher> manager(*tree);
    manager.Decode();
    // 1-best
    const Syntax::SHyperedge *best = manager.GetBestSHyperedge();
    m_ioWrapper.OutputBestHypo(best, translationId);
    // n-best
    manager.OutputNBest(m_ioWrapper.GetNBestOutputCollector());
    // Write 1-best derivation (-translation-details / -T option).
    manager.OutputDetailedTranslationReport(
        m_ioWrapper.GetDetailedTranslationCollector());
    // Write unknown words file (-output-unknowns option)
    manager.OutputUnknowns(m_ioWrapper.GetUnknownsCollector());
  }

  template<typename RuleMatcher>
  void DecodeF2S() {
    const std::size_t translationId = m_source->GetTranslationId();
    const TreeInput *tree = NULL;
    if (!(tree = dynamic_cast<const TreeInput *>(m_source))) {
      // TODO
    }
    Syntax::F2S::Manager<RuleMatcher> manager(*tree);
    manager.Decode();
    // 1-best
    const Syntax::SHyperedge *best = manager.GetBestSHyperedge();
    m_ioWrapper.OutputBestHypo(best, translationId);
    // n-best
    manager.OutputNBest(m_ioWrapper.GetNBestOutputCollector());
    // Write 1-best derivation (-translation-details / -T option).
    manager.OutputDetailedTranslationReport(
        m_ioWrapper.GetDetailedTranslationCollector());
    // Write unknown words file (-output-unknowns option)
    manager.OutputUnknowns(m_ioWrapper.GetUnknownsCollector());
  }
};

} //namespace
