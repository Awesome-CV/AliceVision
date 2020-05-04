// This file is part of the AliceVision project.
// Copyright (c) 2016 AliceVision contributors.
// Copyright (c) 2012 openMVG contributors.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "aliceVision/robustEstimation/LineKernel.hpp"
#include "aliceVision/robustEstimation/LORansac.hpp"
#include "aliceVision/robustEstimation/ScoreEvaluator.hpp"

#include "lineTestGenerator.hpp"

#include "aliceVision/numeric/numeric.hpp"

#include <iostream>
#include <random>
#include <fstream>
#include <vector>
#include <string>

#define BOOST_TEST_MODULE robustEstimationLORansac

#include <boost/test/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>

using namespace aliceVision;
using namespace aliceVision::robustEstimation;
 

struct LineKernelLoRansac : public LineKernel
{
  typedef Vec2 Model; // line parametrization: a, b;

  enum
  {
    MINIMUM_SAMPLES = 2,
    MINIMUM_LSSAMPLES = 2
  };

  LineKernelLoRansac(const Mat2X &xs) : LineKernel(xs)
  {
  }

  void FitLS(const std::vector<size_t> &samples, std::vector<Vec2> *lines, const std::vector<double> *weights = nullptr) const
  {

    assert(samples.size() >= (unsigned int) MINIMUM_SAMPLES);
    // Standard least squares solution.
    const Mat2X sampled_xs = ExtractColumns(xs_, samples);
    if(weights)
      LineSolver::SolveWeightedLS(sampled_xs, lines, *weights);
    else
      LineSolver::Solve(sampled_xs, lines);
  }

  void computeWeights(const Model & model,
                      const std::vector<std::size_t> &inliers,
                      std::vector<double> & vec_weights,
                      const double eps = 0.001) const
  {
    const auto numInliers = inliers.size();
    vec_weights.resize(numInliers);
    for(std::size_t sample = 0; sample < numInliers; ++sample)
    {
      const auto idx = inliers[sample];
      vec_weights[sample] = Error(idx, model);
      // avoid division by zero
      vec_weights[sample] = 1 / std::pow(std::max(eps, vec_weights[sample]), 2);
    }
  }
};

void lineFittingTest(std::size_t numPoints,
                    double outlierRatio,
                    double gaussianNoiseLevel,
                    const Vec2& GTModel,
                    std::mt19937& gen,
                    Vec2& estimatedModel,
                    std::vector<std::size_t>& vec_inliers)
{
  assert(outlierRatio >= 0 && outlierRatio < 1);
  assert(gaussianNoiseLevel >= 0);
  
  Mat2X xy(2, numPoints);
  vector<std::size_t> vec_inliersGT;
  generateLine(numPoints, outlierRatio, gaussianNoiseLevel, GTModel, gen, xy, vec_inliersGT);

  const bool withNoise = (gaussianNoiseLevel > std::numeric_limits<double>::epsilon());
  const std::size_t expectedInliers = numPoints - (std::size_t) numPoints * outlierRatio;
  const double threshold = (withNoise) ? 3 * gaussianNoiseLevel : 0.3;
  LineKernelLoRansac kernel(xy);

  estimatedModel = LO_RANSAC(gen, kernel, ScoreEvaluator<LineKernel>(threshold), &vec_inliers);
  ALICEVISION_LOG_DEBUG("#inliers found : " << vec_inliers.size()
          << " expected: " << numPoints - expectedInliers);
  ALICEVISION_LOG_DEBUG("model[0] found : " << estimatedModel[0]
          << " expected: " << GTModel[0]);
  ALICEVISION_LOG_DEBUG("model[1] found : " << estimatedModel[1]
          << " expected: " << GTModel[1]);

  const std::string base = "testRansac_line_t" + std::to_string(threshold) + "_n" + std::to_string(gaussianNoiseLevel);
  const int W = std::abs(xy(0, 0) - xy(0, numPoints - 1));
  const int H = (int) std::fabs(xy(1, 0) - xy(1, numPoints - 1));
  drawTest(base + "_LORANSACtrial" + std::to_string(0) + ".svg",
           W, H,
           GTModel,
           estimatedModel,
           xy,
           vec_inliers);
}


BOOST_AUTO_TEST_CASE(LoRansacLineFitter_IdealCaseLoRansac)
{

  const std::size_t numPoints = 300;
  const double outlierRatio = .3;
  const double gaussianNoiseLevel = 0.0;
  const std::size_t numTrials = 10;
  Mat2X xy(2, numPoints);
  std::mt19937 gen;

  Vec2 GTModel; // y = 2x + 6.3
  GTModel <<  -2.0, 6.3;
  
  for(std::size_t trial = 0; trial < numTrials; ++trial)
  {
    Vec2 model;
    std::vector<std::size_t> vec_inliers;
    lineFittingTest(numPoints, outlierRatio, gaussianNoiseLevel, GTModel, gen, model, vec_inliers);
    const std::size_t expectedInliers = numPoints - (std::size_t) numPoints * outlierRatio;

    BOOST_CHECK_EQUAL(expectedInliers, vec_inliers.size());
    BOOST_CHECK_SMALL(GTModel[0]-model[0], 1e-2);
    BOOST_CHECK_SMALL(GTModel[1]-model[1], 1e-2);
  }
}


BOOST_AUTO_TEST_CASE(LoRansacLineFitter_RealCaseLoRansac)
{

  const std::size_t numPoints = 300;
  const double outlierRatio = .3;
  const double gaussianNoiseLevel = 0.01;
  const std::size_t numTrials = 10;
  
  Mat2X xy(2, numPoints);

  Vec2 GTModel; // y = 2x + 1
  GTModel << -2, .3;

  std::mt19937 gen;
  
  for(std::size_t trial = 0; trial < numTrials; ++trial)
  {
    Vec2 model;
    std::vector<std::size_t> vec_inliers;
    lineFittingTest(numPoints, outlierRatio, gaussianNoiseLevel, GTModel, gen, model, vec_inliers);
    const std::size_t expectedInliers = numPoints - (std::size_t) numPoints * outlierRatio;
    BOOST_CHECK_EQUAL(expectedInliers, vec_inliers.size());
  }
}
