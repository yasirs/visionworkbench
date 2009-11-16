// __BEGIN_LICENSE__
// Copyright (C) 2006-2009 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// deplate.cc
///
/// Converts a plate file to GeoTIFF tiles on disk.

#include <vw/Image.h>
#include <vw/FileIO.h>
#include <vw/Plate/PlateView.h>
#include <vw/Plate/KmlPlateManager.h>

using namespace vw;
using namespace vw::platefile;

#include <boost/program_options.hpp>
namespace po = boost::program_options;

// Global variables
std::string output_prefix;
std::string plate_file_name;
int west = 0, east = 0, north = 0, south = 0;
int tile_size;

// Erases a file suffix if one exists and returns the base string
static std::string prefix_from_filename(std::string const& filename) {
  std::string result = filename;
  int index = result.rfind(".");
  if (index != -1) 
    result.erase(index, result.size());
  return result;
}

template <class PixelT>
void do_tiles(cartography::GeoReference output_georef) {
    
  PlateView<PixelT> plate_view(plate_file_name);    
  std::cout << "Converting " << plate_file_name << " to " << output_prefix << "\n";
  std::cout << output_georef << "\n";
  // Get the output georeference.
  vw::BBox2i output_bbox;
  output_bbox.grow(output_georef.lonlat_to_pixel(Vector2(west, north)));
  output_bbox.grow(output_georef.lonlat_to_pixel(Vector2(east, north)));
  output_bbox.grow(output_georef.lonlat_to_pixel(Vector2(west, south)));
  output_bbox.grow(output_georef.lonlat_to_pixel(Vector2(east, south)));
  std::cout << "\t--> Output bbox: " << output_bbox << "\n";
  
  // Compute the bounding box for each tile.
  std::vector<BBox2i> crop_bboxes = image_blocks(crop(plate_view, output_bbox), 
                                                 tile_size, tile_size);
  
  for (int i = 0; i < crop_bboxes.size(); ++i) {
    // The crop bboxes start at (0,0), and we want them to start at
    // the upper left corner of the output_bbox.  
    crop_bboxes[i].min() += output_bbox.min();
    crop_bboxes[i].max() += output_bbox.min();
    
    cartography::GeoReference tile_georef = output_georef;
    Vector2 top_left_ll = output_georef.pixel_to_lonlat(crop_bboxes[i].min());
    Matrix3x3 T = tile_georef.transform();
    T(0,2) = top_left_ll(0);
    T(1,2) = top_left_ll(1);
    tile_georef.set_transform(T);
    
    std::cout << "\t--> Generating tile " << (i+1) << " / " << crop_bboxes.size() 
              << " : " << crop_bboxes[i] << "\n"
              << "\t    with transform  " 
              << tile_georef.transform() << "\n";
    
    std::ostringstream output_filename;
    output_filename << output_prefix << "_" << round(top_left_ll[0]) << "E_" 
                    << round(top_left_ll[1]) << "N.tif";
    
    ImageView<PixelT> cropped_view = crop(plate_view, crop_bboxes[i]);
    if( ! is_transparent(cropped_view) ) {
      DiskImageResourceGDAL rsrc(output_filename.str(), cropped_view.format(), 
                                 Vector2i(256,256));
      write_georeference(rsrc, tile_georef);
      write_image(rsrc, cropped_view, 
                  TerminalProgressCallback(InfoMessage, "\t    Writing: "));
    }
  }
}

int main( int argc, char *argv[] ) {
 
  po::options_description general_options("Turns georeferenced image(s) into a TOAST quadtree.\n\nGeneral Options");
  general_options.add_options()
    ("output-prefix,o", po::value<std::string>(&output_prefix), "Specify the base output directory")
    ("west,w", po::value<int>(&west), "Specify west edge of the region to extract.")
    ("east,e", po::value<int>(&east), "Specify east edge of the region to extract.")
    ("north,n", po::value<int>(&north), "Specify north edge of the region to extract.")
    ("south,s", po::value<int>(&south), "Specify south edge of the region to extract.")
    ("tile-size", po::value<int>(&tile_size)->default_value(4096), "Specify the size of each output tile (in degres).")
    ("help", "Display this help message");

  po::options_description hidden_options("");
  hidden_options.add_options()
    ("plate-file", po::value<std::string>(&plate_file_name));

  po::options_description options("Allowed Options");
  options.add(general_options).add(hidden_options);

  po::positional_options_description p;
  p.add("plate-file", -1);

  std::ostringstream usage;
  usage << "Usage: " << argv[0] << " [options] <filename>..." <<std::endl << std::endl;
  usage << general_options << std::endl;

  po::variables_map vm;
  try { 
    po::store( po::command_line_parser( argc, argv ).options(options).positional(p).run(), vm );
    po::notify( vm );
  } catch (po::error &e) {
    std::cout << "An error occured while parsing command line arguments.\n\n";
    std::cout << usage.str();
    return 0;    
  }

  if( vm.count("help") ) {
    std::cout << usage.str();
    return 0;
  }

  if( vm.count("plate-file") != 1 ) {
    std::cerr << "Error: must specify an input platefile!" << std::endl << std::endl;
    std::cout << usage.str();
    return 1;
  }

  if( output_prefix == "" )
    output_prefix = prefix_from_filename(plate_file_name);

  // Open the plate file
  try {
    boost::shared_ptr<PlateFile> platefile(new PlateFile(plate_file_name));
    KmlPlateManager pm(platefile, 1);

    std::cout << "Opened " << plate_file_name << ".     Depth: " 
              << platefile->depth() << " levels.\n";

    PixelFormatEnum pixel_format = platefile->pixel_format();
    ChannelTypeEnum channel_type = platefile->channel_type();

    switch(pixel_format) {
    case VW_PIXEL_GRAY:
      switch(channel_type) {
      case VW_CHANNEL_UINT8:  
        do_tiles<PixelGray<uint8> >(pm.georeference(platefile->depth()));
        break;
      case VW_CHANNEL_INT16:  
        do_tiles<PixelGray<int16> >(pm.georeference(platefile->depth()));
        break;
      default:
        vw_throw(ArgumentErr() << "Platefile contains a channel type not supported by image2plate.\n");
      }
      break;
    case VW_PIXEL_GRAYA:
      switch(channel_type) {
      case VW_CHANNEL_UINT8:  
        do_tiles<PixelGrayA<uint8> >(pm.georeference(platefile->depth()));
        break;
      default:
        vw_throw(ArgumentErr() << "Platefile contains a channel type not supported by image2plate.\n");
      }
      break;
    case VW_PIXEL_RGB:
    case VW_PIXEL_RGBA:
    default:
      switch(channel_type) {
      case VW_CHANNEL_UINT8:  
        do_tiles<PixelRGBA<uint8> >(pm.georeference(platefile->depth()));
        break;
      default:
        vw_throw(ArgumentErr() << "Platefile contains a channel type not supported by image2plate.\n");
      }
      break;
    }
 


  } catch (vw::Exception &e) {
    std::cout << "An error occurred: " << e.what() << "\nExiting.\n\n";
  }
}