#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <small_gicp/util/lie.hpp>
#include <small_gicp/ann/traits.hpp>
#include <small_gicp/points/traits.hpp>

namespace small_gicp {

struct GICPFactor {
  GICPFactor() : target_index(std::numeric_limits<size_t>::max()), source_index(std::numeric_limits<size_t>::max()), mahalanobis(Eigen::Matrix4d::Zero()) {}

  template <typename TargetPointCloud, typename SourcePointCloud, typename TargetTree, typename CorrespondenceRejector>
  bool linearize(
    const TargetPointCloud& target,
    const SourcePointCloud& source,
    const TargetTree& target_tree,
    const Eigen::Isometry3d& T,
    size_t source_index,
    const CorrespondenceRejector& rejector,
    Eigen::Matrix<double, 6, 6>* H,
    Eigen::Matrix<double, 6, 1>* b,
    double* e) {
    //
    this->source_index = source_index;
    this->target_index = std::numeric_limits<size_t>::max();

    const Eigen::Vector4d transed_source_pt = T * traits::point(source, source_index);

    size_t k_index;
    double k_sq_dist;
    if (!traits::knn_search(target_tree, transed_source_pt, 1, &k_index, &k_sq_dist) || rejector(T, k_index, source_index, k_sq_dist)) {
      return false;
    }

    target_index = k_index;

    const Eigen::Matrix4d RCR = traits::cov(target, target_index) + T.matrix() * traits::cov(source, source_index) * T.matrix().transpose();
    mahalanobis.block<3, 3>(0, 0) = RCR.block<3, 3>(0, 0).inverse();

    const Eigen::Vector4d residual = traits::point(target, target_index) - transed_source_pt;

    Eigen::Matrix<double, 4, 6> J = Eigen::Matrix<double, 4, 6>::Zero();
    J.block<3, 3>(0, 0) = T.linear() * skew(traits::point(source, source_index).template head<3>());
    J.block<3, 3>(0, 3) = -T.linear();

    *H = J.transpose() * mahalanobis * J;
    *b = J.transpose() * mahalanobis * residual;
    *e = 0.5 * residual.dot(mahalanobis * residual);

    return true;
  }

  template <typename TargetPointCloud, typename SourcePointCloud>
  double error(const TargetPointCloud& target, const SourcePointCloud& source, const Eigen::Isometry3d& T) const {
    if (target_index == std::numeric_limits<size_t>::max()) {
      return 0.0;
    }

    const Eigen::Vector4d transed_source_pt = T * traits::point(source, source_index);
    const Eigen::Vector4d residual = traits::point(target, target_index) - transed_source_pt;
    return 0.5 * residual.dot(mahalanobis * residual);
  }

  bool inlier() const { return target_index != std::numeric_limits<size_t>::max(); }

  size_t target_index;
  size_t source_index;
  Eigen::Matrix4d mahalanobis;
};
}  // namespace small_gicp