// An image on the Canvas
//
// Copyright (C) 2012  Thomas Geymayer <tomgey@gmail.com>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA

#include "CanvasImage.hxx"

#include <simgear/canvas/Canvas.hxx>
#include <simgear/canvas/CanvasMgr.hxx>
#include <simgear/canvas/CanvasSystemAdapter.hxx>
#include <simgear/canvas/MouseEvent.hxx>
#include <simgear/scene/util/OsgMath.hxx>
#include <simgear/scene/util/parse_color.hxx>
#include <simgear/misc/sg_path.hxx>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/PrimitiveSet>

#include <boost/algorithm/string/predicate.hpp>

namespace simgear
{
namespace canvas
{
  /**
   * Callback to enable/disable rendering of canvas displayed inside windows or
   * other canvases.
   */
  class CullCallback:
    public osg::Drawable::CullCallback
  {
    public:
      CullCallback(const CanvasWeakPtr& canvas);
      void cullNextFrame();

    private:
      CanvasWeakPtr _canvas;
      mutable bool  _cull_next_frame;

      virtual bool cull( osg::NodeVisitor* nv,
                         osg::Drawable* drawable,
                         osg::RenderInfo* renderInfo ) const;
  };

  //----------------------------------------------------------------------------
  CullCallback::CullCallback(const CanvasWeakPtr& canvas):
    _canvas( canvas ),
    _cull_next_frame( false )
  {

  }

  //----------------------------------------------------------------------------
  void CullCallback::cullNextFrame()
  {
    _cull_next_frame = true;
  }

  //----------------------------------------------------------------------------
  bool CullCallback::cull( osg::NodeVisitor* nv,
                           osg::Drawable* drawable,
                           osg::RenderInfo* renderInfo ) const
  {
    if( !_canvas.expired() )
      _canvas.lock()->enableRendering();

    if( !_cull_next_frame )
      // TODO check if window/image should be culled
      return false;

    _cull_next_frame = false;
    return true;
  }

  //----------------------------------------------------------------------------
  const std::string Image::TYPE_NAME = "image";

  //----------------------------------------------------------------------------
  void Image::staticInit()
  {
    if( isInit<Image>() )
      return;

    addStyle("fill", "color", &Image::setFill);
    addStyle("slice", "", &Image::setSlice);
    addStyle("slice-width", "", &Image::setSliceWidth);
    addStyle("outset", "", &Image::setOutset);
  }

  //----------------------------------------------------------------------------
  Image::Image( const CanvasWeakPtr& canvas,
                const SGPropertyNode_ptr& node,
                const Style& parent_style,
                Element* parent ):
    Element(canvas, node, parent_style, parent),
    _texture(new osg::Texture2D),
    _node_src_rect( node->getNode("source", 0, true) ),
    _src_rect(0,0),
    _region(0,0)
  {
    staticInit();

    _geom = new osg::Geometry;
    _geom->setUseDisplayList(false);

    osg::StateSet *stateSet = _geom->getOrCreateStateSet();
    stateSet->setTextureAttributeAndModes(0, _texture.get());
    stateSet->setDataVariance(osg::Object::STATIC);

    // allocate arrays for the image
    _vertices = new osg::Vec3Array(4);
    _vertices->setDataVariance(osg::Object::DYNAMIC);
    _geom->setVertexArray(_vertices);

    _texCoords = new osg::Vec2Array(4);
    _texCoords->setDataVariance(osg::Object::DYNAMIC);
    _geom->setTexCoordArray(0, _texCoords);

    _colors = new osg::Vec4Array(1);
    _colors->setDataVariance(osg::Object::DYNAMIC);
    _geom->setColorArray(_colors);
    _geom->setColorBinding(osg::Geometry::BIND_OVERALL);

    _prim = new osg::DrawArrays(osg::PrimitiveSet::QUADS);
    _prim->set(osg::PrimitiveSet::QUADS, 0, 4);
    _prim->setDataVariance(osg::Object::DYNAMIC);
    _geom->addPrimitiveSet(_prim);

    setDrawable(_geom);

    setFill("#ffffff"); // TODO how should we handle default values?
    setupStyle();
  }

  //----------------------------------------------------------------------------
  Image::~Image()
  {

  }

  //----------------------------------------------------------------------------
  void Image::update(double dt)
  {
    Element::update(dt);

    osg::Texture2D* texture = dynamic_cast<osg::Texture2D*>
    (
      _geom->getOrCreateStateSet()
           ->getTextureAttribute(0, osg::StateAttribute::TEXTURE)
    );
    simgear::canvas::CanvasPtr canvas = _src_canvas.lock();

    if(    (_attributes_dirty & SRC_CANVAS)
           // check if texture has changed (eg. due to resizing)
        || (canvas && texture != canvas->getTexture()) )
    {
      _geom->getOrCreateStateSet()
           ->setTextureAttribute(0, canvas ? canvas->getTexture() : 0);

      if( !canvas || canvas->isInit() )
        _attributes_dirty &= ~SRC_CANVAS;
    }

    if( !_attributes_dirty )
      return;

    const SGRect<int>& tex_dim = getTextureDimensions();

    // http://www.w3.org/TR/css3-background/#border-image-slice

    // The ‘fill’ keyword, if present, causes the middle part of the image to be
    // preserved. (By default it is discarded, i.e., treated as empty.)
    bool fill = (_slice.getKeyword() == "fill");

    if( _attributes_dirty & DEST_SIZE )
    {
      size_t num_vertices = (_slice.isValid() ? (fill ? 9 : 8) : 1) * 4;

      if( num_vertices != _prim->getNumPrimitives() )
      {
        _vertices->resize(num_vertices);
        _texCoords->resize(num_vertices);
        _prim->setCount(num_vertices);
        _prim->dirty();

        _attributes_dirty |= SRC_RECT;
      }

      // http://www.w3.org/TR/css3-background/#border-image-outset
      SGRect<float> region = _region;
      if( _outset.isValid() )
      {
        const CSSBorder::Offsets& outset = _outset.getAbsOffsets(tex_dim);
        region.t() -= outset.t;
        region.r() += outset.r;
        region.b() += outset.b;
        region.l() -= outset.l;
      }

      if( !_slice.isValid() )
      {
        setQuad(0, region.getMin(), region.getMax());
      }
      else
      {
        /*
        Image slice, 9-scale, whatever it is called. The four corner images
        stay unscaled (tl, tr, bl, br) whereas the other parts are scaled to
        fill the remaining space up to the specified size.

        x[0] x[1]     x[2] x[3]
          |    |        |    |
          -------------------- - y[0]
          | tl |   top  | tr |
          -------------------- - y[1]
          |    |        |    |
          | l  |        |  r |
          | e  | center |  i |
          | f  |        |  g |
          | t  |        |  h |
          |    |        |  t |
          -------------------- - y[2]
          | bl | bottom | br |
          -------------------- - y[3]
         */

        const CSSBorder::Offsets& slice =
          (_slice_width.isValid() ? _slice_width : _slice)
          .getAbsOffsets(tex_dim);
        float x[4] = {
          region.l(),
          region.l() + slice.l,
          region.r() - slice.r,
          region.r()
        };
        float y[4] = {
          region.t(),
          region.t() + slice.t,
          region.b() - slice.b,
          region.b()
        };

        int i = 0;
        for(int ix = 0; ix < 3; ++ix)
          for(int iy = 0; iy < 3; ++iy)
          {
            if( ix == 1 && iy == 1 && !fill )
              // The ‘fill’ keyword, if present, causes the middle part of the
              // image to be filled.
              continue;

            setQuad( i++,
                     SGVec2f(x[ix    ], y[iy    ]),
                     SGVec2f(x[ix + 1], y[iy + 1]) );
          }
      }

      _vertices->dirty();
      _attributes_dirty &= ~DEST_SIZE;
      _geom->dirtyBound();
      setBoundingBox(_geom->getBound());
    }

    if( _attributes_dirty & SRC_RECT )
    {
      SGRect<float> src_rect = _src_rect;
      if( !_node_src_rect->getBoolValue("normalized", true) )
      {
        src_rect.t() /= tex_dim.height();
        src_rect.r() /= tex_dim.width();
        src_rect.b() /= tex_dim.height();
        src_rect.l() /= tex_dim.width();
      }

      // Image coordinate systems y-axis is flipped
      std::swap(src_rect.t(), src_rect.b());

      if( !_slice.isValid() )
      {
        setQuadUV(0, src_rect.getMin(), src_rect.getMax());
      }
      else
      {
        const CSSBorder::Offsets& slice = _slice.getRelOffsets(tex_dim);
        float x[4] = {
          src_rect.l(),
          src_rect.l() + slice.l,
          src_rect.r() - slice.r,
          src_rect.r()
        };
        float y[4] = {
          src_rect.t(),
          src_rect.t() - slice.t,
          src_rect.b() + slice.b,
          src_rect.b()
        };

        int i = 0;
        for(int ix = 0; ix < 3; ++ix)
          for(int iy = 0; iy < 3; ++iy)
          {
            if( ix == 1 && iy == 1 && !fill )
              // The ‘fill’ keyword, if present, causes the middle part of the
              // image to be filled.
              continue;

            setQuadUV( i++,
                       SGVec2f(x[ix    ], y[iy    ]),
                       SGVec2f(x[ix + 1], y[iy + 1]) );
          }
      }

      _texCoords->dirty();
      _attributes_dirty &= ~SRC_RECT;
    }
  }

  //----------------------------------------------------------------------------
  void Image::valueChanged(SGPropertyNode* child)
  {
    // If the image is switched from invisible to visible, and it shows a
    // canvas, we need to delay showing it by one frame to ensure the canvas is
    // updated before the image is displayed.
    //
    // As canvas::Element handles and filters changes to the "visible" property
    // we can not check this in Image::childChanged but instead have to override
    // Element::valueChanged.
    if(    !isVisible()
        && child->getParent() == _node
        && child->getNameString() == "visible"
        && child->getBoolValue() )
    {
      CullCallback* cb = static_cast<CullCallback*>(_geom->getCullCallback());
      if( cb )
        cb->cullNextFrame();
    }

    Element::valueChanged(child);
  }

  //----------------------------------------------------------------------------
  void Image::setSrcCanvas(CanvasPtr canvas)
  {
    CanvasPtr src_canvas = _src_canvas.lock(),
              self_canvas = _canvas.lock();

    if( src_canvas )
      src_canvas->removeParentCanvas(self_canvas);
    if( self_canvas )
      self_canvas->removeChildCanvas(src_canvas);

    _src_canvas = src_canvas = canvas;
    _attributes_dirty |= SRC_CANVAS;
    _geom->setCullCallback(canvas ? new CullCallback(canvas) : 0);

    if( src_canvas )
    {
      setupDefaultDimensions();

      if( self_canvas )
      {
        self_canvas->addChildCanvas(src_canvas);
        src_canvas->addParentCanvas(self_canvas);
      }
    }
  }

  //----------------------------------------------------------------------------
  CanvasWeakPtr Image::getSrcCanvas() const
  {
    return _src_canvas;
  }

  //----------------------------------------------------------------------------
  void Image::setImage(osg::Image *img)
  {
    // remove canvas...
    setSrcCanvas( CanvasPtr() );

    _texture->setResizeNonPowerOfTwoHint(false);
    _texture->setImage(img);
    _texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    _texture->setWrap(osg::Texture::WRAP_T, osg::Texture::REPEAT);
    _geom->getOrCreateStateSet()
         ->setTextureAttributeAndModes(0, _texture);

    if( img )
      setupDefaultDimensions();
  }

  //----------------------------------------------------------------------------
  void Image::setFill(const std::string& fill)
  {
    osg::Vec4 color(1,1,1,1);
    if(    !fill.empty() // If no color is given default to white
        && !parseColor(fill, color) )
      return;

    _colors->front() = color;
    _colors->dirty();
  }

  //----------------------------------------------------------------------------
  void Image::setSlice(const std::string& slice)
  {
    _slice = CSSBorder::parse(slice);
    _attributes_dirty |= SRC_RECT | DEST_SIZE;
  }

  //----------------------------------------------------------------------------
  void Image::setSliceWidth(const std::string& width)
  {
    _slice_width = CSSBorder::parse(width);
    _attributes_dirty |= DEST_SIZE;
  }

  //----------------------------------------------------------------------------
  void Image::setOutset(const std::string& outset)
  {
    _outset = CSSBorder::parse(outset);
    _attributes_dirty |= DEST_SIZE;
  }

  //----------------------------------------------------------------------------
  const SGRect<float>& Image::getRegion() const
  {
    return _region;
  }

  //----------------------------------------------------------------------------
  bool Image::handleEvent(EventPtr event)
  {
    bool handled = Element::handleEvent(event);

    CanvasPtr src_canvas = _src_canvas.lock();
    if( !src_canvas )
      return handled;

    MouseEventPtr mouse_event = boost::dynamic_pointer_cast<MouseEvent>(event);
    if( mouse_event )
    {
      mouse_event.reset( new MouseEvent(*mouse_event) );
      event = mouse_event;

      mouse_event->client_pos = mouse_event->local_pos
                              - toOsg(_region.getMin());

      osg::Vec2f size(_region.width(), _region.height());
      if( _outset.isValid() )
      {
        CSSBorder::Offsets outset =
          _outset.getAbsOffsets(getTextureDimensions());

        mouse_event->client_pos += osg::Vec2f(outset.l, outset.t);
        size.x() += outset.l + outset.r;
        size.y() += outset.t + outset.b;
      }

      // Scale event pos according to canvas view size vs. displayed/screen size
      mouse_event->client_pos.x() *= src_canvas->getViewWidth() / size.x();
      mouse_event->client_pos.y() *= src_canvas->getViewHeight()/ size.y();
      mouse_event->local_pos = mouse_event->client_pos;
    }

    return handled | src_canvas->handleMouseEvent(mouse_event);
  }

  //----------------------------------------------------------------------------
  void Image::childChanged(SGPropertyNode* child)
  {
    const std::string& name = child->getNameString();

    if( child->getParent() == _node_src_rect )
    {
      _attributes_dirty |= SRC_RECT;

      if(      name == "left" )
        _src_rect.setLeft( child->getFloatValue() );
      else if( name == "right" )
        _src_rect.setRight( child->getFloatValue() );
      else if( name == "top" )
        _src_rect.setTop( child->getFloatValue() );
      else if( name == "bottom" )
        _src_rect.setBottom( child->getFloatValue() );

      return;
    }
    else if( child->getParent() != _node )
      return;

    if( name == "x" )
    {
      _region.setX( child->getFloatValue() );
      _attributes_dirty |= DEST_SIZE;
    }
    else if( name == "y" )
    {
      _region.setY( child->getFloatValue() );
      _attributes_dirty |= DEST_SIZE;
    }
    else if( name == "size" )
    {
      if( child->getIndex() == 0 )
        _region.setWidth( child->getFloatValue() );
      else
        _region.setHeight( child->getFloatValue() );

      _attributes_dirty |= DEST_SIZE;
    }
    else if( name == "file" )
    {
      static const std::string CANVAS_PROTOCOL = "canvas://";
      const std::string& path = child->getStringValue();

      CanvasPtr canvas = _canvas.lock();
      if( !canvas )
      {
        SG_LOG(SG_GL, SG_ALERT, "canvas::Image: No canvas available");
        return;
      }

      if( boost::starts_with(path, CANVAS_PROTOCOL) )
      {
        CanvasMgr* canvas_mgr = canvas->getCanvasMgr();
        if( !canvas_mgr )
        {
          SG_LOG(SG_GL, SG_ALERT, "canvas::Image: Failed to get CanvasMgr");
          return;
        }

        const SGPropertyNode* canvas_node =
          canvas_mgr->getPropertyRoot()
                    ->getParent()
                    ->getNode( path.substr(CANVAS_PROTOCOL.size()) );
        if( !canvas_node )
        {
          SG_LOG(SG_GL, SG_ALERT, "canvas::Image: No such canvas: " << path);
          return;
        }

        // TODO add support for other means of addressing canvases (eg. by
        // name)
        CanvasPtr src_canvas = canvas_mgr->getCanvas( canvas_node->getIndex() );
        if( !src_canvas )
        {
          SG_LOG(SG_GL, SG_ALERT, "canvas::Image: Invalid canvas: " << path);
          return;
        }

        setSrcCanvas(src_canvas);
      }
      else
      {
        setImage( canvas->getSystemAdapter()->getImage(path) );
      }
    }
  }

  //----------------------------------------------------------------------------
  void Image::setupDefaultDimensions()
  {
    if( !_src_rect.width() || !_src_rect.height() )
    {
      // Show whole image by default
      _node_src_rect->setBoolValue("normalized", true);
      _node_src_rect->setFloatValue("right", 1);
      _node_src_rect->setFloatValue("bottom", 1);
    }

    if( !_region.width() || !_region.height() )
    {
      // Default to image size.
      // TODO handle showing only part of image?
      const SGRect<int>& dim = getTextureDimensions();
      _node->setFloatValue("size[0]", dim.width());
      _node->setFloatValue("size[1]", dim.height());
    }
  }

  //----------------------------------------------------------------------------
  SGRect<int> Image::getTextureDimensions() const
  {
    CanvasPtr canvas = _src_canvas.lock();
    SGRect<int> dim(0,0);

    // Use canvas/image dimensions rather than texture dimensions, as they could
    // be resized internally by OpenSceneGraph (eg. to nearest power of two).
    if( canvas )
    {
      dim.setRight( canvas->getViewWidth() );
      dim.setBottom( canvas->getViewHeight() );
    }
    else if( _texture )
    {
      osg::Image* img = _texture->getImage();

      if( img )
      {
        dim.setRight( img->s() );
        dim.setBottom( img->t() );
      }
    }

    return dim;
  }

  //----------------------------------------------------------------------------
  void Image::setQuad(size_t index, const SGVec2f& tl, const SGVec2f& br)
  {
    int i = index * 4;
    (*_vertices)[i + 0].set(tl.x(), tl.y(), 0);
    (*_vertices)[i + 1].set(br.x(), tl.y(), 0);
    (*_vertices)[i + 2].set(br.x(), br.y(), 0);
    (*_vertices)[i + 3].set(tl.x(), br.y(), 0);
  }

  //----------------------------------------------------------------------------
  void Image::setQuadUV(size_t index, const SGVec2f& tl, const SGVec2f& br)
  {
    int i = index * 4;
    (*_texCoords)[i + 0].set(tl.x(), tl.y());
    (*_texCoords)[i + 1].set(br.x(), tl.y());
    (*_texCoords)[i + 2].set(br.x(), br.y());
    (*_texCoords)[i + 3].set(tl.x(), br.y());
  }

} // namespace canvas
} // namespace simgear
