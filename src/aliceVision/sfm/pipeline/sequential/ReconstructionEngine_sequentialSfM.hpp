// This file is part of the AliceVision project.
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include <aliceVision/sfm/pipeline/ReconstructionEngine.hpp>
#include <aliceVision/sfm/LocalBundleAdjustmentData.hpp>
#include <aliceVision/sfm/pipeline/localization/SfMLocalizer.hpp>
#include <aliceVision/sfm/pipeline/pairwiseMatchesIO.hpp>
#include <aliceVision/sfm/sfmDataIO.hpp>
#include <aliceVision/feature/FeaturesPerView.hpp>
#include <aliceVision/track/Track.hpp>

#include <dependencies/htmlDoc/htmlDoc.hpp>
#include <dependencies/histogram/histogram.hpp>

#include <boost/property_tree/ptree.hpp>

namespace pt = boost::property_tree;

namespace aliceVision {
namespace sfm {

/// Image score contains <ImageId, NbPutativeCommonPoint, score, isIntrinsicsReconstructed>
typedef std::tuple<IndexT, std::size_t, std::size_t, bool> ViewConnectionScore;

/**
 * @brief Sequential SfM Pipeline Reconstruction Engine.
 */
class ReconstructionEngine_sequentialSfM : public ReconstructionEngine
{
public:

  ReconstructionEngine_sequentialSfM(const SfMData& sfmData,
                                     const std::string& soutDirectory,
                                     const std::string& loggingFile = "");

  void setFeatures(feature::FeaturesPerView* featuresPerView)
  {
    _featuresPerView = featuresPerView;
  }

  void setMatches(matching::PairwiseMatches* pairwiseMatches)
  {
    _pairwiseMatches = pairwiseMatches;
  }

  void setInitialPair(const Pair& initialPair)
  {
    _userInitialImagePair = initialPair;
  }

  void setMinTrackLength(int minTrackLength)
  {
    _minTrackLength = minTrackLength;
  }

  void setMinInputTrackLength(int minInputTrackLength)
  {
    _minInputTrackLength = minInputTrackLength;
  }

  void setIntermediateFileExtension(const std::string& interFileExtension)
  {
    _sfmdataInterFileExtension = interFileExtension;
  }

  void setNbOfObservationsForTriangulation(std::size_t minNbObservationsForTriangulation)
  {
    _minNbObservationsForTriangulation = minNbObservationsForTriangulation;
  }

  void setLocalBundleAdjustmentGraphDistance(std::size_t distance)
  {
    if(_uselocalBundleAdjustment)
      _localBA_data->setGraphDistanceLimit(distance);
  }

  void setUseLocalBundleAdjustmentStrategy(bool v)
  {
    _uselocalBundleAdjustment = v;
    if(v)
    {
      _localBA_data = std::make_shared<LocalBundleAdjustmentData>(_sfm_data);
      _localBA_data->setOutDirectory(stlplus::folder_append_separator(_sOutDirectory)+"localBA/");

      // delete all the previous data about the Local BA.
      if(stlplus::folder_exists(_localBA_data->getOutDirectory()))
        stlplus::folder_delete(_localBA_data->getOutDirectory(), true);
      stlplus::folder_create(_localBA_data->getOutDirectory());
    }
  }

  void setLocalizerEstimator(robustEstimation::ERobustEstimator estimator)
  {
    _localizerEstimator = estimator;
  }

  /**
   * @brief Process the entire incremental reconstruction
   * @return true if done
   */
  virtual bool Process();

  /**
   * @brief Initialize pyramid scoring
   */
  void initializePyramidScoring();

  /**
   * @brief Initialize tracks
   * @return number of traks
   */
  std::size_t fuseMatchesIntoTracks();

  /**
   * @brief Get all initial pair candidates
   * @return pair list
   */
  std::vector<Pair> getInitialImagePairsCandidates();

  /**
   * @brief Try all initial pair candidates in order to create an initial reconstruction
   * @param initialPairCandidate The list of all initial pair candidates
   */
  void createInitialReconstruction(const std::vector<Pair>& initialImagePairCandidates);

  /**
   * @brief If we have already reconstructed landmarks in a previous reconstruction,
   * we need to recognize the corresponding tracks and update the landmarkIds accordingly.
   */
  void remapLandmarkIdsToTrackIds();

  /**
   * @brief Loop of reconstruction updates
   * @return the duration of the incremental reconstruction
   */
  double incrementalReconstruction();

  /**
   * @brief Update the reconstruction with a new resection group of images
   * @param resectionId The resection id
   * @param bestViewIds The best remaining view ids
   * @param viewIds The remaining view ids
   */
  void updateReconstruction(IndexT resectionId, const std::vector<IndexT>& bestViewIds, std::set<IndexT>& viewIds);

  /**
   * @brief Export and print statistics of a complete reconstruction
   * @param[in] reconstructionTime The duration of the reconstruction
   */
  void exportStatistics(double reconstructionTime);

  /**
   * @brief Return all the images containing matches with already reconstructed 3D points.
   * The images are sorted by a score based on the number of features id shared with
   * the reconstruction and the repartition of these points in the image.
   *
   * @param[out] out_connectedViews: output list of view IDs connected with the 3D reconstruction.
   * @param[in] remainingViewIds: input list of remaining view IDs in which we will search for connected views.
   * @return False if there is no view connected.
   */
  bool findConnectedViews(std::vector<ViewConnectionScore>& out_connectedViews,
                          const std::set<IndexT>& remainingViewIds) const;

  /**
   * @brief Estimate the best images on which we can compute the resectioning safely.
   * The images are sorted by a score based on the number of features id shared with
   * the reconstruction and the repartition of these points in the image.
   *
   * @param[out] out_selectedViewIds: output list of view IDs we can use for resectioning.
   * @param[in] remainingViewIds: input list of remaining view IDs in which we will search for the best ones for resectioning.
   * @return False if there is no possible resection.
   */
  bool findNextBestViews(std::vector<IndexT>& out_selectedViewIds,
                         const std::set<IndexT>& remainingViewIds) const;

private:

  struct ResectionData : ImageLocalizerMatchData
  {
    /// tracks index for resection
    std::set<std::size_t> tracksId;
    /// features index for resection
    std::vector<track::TracksUtilsMap::FeatureId> featuresId;
    /// pose estimated by the resection
    geometry::Pose3 pose;
    /// intrinsic estimated by resection
    std::shared_ptr<camera::IntrinsicBase> optionalIntrinsic = nullptr;
    /// the instrinsic already exists in the scene or not.
    bool isNewIntrinsic;
  };

  /**
   * @brief Compute the initial 3D seed (First camera t=0; R=Id, second estimated by 5 point algorithm)
   * @param[in] initialPair
   * @return
   */
  bool makeInitialPair3D(const Pair& initialPair);

  /**
   * @brief Automatic initial pair selection (based on a 'baseline' computation score)
   * @param[out] out_bestImagePairs
   * @return
   */
  bool getBestInitialImagePairs(std::vector<Pair>& out_bestImagePairs) const;

  /**
   * @brief Compute MSE (Mean Square Error) and a histogram of residual values.
   * @param[in] histogram
   * @return MSE
   */
  double computeResidualsHistogram(Histogram<double>* histogram) const;

  /**
   * @brief Compute MSE (Mean Square Error) and a histogram of tracks size.
   * @param[in] histogram
   * @return MSE
   */
  double computeTracksLengthsHistogram(Histogram<double>* histogram) const;

  /**
   * @brief Compute a score of the view for a subset of features. This is
   *        used for the next best view choice.
   *
   * The score is based on a pyramid which allows to compute a weighting
   * strategy to promote a good repartition in the image (instead of relying
   * only on the number of features).
   * Inspired by [Schonberger 2016]:
   * "Structure-from-Motion Revisited", Johannes L. Schonberger, Jan-Michael Frahm
   * 
   * http://people.inf.ethz.ch/jschoenb/papers/schoenberger2016sfm.pdf
   * We don't use the same weighting strategy. The weighting choice
   * is not justified in the paper.
   *
   * @param[in] viewId: the ID of the view
   * @param[in] trackIds: set of track IDs contained in viewId
   * @return the computed score
   */
  std::size_t computeImageScore(IndexT viewId, const std::vector<std::size_t>& trackIds) const;

  /**
   * @brief Apply the resection on a single view.
   * @param[in] viewIndex: image index to add to the reconstruction.
   * @param[out] resectionData: contains the result (P) and all the data used during the resection.
   * @return false if resection failed
   */
  bool computeResection(const IndexT viewIndex, ResectionData& resectionData);

  /**
   * @brief Update the global scene with the new found camera pose, intrinsic (if not defined) and 
   * Update its observations into the global scene structure.
   * @param[in] viewIndex: image index added to the reconstruction.
   * @param[in] resectionData: contains the camera pose and all data used during the resection.
   */
  void updateScene(const IndexT viewIndex, const ResectionData& resectionData);
                   
  /**
   * @brief  Triangulate new possible 2D tracks
   * List tracks that share content with this view and add observations and new 3D track if required.
   * @param previousReconstructedViews
   * @param newReconstructedViews
   */
  void triangulate(SfMData& scene, const std::set<IndexT>& previousReconstructedViews, const std::set<IndexT>& newReconstructedViews);
  
  /**
   * @brief Triangulate new possible 2D tracks
   * List tracks that share content with this view and run a multiview triangulation on them, using the Lo-RANSAC algorithm.
   * @param[in,out] scene All the data about the 3D reconstruction.
   * @param[in] previousReconstructedViews The list of the old reconstructed views (views index).
   * @param[in] newReconstructedViews The list of the new reconstructed views (views index).
   */
  void triangulateMultiViews_LORANSAC(SfMData& scene, const std::set<IndexT>& previousReconstructedViews, const std::set<IndexT>& newReconstructedViews);
  
  /**
   * @brief Check if a 3D points is well located in front of a set of views.
   * @param[in] pt3D A 3D point (euclidian coordinates)
   * @param[in] viewsId A set of views index
   * @param[in] scene All the data about the 3D reconstruction. 
   * @return false if the 3D points is located behind one view (or more), else \c true.
   */
  bool checkChieralities(const Vec3& pt3D, const std::set<IndexT>& viewsId, const SfMData& scene);
  
  /**
   * @brief Check if the maximal angle formed by a 3D points and 2 views exceeds a min. angle, among a set of views.
   * @param[in] pt3D A 3D point (euclidian coordinates)
   * @param[in] viewsId A set of views index   
   * @param[in] scene All the data about the 3D reconstruction. 
   * @param[in] kMinAngle The angle limit.
   * @return false if the maximal angle does not exceed the limit, else \c true.
   */
  bool checkAngles(const Vec3& pt3D, const std::set<IndexT>& viewsId, const SfMData& scene, const double& kMinAngle);

  /**
   * @brief Bundle adjustment to refine Structure; Motion and Intrinsics
   * @param fixedIntrinsics
   */
  bool BundleAdjustment(bool fixedIntrinsics);
  
  /**
   * @brief Apply the bundle adjustment choosing a small amount of parameters to reduce.
   * It reduces drastically the reconstruction time for big dataset of images.
   * @param The parameters to refine (landmarks, intrinsics, poses) are choosen according to the their
   * @details proximity to the cameras newly added to the reconstruction.
   */
  bool localBundleAdjustment(const std::set<IndexT>& newReconstructedViews);

  /**
   * @brief Select the candidate tracks for the next triangulation step. 
   * @details A track is considered as triangulable if it is visible by at least one new reconsutructed 
   * view and at least \c _minNbObservationsForTriangulation (new and previous) reconstructed view.
   * @param[in] previousReconstructedViews The old reconstructed views.
   * @param[in] newReconstructedViews The newly reconstructed views.
   * @param[out] mapTracksToTriangulate A map with the tracks to triangulate and the observations to do it.
   */
  void getTracksToTriangulate(
      const std::set<IndexT> & previousReconstructedViews, 
      const std::set<IndexT> & newReconstructedViews, 
      std::map<IndexT, std::set<IndexT> > & mapTracksToTriangulate) const;

  /**
   * @brief Remove observation/tracks that have:
   * - too large residual error
   * - too small angular value
   *
   * @param[in] precision
   * @return number of removed outliers
   */
  std::size_t removeOutliers(double precision);

  // Parameters

  Pair _userInitialImagePair;
  int _minInputTrackLength = 2;
  int _minTrackLength = 2;
  int _minPointsPerPose = 30;
  bool _uselocalBundleAdjustment = false;
  /// minimum number of obersvations to triangulate a 3d point.
  std::size_t _minNbObservationsForTriangulation = 2;
  /// a 3D point must have at least 2 obervations not too much aligned.
  double _minAngleForTriangulation = 3.0;
  robustEstimation::ERobustEstimator _localizerEstimator = robustEstimation::ERobustEstimator::ACRANSAC;

  // Data providers

  feature::FeaturesPerView* _featuresPerView;
  matching::PairwiseMatches* _pairwiseMatches;

  // Pyramid scoring

  const int _pyramidBase = 2;
  const int _pyramidDepth = 5;
  /// internal cache of precomputed values for the weighting of the pyramid levels
  std::vector<int> _pyramidWeights;
  int _pyramidThreshold;

  // Temporary data

  /// Putative landmark tracks (visibility per potential 3D point)
  track::TracksMap _map_tracks;
  /// Putative tracks per view
  track::TracksPerView _map_tracksPerView;
  /// Precomputed pyramid index for each trackId of each viewId.
  track::TracksPyramidPerView _map_featsPyramidPerView;
  /// Per camera confidence (A contrario estimated threshold error)
  HashMap<IndexT, double> _map_ACThreshold;

  // Local Bundle Adjustment data

  /// Contains all the data used by the Local BA approach
  std::shared_ptr<LocalBundleAdjustmentData> _localBA_data;

  // Intermediate reconstructions

  /// extension of the intermediate reconstruction files
  std::string _sfmdataInterFileExtension = ".ply";
  /// filter for the intermediate reconstruction files
  ESfMData _sfmdataInterFilter = ESfMData(EXTRINSICS | INTRINSICS | STRUCTURE | OBSERVATIONS | CONTROL_POINTS);

  // Log

  /// HTML logger
  std::shared_ptr<htmlDocument::htmlDocumentStream> _htmlDocStream;
  /// HTML log file
  std::string _htmlLogFile;
  /// property tree for json stats export
  pt::ptree _jsonLogTree;
};

} // namespace sfm
} // namespace aliceVision

