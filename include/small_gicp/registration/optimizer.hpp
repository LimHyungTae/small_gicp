#pragma once

#include <small_gicp/util/lie.hpp>
#include <small_gicp/registration/registration_result.hpp>

namespace small_gicp {

struct GaussNewtonOptimizer {
  GaussNewtonOptimizer() : verbose(false), max_iterations(20), lambda(1e-6) {}

  template <
    typename TargetPointCloud,
    typename SourcePointCloud,
    typename TargetTree,
    typename CorrespondenceRejector,
    typename TerminationCriteria,
    typename Reduction,
    typename Factor>
  RegistrationResult optimize(
    const TargetPointCloud& target,
    const SourcePointCloud& source,
    const TargetTree& target_tree,
    const CorrespondenceRejector& rejector,
    const TerminationCriteria& criteria,
    Reduction& reduction,
    const Eigen::Isometry3d& init_T,
    std::vector<Factor>& factors) {
    //
    if (verbose) {
      std::cout << "--- GN optimization ---" << std::endl;
    }

    RegistrationResult result(init_T);
    for (int i = 0; i < max_iterations && !result.converged; i++) {
      const auto [H, b, e] = reduction.linearize(target, source, target_tree, rejector, result.T_target_source, factors);
      const Eigen::Matrix<double, 6, 1> delta = (H + lambda * Eigen ::Matrix<double, 6, 6>::Identity()).ldlt().solve(-b);

      if (verbose) {
        std::cout << "iter=" << i << " e=" << e << " lambda=" << lambda << " dt=" << delta.tail<3>().norm() << " dr=" << delta.head<3>().norm() << std::endl;
      }

      result.converged = criteria.converged(delta);
      result.T_target_source = result.T_target_source * se3_exp(delta);
      result.iterations = i;
      result.H = H;
      result.b = b;
      result.error = e;
    }

    result.num_inliers = std::ranges::count_if(factors, [](const auto& factor) { return factor.inlier(); });

    return result;
  }

  bool verbose;
  int max_iterations;
  double lambda;
};

struct LevenbergMarquardtOptimizer {
  LevenbergMarquardtOptimizer() : verbose(false), max_iterations(20), max_inner_iterations(10), init_lambda(1e-3), lambda_factor(10.0) {}

  template <
    typename TargetPointCloud,
    typename SourcePointCloud,
    typename TargetTree,
    typename CorrespondenceRejector,
    typename TerminationCriteria,
    typename Reduction,
    typename Factor>
  RegistrationResult optimize(
    const TargetPointCloud& target,
    const SourcePointCloud& source,
    const TargetTree& target_tree,
    const CorrespondenceRejector& rejector,
    const TerminationCriteria& criteria,
    Reduction& reduction,
    const Eigen::Isometry3d& init_T,
    std::vector<Factor>& factors) {
    //
    if (verbose) {
      std::cout << "--- LM optimization ---" << std::endl;
    }

    double lambda = init_lambda;
    RegistrationResult result(init_T);
    for (int i = 0; i < max_iterations && !result.converged; i++) {
      const auto [H, b, e] = reduction.linearize(target, source, target_tree, rejector, result.T_target_source, factors);

      bool success = false;
      for (int j = 0; j < max_inner_iterations; j++) {
        const Eigen::Matrix<double, 6, 1> delta = (H + lambda * Eigen ::Matrix<double, 6, 6>::Identity()).ldlt().solve(-b);
        const Eigen::Isometry3d new_T = result.T_target_source * se3_exp(delta);
        const double new_e = reduction.error(target, source, new_T, factors);

        if (verbose) {
          std::cout << "iter=" << i << " inner=" << j << " e=" << e << " new_e=" << new_e << " lambda=" << lambda << " dt=" << delta.tail<3>().norm()
                    << " dr=" << delta.head<3>().norm() << std::endl;
        }

        if (new_e < e) {
          result.converged = criteria.converged(delta);
          result.T_target_source = new_T;
          lambda /= lambda_factor;
          success = true;

          break;
        } else {
          lambda *= lambda_factor;
        }
      }

      result.iterations = i;
      result.H = H;
      result.b = b;
      result.error = e;
    }

    result.num_inliers = std::ranges::count_if(factors, [](const auto& factor) { return factor.inlier(); });

    return result;
  }

  bool verbose;
  int max_iterations;
  int max_inner_iterations;
  double init_lambda;
  double lambda_factor;
};

}  // namespace small_gicp