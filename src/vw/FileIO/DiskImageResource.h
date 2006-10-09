// __BEGIN_LICENSE__
// 
// Copyright (C) 2006 United States Government as represented by the
// Administrator of the National Aeronautics and Space Administration
// (NASA).  All Rights Reserved.
// 
// Copyright 2006 Carnegie Mellon University. All rights reserved.
// 
// This software is distributed under the NASA Open Source Agreement
// (NOSA), version 1.3.  The NOSA has been approved by the Open Source
// Initiative.  See the file COPYING at the top of the distribution
// directory tree for the complete NOSA document.
// 
// THE SUBJECT SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY OF ANY
// KIND, EITHER EXPRESSED, IMPLIED, OR STATUTORY, INCLUDING, BUT NOT
// LIMITED TO, ANY WARRANTY THAT THE SUBJECT SOFTWARE WILL CONFORM TO
// SPECIFICATIONS, ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR
// A PARTICULAR PURPOSE, OR FREEDOM FROM INFRINGEMENT, ANY WARRANTY THAT
// THE SUBJECT SOFTWARE WILL BE ERROR FREE, OR ANY WARRANTY THAT
// DOCUMENTATION, IF PROVIDED, WILL CONFORM TO THE SUBJECT SOFTWARE.
// 
// __END_LICENSE__

/// \file FileIO/DiskImageResource.h
/// 
/// An abstract base class referring to an image on disk.
/// 
#ifndef __VW_FILEIO_DISK_IMAGE_RESOURCE_H__
#define __VW_FILEIO_DISK_IMAGE_RESOURCE_H__

#include <string>
#include <boost/type_traits.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#include <vw/Core/Debugging.h>
#include <vw/Math/BBox.h>
#include <vw/Math/Vector.h>
#include <vw/Image/ImageView.h>
#include <vw/Image/PixelTypes.h>
#include <vw/Image/GenericImageBuffer.h>

namespace vw {

  // *******************************************************************
  // The DiskImageResource abstract base class
  // *******************************************************************

  /// Base class from which specific file handlers derive.
  class DiskImageResource { 
  public:

    virtual ~DiskImageResource() {};

    /// Returns the number of columns in an image on disk.
    int cols() const { return m_format.cols; }

    /// Returns the number of rows in an image on disk.
    int rows() const { return m_format.rows; }

    /// Returns the number of planes in an image on disk.
    int planes() const { return m_format.planes; }

    /// Returns the number of channels in a image on disk
    int channels() const { return num_channels(m_format.pixel_format); }

    /// Returns the pixel format of an image on disk.
    PixelFormatEnum pixel_format() const { return m_format.pixel_format; }

    /// Returns the channel type of an image on disk.
    ChannelTypeEnum channel_type() const { return m_format.channel_type; }

    /// Read the image on disk into the given buffer.
    virtual void read( GenericImageBuffer const& buf ) const = 0;

    /// Read a block of the image on disk into the given buffer.
    virtual void read( GenericImageBuffer const& buf, BBox2i bbox ) const {
      if( bbox==BBox2i(0,0,cols(),rows()) ) return read( buf );
      throw NoImplErr() << "This DiskImageResource does not support partial reads!";
    }

    /// Returns the optimal block size/alignment for partial reads.
    virtual Vector2i native_read_block_size() const { return Vector2i(cols(),rows()); }

    /// Write the given buffer to the image on disk.
    virtual void write( GenericImageBuffer const& buf ) = 0;

    /// Force any changes to disk.
    virtual void flush() = 0;

    /// Read the image on disk into the given image view.
    template <class PixelT>
    void read( ImageView<PixelT> const& buf ) const {
      read( GenericImageBuffer(buf) );
    }

    /// Read a block of the image on disk into the given image view.
    template <class PixelT>
    void read( ImageView<PixelT> const& buf, BBox2i bbox ) const {
      read( GenericImageBuffer(buf), bbox );
    }

    /// Read the image on disk into the given image view, resizing the
    /// view if needed.
    template <class PixelT>
    void read( ImageView<PixelT>& buf ) const {

      int im_planes = 1;
      if( ! IsCompound<PixelT>::value ) {
        // The image has a fundamental pixel type
        if( planes()>1 && num_channels(pixel_format())>1 )
          throw ArgumentErr() << "Cannot read a multi-plane multi-channel image file into a single-channel buffer.";
        im_planes = std::max( planes(), num_channels(pixel_format()) );
      }
      buf.set_size( cols(), rows(), im_planes );

      read( GenericImageBuffer(buf) );
    }

    /// Read a block of the image on disk into the given image view,
    /// resizing the view if needed.
    template <class PixelT>
    void read( ImageView<PixelT>& buf, BBox2i bbox ) const {

      int im_planes = 1;
      if( ! IsCompound<PixelT>::value ) {
        // The image has a fundamental pixel type
        if( planes()>1 && num_channels(pixel_format())>1 )
          throw ArgumentErr() << "Cannot read a multi-plane multi-channel image file into a single-channel buffer.";
        im_planes = std::max( planes(), num_channels(pixel_format()) );
      }
      buf.set_size( bbox.width(), bbox.height(), im_planes );

      read( GenericImageBuffer(buf), bbox );
    }

    /// Write the given image view into the image on disk.
    template <class PixelT>
    void write( ImageView<PixelT> const& buf ) const {
      write( GenericImageBuffer(buf) );
    }

    /// Create a new DiskImageResource of the appropriate type
    /// pointing to an existing file on disk.
    ///
    /// Don't forget to delete the DiskImageResource object when
    /// you're finished with it!
    static DiskImageResource* open( std::string const& filename );

    /// Create a new DiskImageResource of the appropriate type 
    /// pointing to a newly-created empty file on disk.
    ///
    /// The underlying driver chooses a file format that it supports
    /// that matches the one you requested as closely as possible.  
    /// If you care exactly what format it chose, you can ask the
    /// resource after it's created.  If you need finer-grained
    /// control you must manually create a resource of the appropraite
    /// type.  Don't forget to delete this DiskImageResource object
    /// when you're finished with it!
    static DiskImageResource* create( std::string const& filename, GenericImageFormat const& format );

    typedef DiskImageResource* (*construct_open_func)( std::string const& filename );
    
    typedef DiskImageResource* (*construct_create_func)( std::string const& filename,
                                                         GenericImageFormat const& format );

    static void register_file_type( std::string const& extension,
                                    construct_open_func open_func,
                                    construct_create_func create_func );

  protected:
    GenericImageFormat m_format;
  };


  // *******************************************************************
  // Free functions using the DiskImageResource interface
  // *******************************************************************

  /// Read an image on disk into a vw::ImageView<T> object.  If you
  /// supply a filename with an asterisk ('*'), each plane of the
  /// image will be read from a seperate file on disk with the asterisk
  /// replaced by the plane number.
  template <class PixelT>
  void read_image( ImageView<PixelT>& in_image, const std::string &filename ) {

    // XXX If there's an asterisk in the filename, handle it here.
    if( boost::find_last(filename,"*") )
      throw NoImplErr() << "File-per-plane reading is not yet supported.";

    vw_out(InfoMessage) << "\tLoading image: " << filename << "\t";

    // Open the file for reading
    DiskImageResource *r = DiskImageResource::open( filename );

    vw_out(InfoMessage) << r->cols() << "x" << r->rows() << "x" << r->planes() << "  " << r->channels() << " channel(s)\n";

    // Read it in and wrap up
    r->read( in_image );
    delete r;
  }


  /// Write an vw::ImageView<T> to disk.  If you supply a filename
  /// with an asterisk ('*'), each plane of the image will be saved as
  /// a seperate file on disk and the asterisk will be replaced with
  /// the plane number.
  template <class ImageT>
  void write_image( const std::string &filename, ImageViewBase<ImageT> const& out_image ) {

    VW_ASSERT( out_image.impl().cols() != 0 && out_image.impl().rows() != 0 && out_image.impl().planes() != 0,
               ArgumentErr() << "write_image: cannot write empty image to disk" );

    // Rasterize the image if needed
    ImageView<typename ImageT::pixel_type> image( out_image.impl() );
    GenericImageBuffer buf(image);

    int files = 1;
    // If there's an asterisk, save one file per plane
    if( boost::find_last(filename,"*") ) {
      files = buf.format.planes;
      buf.format.planes = 1;
    }
    
    for( int i=0; i<files; ++i ) {
      std::string name = filename;
      if( files > 1 ) boost::replace_last( name, "*",  str( boost::format("%1%") % i ) );
      vw_out(InfoMessage) << "\tSaving image: " << name << "\t";
      DiskImageResource *r = DiskImageResource::create( name, buf.format );
      vw_out(InfoMessage) << r->cols() << "x" << r->rows() << "x" << r->planes() << "  " << r->channels() << " channel(s)\n";
      r->write( buf );
      delete r;
      buf.data = (uint8*)buf.data + buf.pstride;
    }
  }

  /// Write an vw::ImageView<T> to disk.  If you supply a filename
  /// with an asterisk ('*'), each plane of the image will be saved as
  /// a seperate file on disk and the asterisk will be replaced with
  /// the plane number.
  template <class ElemT>
    void write_image( const std::string &filename, std::vector<ElemT> const& out_image_vector ) {

    // If there's an asterisk, save one file per plane
    if( ! boost::find_last(filename,"*") ) {
      throw vw::ArgumentErr() << "write_image: filename must contain * when writing a vector of image views\n";
    }

    for (int i=0; i<out_image_vector.size(); i++){
      std::string name = filename;
      boost::replace_last( name, "*",  str( boost::format("%1%") % i ) );
      write_image( name, out_image_vector[i] );
    }
  }

} // namespace vw

#endif // __VW_FILEIO_DISK_IMAGE_RESOURCE_H__
