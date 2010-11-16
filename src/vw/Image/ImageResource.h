// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// \file ImageResource.h
///
/// Defines the abstract base image resource type.
///
#ifndef __VW_IMAGE_IMAGERESOURCE_H__
#define __VW_IMAGE_IMAGERESOURCE_H__

#include <vw/Math/Vector.h>
#include <vw/Math/BBox.h>

#include <vw/Image/PixelTypeInfo.h>

namespace vw {

  // Forward declaration
  struct ImageBuffer;


  /// Copies image pixel data from the source buffer to the destination
  /// buffer, converting the pixel format and channel type as required.
  void convert( ImageBuffer const& dst, ImageBuffer const& src, bool rescale=false );


  /// Describes the format of an image, i.e. its dimensions, pixel
  /// structure, and channel type.
  struct ImageFormat {
    uint32 cols, rows, planes;
    PixelFormatEnum pixel_format;
    ChannelTypeEnum channel_type;

    ImageFormat()
      : cols(0), rows(0), planes(0),
        pixel_format(VW_PIXEL_UNKNOWN),
        channel_type(VW_CHANNEL_UNKNOWN)
    {}

    // Does this represent a fully-specified data format?
    bool complete() const {
      return   cols != 0
          &&   rows != 0
          && planes != 0
          && num_channels_nothrow(pixel_format) > 0
          && channel_size_nothrow(channel_type) > 0;
    }

    inline bool simple_convert(const ImageFormat& b) const {
      return same_size(b)
          && simple_conversion(channel_type, b.channel_type)
          && simple_conversion(pixel_format, b.pixel_format);
    }

    inline bool same_size(const ImageFormat& b) const {
      return cols == b.cols && rows == b.rows && planes == b.planes;
    }
  };

  // A read-only image resource
  class SrcImageResource {
    public:
      virtual ~SrcImageResource() {}

      /// Returns the number of columns in an image resource.
      virtual int32 cols() const = 0;

      /// Returns the number of rows in an image resource.
      virtual int32 rows() const = 0;

      /// Returns the number of planes in an image resource.
      virtual int32 planes() const = 0;

      /// Returns the number of channels in a image resource.
      int32 channels() const { return num_channels( pixel_format() ); }

      /// Returns the native pixel format of the resource.
      virtual PixelFormatEnum pixel_format() const = 0;

      /// Returns the native channel type of the resource.
      virtual ChannelTypeEnum channel_type() const = 0;

      /// Read the image resource at the given location into the given buffer.
      virtual void read( ImageBuffer const& buf, BBox2i const& bbox ) const = 0;

      /// Does this resource support block reads?
      // If you override this to true, you must implement the other block_read functions
      virtual bool has_block_read() const = 0;

      /// Returns the preferred block size/alignment for partial reads.
      virtual Vector2i block_read_size() const { return Vector2i(cols(),rows()); }

      // Does this resource have a nodata value?
      // If you override this to true, you must implement the other nodata_read functions
      virtual bool has_nodata_read() const = 0;

      /// Fetch this ImageResource's nodata value
      virtual double nodata_read() const {
        vw_throw(NoImplErr() << "This ImageResource does not support nodata_read_value().");
      }
  };

  // A write-only image resource
  class DstImageResource {
    public:
      virtual ~DstImageResource() {}

      /// Write the given buffer to the image resource at the given location.
      virtual void write( ImageBuffer const& buf, BBox2i const& bbox ) = 0;

      // Does this resource support block writes?
      // If you override this to true, you must implement the other block_write functions
      virtual bool has_block_write() const = 0;

      /// Gets the preferred block size/alignment for partial writes.
      virtual Vector2i block_write_size() const {
        vw_throw(NoImplErr() << "This ImageResource does not support block writes");
      }

      /// Sets the preferred block size/alignment for partial writes.
      virtual void set_block_write_size(const Vector2i& /*v*/) {
        vw_throw(NoImplErr() << "This ImageResource does not support block writes");
      }

      // Does this resource have an output nodata value?
      // If you override this to true, you must implement the other nodata_write functions
      virtual bool has_nodata_write() const = 0;

      /// Set a nodata value that will be stored in the underlying stream
      virtual void set_nodata_write( double /*value*/ ) {
        vw_throw(NoImplErr() << "This ImageResource does not support set_nodata_write().");
      }

      /// Force any changes to be written to the resource.
      virtual void flush() = 0;
  };

  // A read-write image resource
  class ImageResource : public SrcImageResource, public DstImageResource {};

  /// Represents a generic image buffer in memory, with dimensions and
  /// pixel format specified at run time.  This class does not
  /// allocate any memory, but rather provides a common format for
  /// describing an existing in-memory buffer of pixels.  The primary
  /// purpose of this class is to provide some common ground for
  /// converting between image formats using the convert() function.
  /// To allocate a fresh buffer for an image, see ImageView.
  struct ImageBuffer {
    void* data;
    ImageFormat format;
    ssize_t cstride, rstride, pstride;
    bool unpremultiplied;

    /// Default constructor; constructs an undefined buffer
    ImageBuffer()
      : data(0), format(),
        cstride(0), rstride(0), pstride(0),
        unpremultiplied(false)
    {}

    /// Populates stride information from format
    explicit ImageBuffer(ImageFormat format, void *data, bool unpremultiplied = false)
      : data(data), format(format),
        cstride(channel_size(format.channel_type) * num_channels(format.pixel_format)),
        rstride(cstride * format.cols), pstride(rstride * format.rows),
        unpremultiplied(unpremultiplied)
    {}

    virtual ~ImageBuffer() {}

    /// Returns the number of columns in the bufffer.
    inline int32 cols() const { return format.cols; }

    /// Returns the number of rows in the bufffer.
    inline int32 rows() const { return format.rows; }

    /// Returns the number of planes in the bufffer.
    inline int32 planes() const { return format.planes; }

    /// Returns the native pixel format of the bufffer.
    inline PixelFormatEnum pixel_format() const { return format.pixel_format; }

    /// Returns the native channel type of the bufffer.
    inline ChannelTypeEnum channel_type() const { return format.channel_type; }

    /// Returns the size (in bytes) of the data described by this buffer
    inline size_t byte_size() const {
      return planes() * pstride;
    }

    /// Returns a cropped version of this bufffer.
    inline ImageBuffer cropped( BBox2i const& bbox ) const {
      ImageBuffer self = *this;
      self.data = (uint8*)self.data + cstride*bbox.min().x() + rstride*bbox.min().y();
      self.format.cols = bbox.width();
      self.format.rows = bbox.height();
      return self;
    }

    /// Read the image resource at the given location into the given buffer.
    inline void read( ImageBuffer const& buf, BBox2i const& bbox ) const {
      convert( buf, cropped(bbox) );
    }

    /// Write the given buffer to the image resource at the given location.
    inline void write( ImageBuffer const& buf, BBox2i const& bbox ) {
      convert( cropped(bbox), buf );
    }

    /// Return a pointer to the pixel at (u,v,p)
    inline void* operator()( int32 i, int32 j, int32 p = 0 ) const {
      return ((uint8*)data) + (i*cstride + j*rstride + p*pstride);
    }

  };

} // namespace vw

#endif // __VW_IMAGE_IMAGERESOURCE_H__
