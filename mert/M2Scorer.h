#ifndef MERT_M2_SCORER_H_
#define MERT_M2_SCORER_H_

#include <string>
#include <vector>
#include <functional>

#include <boost/python.hpp>

#include "Types.h"
#include "Util.h"
#include "StatisticsBasedScorer.h"

namespace MosesTuning
{

/**
 * M2Scorer class can compute CoNLL m2 F-score.
 */
class M2Scorer: public StatisticsBasedScorer
{
public:
  explicit M2Scorer(const std::string& config);

  virtual void setReferenceFiles(const std::vector<std::string>& referenceFiles);
  virtual void prepareStats(std::size_t sid, const std::string& text, ScoreStats& entry);

  virtual std::size_t NumberOfScores() const {
    //return 9;
    return 3;
  }

  virtual float calculateScore(const std::vector<ScoreStatsType>& comps) const;

private:  
  boost::python::object main_namespace_;
  boost::python::object m2_;
  float beta_; 
  int max_unchanged_words_;
  bool ignore_whitespace_casing_;

  std::map<std::string, std::vector<ScoreStatsType> > seen_;
  
  const char* code();
  
  // no copying allowed
  M2Scorer(const M2Scorer&);
  M2Scorer& operator=(const M2Scorer&);
};

float sentenceM2 (const std::vector<ScoreStatsType>& stats);
float sentenceScaledM2(const std::vector<ScoreStatsType>& stats);
float sentenceBackgroundM2(const std::vector<ScoreStatsType>& stats, const std::vector<ScoreStatsType>& bg);

}

#endif  // MERT_M2_SCORER_H_