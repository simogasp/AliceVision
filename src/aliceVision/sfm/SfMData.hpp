// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <aliceVision/types.hpp>
#include <aliceVision/sfm/View.hpp>
#include <aliceVision/sfm/Rig.hpp>
#include <aliceVision/sfm/Landmark.hpp>
#include <aliceVision/geometry/Pose3.hpp>
#include <aliceVision/camera/camera.hpp>

#include <stdexcept>
#include <cassert>

namespace aliceVision {
namespace sfm {

/// Define a collection of View
using Views = HashMap<IndexT, std::shared_ptr<View> >;

/// Define a collection of Pose (indexed by View::id_pose)
using Poses = HashMap<IndexT, geometry::Pose3>;

/// Define a collection of IntrinsicParameter (indexed by View::id_intrinsic)
using Intrinsics = HashMap<IndexT, std::shared_ptr<camera::IntrinsicBase> >;

/// Define a collection of landmarks are indexed by their TrackId
using Landmarks = HashMap<IndexT, Landmark>;

/// Define a collection of Rig
using Rigs = std::map<IndexT, Rig>;

/// Generic SfM data container
/// Store structure and camera properties:
class SfMData
{
public:
  /// Considered views
  Views views;
  /// Considered camera intrinsics (indexed by view.getIntrinsicId())
  Intrinsics intrinsics;
  /// Structure (3D points with their 2D observations)
  Landmarks structure;
  /// Controls points (stored as Landmarks (id_feat has no meaning here))
  Landmarks control_points;
  /// Feature folder path
  std::vector<std::string> _featuresFolders;
  /// Matching folder path
  std::vector<std::string> _matchesFolders;

  // Operators

  bool operator==(const SfMData& other) const;

  // Accessors

  const Views& GetViews() const {return views;}
  Views& GetViews() {return views;}
  const Poses& GetPoses() const {return _poses;}
  Poses& GetPoses() {return _poses;}
  const Rigs& getRigs() const {return _rigs;}
  Rigs& getRigs() {return _rigs;}
  const Intrinsics& GetIntrinsics() const {return intrinsics;}
  Intrinsics& GetIntrinsics() {return intrinsics;}
  const Landmarks& GetLandmarks() const {return structure;}
  Landmarks& GetLandmarks() {return structure;}
  const Landmarks& GetControl_Points() const {return control_points;}
  Landmarks& GetControl_Points() {return control_points;}
  const std::vector<std::string>& getFeaturesFolders() const {return _featuresFolders;}
  const std::vector<std::string>& getMatchesFolders() const {return _matchesFolders;}

  /**
   * @brief List the view indexes that have valid camera intrinsic and pose.
   * @return view indexes list
   */
  std::set<IndexT> getValidViews() const;

  /**
   * @brief List the intrinsic indexes that have valid camera intrinsic and pose.
   * @return intrinsic indexes list
   */
  std::set<IndexT> getReconstructedIntrinsics() const;

  /**
   * @brief Return a pointer to an intrinsic if available or nullptr otherwise.
   * @param[in] intrinsicId
   */
  const camera::IntrinsicBase * GetIntrinsicPtr(IndexT intrinsicId) const
  {
    if(intrinsics.count(intrinsicId))
      return intrinsics.at(intrinsicId).get();
    return nullptr;
  }

  /**
   * @brief Return a pointer to an intrinsic if available or nullptr otherwise.
   * @param[in] intrinsicId
   */
  camera::IntrinsicBase * GetIntrinsicPtr(IndexT intrinsicId)
  {
    if(intrinsics.count(intrinsicId))
      return intrinsics.at(intrinsicId).get();
    return nullptr;
  }

  /**
   * @brief Return a shared pointer to an intrinsic if available or nullptr otherwise.
   * @param[in] intrinsicId
   */
  std::shared_ptr<camera::IntrinsicBase> GetIntrinsicSharedPtr(IndexT intrinsicId)
  {
    if(intrinsics.count(intrinsicId))
      return intrinsics.at(intrinsicId);
    return nullptr;
  }

  /**
   * @brief Get a set of views keys
   * @return set of views keys
   */
  std::set<IndexT> GetViewsKeys() const
  {
    std::set<IndexT> viewKeys;
    for(auto v: views)
        viewKeys.insert(v.first);
    return viewKeys;
  }

  /**
   * @brief Check if the given view have defined intrinsic and pose
   * @param[in] view The given view
   * @return true if intrinsic and pose defined
   */
  bool IsPoseAndIntrinsicDefined(const View* view) const
  {
    if (view == nullptr)
      return false;
    return (
      view->getIntrinsicId() != UndefinedIndexT &&
      view->getPoseId() != UndefinedIndexT &&
      (!view->isPartOfRig() || getRigSubPose(*view).status != ERigSubPoseStatus::UNINITIALIZED) &&
      intrinsics.find(view->getIntrinsicId()) != intrinsics.end() &&
      _poses.find(view->getPoseId()) != _poses.end());
  }
  
  /**
   * @brief Check if the given view have defined intrinsic and pose
   * @param[in] viewID The given viewID
   * @return true if intrinsic and pose defined
   */
  bool IsPoseAndIntrinsicDefined(IndexT viewId) const
  { 
    return IsPoseAndIntrinsicDefined(views.at(viewId).get());
  }

  /**
   * @brief Check if the given view has an existing pose
   * @param[in] view The given view
   * @return true if the pose exists
   */
  bool existsPose(const View& view) const
  {
     return (_poses.find(view.getPoseId()) != _poses.end());
  }

  /**
   * @brief Gives the pose of the input view. If this view is part of a RIG, it returns rigPose + rigSubPose.
   * @param[in] view The given view
   */
  const geometry::Pose3 getPose(const View& view) const
  {
    // check the view has valid pose / rig etc
    if(!view.isPartOfRig())
    {
      return _poses.at(view.getPoseId());
    }

    // get the pose of the rig and the subpose of the camera
    const geometry::Pose3& rigPose = getRigPose(view);
    const geometry::Pose3& subPose = getRigSubPose(view).pose;

    // multiply rig pose by camera subpose
    return  subPose * rigPose;
  }

  /**
   * @brief Get the rig of the given view
   * @param[in] view The given view
   * @return rig of the given view
   */
  const Rig& getRig(const View& view) const
  {
    assert(view.isPartOfRig());
    return _rigs.at(view.getRigId());
  }

  /**
   * @brief Add the given features Folder
   * @param[in] featuresFolder The given features folder
   */
  void addFeaturesFolder(const std::string& featuresFolder)
  {
    _featuresFolders.emplace_back(featuresFolder);
  }

  /**
   * @brief A the given matches Folder
   * @param[in] matchesFolder The given mathes folder
   */
  void addMatchesFolder(const std::string& matchesFolder)
  {
    _matchesFolders.emplace_back(matchesFolder);
  }

  /**
   * @brief Set the given features folders
   * @param[in] featuresFolders The given features folders
   */
  void setFeaturesFolders(const std::vector<std::string>& featuresFolders)
  {
    _featuresFolders = featuresFolders;
  }

  /**
   * @brief Set the given mathes folders
   * @param[in] matchesFolders The given mathes folders
   */
  void setMatchesFolders(const std::vector<std::string>& matchesFolders)
  {
    _matchesFolders = matchesFolders;
  }

  /**
   * @brief Set the given pose for the given view
   * if the view is part of a rig, this method update rig pose/sub-pose
   * @param[in] view The given view
   * @param[in] pose The given pose
   */
  void setPose(const View& view, const geometry::Pose3& pose);


  /**
   * @brief Set the given pose for the given poseId
   * @param[in] poseId The given poseId
   * @param[in] pose The given pose
   */
  void setAbsolutePose(IndexT poseId, const geometry::Pose3& pose)
  {
    _poses[poseId] = pose;
  }

  /**
   * @brief Erase yhe pose for the given poseId
   * @param[in] poseId The given poseId
   */
  void erasePose(IndexT poseId)
  {
    auto it =_poses.find(poseId);
    if(it != _poses.end())
      _poses.erase(it);
    else
      throw std::out_of_range(std::string("Can't erase unfind pose ") + std::to_string(poseId));
  }

  /**
   * @brief Reset rigs sub-poses parameters
   */
  void resetRigs()
  {
    for(auto rigIt : _rigs)
      rigIt.second.reset();
  }

  /**
   * @brief Insert data from the given sfmData if possible.
   * note: This operation doesn't override existing data.
   * @param[in] sfmData A given SfMData
   */
  void combine(const SfMData& sfmData);

private:

  /// Considered poses (indexed by view.getPoseId())
  Poses _poses;
  /// Considered rigs
  Rigs _rigs;

  /**
   * @brief Get Rig pose of a given camera view
   * @param[in] view The given view
   * @return Rig pose of the given camera view
   */
  const geometry::Pose3& getRigPose(const View& view) const
  {
    return _poses.at(view.getPoseId());
  }

  /**
   * @brief Get Rig subPose of a given camera view
   * @param[in] view The given view
   * @return Rig subPose of the given camera view
   */
  const RigSubPose& getRigSubPose(const View& view) const
  {
    assert(view.isPartOfRig());
    const Rig& rig = _rigs.at(view.getRigId());
    return rig.getSubPose(view.getSubPoseId());
  }

  /**
   * @brief Get Rig pose of a given camera view
   * @param[in] view The given view
   * @return Rig pose of the given camera view
   */
  geometry::Pose3& getRigPose(const View& view)
  {
    return _poses.at(view.getPoseId());
  }

  /**
   * @brief Get Rig subPose of a given camera view
   * @param[in] view The given view
   * @return Rig subPose of the given camera view
   */
  RigSubPose& getRigSubPose(const View& view)
  {
    assert(view.isPartOfRig());
    Rig& rig = _rigs.at(view.getRigId());
    return rig.getSubPose(view.getSubPoseId());
  }
};

/**
 * @brief ColorizeTracks Add the associated color to each 3D point of
 * the sfm_data, using the track to determine the best view from which
 * to get the color.
 * @param sfm_data The container of the data
 * @return true if everything went well
 */
bool ColorizeTracks( SfMData & sfm_data );

} // namespace sfm
} // namespace aliceVision
