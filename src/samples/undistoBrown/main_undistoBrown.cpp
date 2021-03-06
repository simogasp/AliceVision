// This file is part of the AliceVision project and is made available under
// the terms of the MPL2 license (see the COPYING.md file).

#include "aliceVision/image/all.hpp"
#include "aliceVision/camera/camera.hpp"

#include "dependencies/stlplus3/filesystemSimplified/file_system.hpp"

#include <boost/progress.hpp>
#include <boost/program_options.hpp>

#include <string>
#include <iostream>

using namespace std;
using namespace aliceVision;
using namespace aliceVision::camera;
using namespace aliceVision::image;
namespace po = boost::program_options;

int main(int argc, char **argv)
{
  std::string inputImagePath;
  std::string outputImagePath;
  // Temp storage for the Brown's distortion model
  Vec2 c; // distortion center
  Vec3 k; // distortion factors
  double f; // Focal
  std::string suffix = "jpg";

  po::options_description allParams("AliceVision Sample undistoBrown");
  allParams.add_options()
    ("input,i", po::value<std::string>(&inputImagePath)->required(),
      "An image.")
    ("output,o", po::value<std::string>(&outputImagePath)->required(),
      "An image.")
    ("cx", po::value<double>(&c(0))->required(),
      "Distortion center (x).")
    ("cy", po::value<double>(&c(1))->required(),
      "Distortion center (y).")
    ("k1", po::value<double>(&k(0))->required(),
      "Distortion factors (1).")
    ("k2", po::value<double>(&k(1))->required(),
      "Distortion factors (2).")
    ("k3", po::value<double>(&k(2))->required(),
      "Distortion factors (3).")
    ("focal", po::value<double>(&f)->required(),
      "Focal length.")
    ("suffix", po::value<std::string>(&suffix)->default_value(suffix),
      "Suffix of the input files.");

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, allParams), vm);

    if(vm.count("help") || (argc == 1))
    {
      ALICEVISION_COUT(allParams);
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    ALICEVISION_CERR("ERROR: " << e.what());
    ALICEVISION_COUT("Usage:\n\n" << allParams);
    return EXIT_FAILURE;
  }

  if (outputImagePath == inputImagePath)
  {
    std::cerr << "Input and Ouput path are set to the same value" << std::endl;
    return EXIT_FAILURE;
  }

  if (!stlplus::folder_exists(outputImagePath))
    stlplus::folder_create(outputImagePath);

  std::cout << "Used Brown's distortion model values: \n"
    << "  Distortion center: " << c.transpose() << "\n"
    << "  Distortion coefficients (K1,K2,K3): "
    << k.transpose() << "\n"
    << "  Distortion focal: " << f << std::endl;

  const std::vector<std::string> vec_fileNames =
    stlplus::folder_wildcard(inputImagePath, "*."+suffix, false, true);
  std::cout << "\nLocated " << vec_fileNames.size() << " files in " << inputImagePath
    << " with suffix " << suffix;

  boost::progress_display my_progress_bar( vec_fileNames.size() );
  for (size_t j = 0; j < vec_fileNames.size(); ++j, ++my_progress_bar)
  {
    const string inFileName = stlplus::create_filespec(inputImagePath, stlplus::basename_part(vec_fileNames[j]));
    const string outFileName = stlplus::create_filespec(outputImagePath, stlplus::basename_part(vec_fileNames[j]), "JPG");

    Image<RGBColor> image, imageUd;
    readImage(inFileName, image);

    const PinholeRadialK3 cam(image.Width(), image.Height(), f, c(0), c(1), k(0), k(1), k(2));

    UndistortImage(image, &cam, imageUd, BLACK);
    writeImage(outFileName, imageUd);

  } //end loop for each file
  return EXIT_SUCCESS;
}

