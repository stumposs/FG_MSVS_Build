// Interface for 2D Canvas element
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

#include "CanvasElement.hxx"
#include <simgear/canvas/Canvas.hxx>
#include <simgear/canvas/CanvasEventListener.hxx>
#include <simgear/canvas/CanvasEventVisitor.hxx>
#include <simgear/canvas/MouseEvent.hxx>
#include <simgear/math/SGMisc.hxx>
#include <simgear/misc/strutils.hxx>
#include <simgear/scene/material/parseBlendFunc.hxx>

#include <osg/Drawable>
#include <osg/Geode>
#include <osg/Scissor>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <cassert>
#include <cstring>

namespace simgear
{
namespace canvas
{
  const std::string NAME_TRANSFORM = "tf";

  //----------------------------------------------------------------------------
  Element::OSGUserData::OSGUserData(ElementPtr element):
    element(element)
  {

  }

  //----------------------------------------------------------------------------
  Element::~Element()
  {

  }

  //----------------------------------------------------------------------------
  void Element::setSelf(const PropertyBasedElementPtr& self)
  {
    PropertyBasedElement::setSelf(self);

    _transform->setUserData
    (
      new OSGUserData(boost::static_pointer_cast<Element>(self))
    );
  }

  //----------------------------------------------------------------------------
  void Element::onDestroy()
  {
    if( !_transform.valid() )
      return;

    // The transform node keeps a reference on this element, so ensure it is
    // deleted.
    BOOST_FOREACH(osg::Group* parent, _transform->getParents())
    {
      parent->removeChild(_transform.get());
    }
  }

  //----------------------------------------------------------------------------
  ElementWeakPtr Element::getWeakPtr() const
  {
    return boost::static_pointer_cast<Element>(_self.lock());
  }

  //----------------------------------------------------------------------------
  void Element::update(double dt)
  {
    if( !_transform->getNodeMask() )
      // Don't do anything if element is hidden
      return;

    if( _transform_dirty )
    {
      osg::Matrix m;
      for( size_t i = 0; i < _transform_types.size(); ++i )
      {
        // Skip unused indizes...
        if( _transform_types[i] == TT_NONE )
          continue;

        SGPropertyNode* tf_node = _node->getChild("tf", i, true);

        // Build up the matrix representation of the current transform node
        osg::Matrix tf;
        switch( _transform_types[i] )
        {
          case TT_MATRIX:
            tf = osg::Matrix( tf_node->getDoubleValue("m[0]", 1),
                              tf_node->getDoubleValue("m[1]", 0),
                              0,
                              tf_node->getDoubleValue("m[6]", 0),

                              tf_node->getDoubleValue("m[2]", 0),
                              tf_node->getDoubleValue("m[3]", 1),
                              0,
                              tf_node->getDoubleValue("m[7]", 0),

                              0,
                              0,
                              1,
                              0,

                              tf_node->getDoubleValue("m[4]", 0),
                              tf_node->getDoubleValue("m[5]", 0),
                              0,
                              tf_node->getDoubleValue("m[8]", 1) );
            break;
          case TT_TRANSLATE:
            tf.makeTranslate( osg::Vec3f( tf_node->getDoubleValue("t[0]", 0),
                                          tf_node->getDoubleValue("t[1]", 0),
                                          0 ) );
            break;
          case TT_ROTATE:
            tf.makeRotate( tf_node->getDoubleValue("rot", 0), 0, 0, 1 );
            break;
          case TT_SCALE:
          {
            float sx = tf_node->getDoubleValue("s[0]", 1);
            // sy defaults to sx...
            tf.makeScale( sx, tf_node->getDoubleValue("s[1]", sx), 1 );
            break;
          }
          default:
            break;
        }
        m.postMult( tf );
      }
      _transform->setMatrix(m);
      _transform_dirty = false;
    }

    // Update bounding box on manual update (manual updates pass zero dt)
    if( dt == 0 && _drawable )
      _drawable->getBound();

    if( _attributes_dirty & BLEND_FUNC )
    {
      parseBlendFunc(
        _transform->getOrCreateStateSet(),
        _node->getChild("blend-source"),
        _node->getChild("blend-destination"),
        _node->getChild("blend-source-rgb"),
        _node->getChild("blend-destination-rgb"),
        _node->getChild("blend-source-alpha"),
        _node->getChild("blend-destination-alpha")
      );
      _attributes_dirty &= ~BLEND_FUNC;
    }
  }

  //----------------------------------------------------------------------------
  naRef Element::addEventListener(const nasal::CallContext& ctx)
  {
    const std::string type_str = ctx.requireArg<std::string>(0);
    naRef code = ctx.requireArg<naRef>(1);

    SG_LOG
    (
      SG_NASAL,
      SG_INFO,
      "addEventListener(" << _node->getPath() << ", " << type_str << ")"
    );

    Event::Type type = Event::strToType(type_str);
    if( type == Event::UNKNOWN )
      naRuntimeError( ctx.c,
                      "addEventListener: Unknown event type %s",
                      type_str.c_str() );

    _listener[ type ].push_back
    (
      boost::make_shared<EventListener>( code,
                                         _canvas.lock()->getSystemAdapter() )
    );

    return naNil();
  }

  //----------------------------------------------------------------------------
  void Element::clearEventListener()
  {
    _listener.clear();
  }

  //----------------------------------------------------------------------------
  bool Element::accept(EventVisitor& visitor)
  {
    if( !isVisible() )
      return false;

    return visitor.apply(*this);
  }

  //----------------------------------------------------------------------------
  bool Element::ascend(EventVisitor& visitor)
  {
    if( _parent )
      return _parent->accept(visitor);
    return true;
  }

  //----------------------------------------------------------------------------
  bool Element::traverse(EventVisitor& visitor)
  {
    return true;
  }

  //----------------------------------------------------------------------------
  bool Element::handleEvent(canvas::EventPtr event)
  {
    ListenerMap::iterator listeners = _listener.find(event->getType());
    if( listeners == _listener.end() )
      return false;

    BOOST_FOREACH(EventListenerPtr listener, listeners->second)
      listener->call(event);

    return true;
  }

  //----------------------------------------------------------------------------
  bool Element::hitBound( const osg::Vec2f& pos,
                          const osg::Vec2f& local_pos ) const
  {
    const osg::Vec3f pos3(pos, 0);

    // Drawables have a bounding box...
    if( _drawable )
      return _drawable->getBound().contains(osg::Vec3f(local_pos, 0));
    // ... for other elements, i.e. groups only a bounding sphere is available
    else
      return _transform->getBound().contains(osg::Vec3f(pos, 0));
  }

  //----------------------------------------------------------------------------
  bool Element::isVisible() const
  {
    return _transform.valid() && _transform->getNodeMask() != 0;
  }

  //----------------------------------------------------------------------------
  osg::MatrixTransform* Element::getMatrixTransform()
  {
    return _transform.get();
  }

  //----------------------------------------------------------------------------
  osg::MatrixTransform const* Element::getMatrixTransform() const
  {
    return _transform.get();
  }

  //----------------------------------------------------------------------------
  void Element::childAdded(SGPropertyNode* parent, SGPropertyNode* child)
  {
    if(    parent == _node
        && child->getNameString() == NAME_TRANSFORM )
    {
      if( child->getIndex() >= static_cast<int>(_transform_types.size()) )
        _transform_types.resize( child->getIndex() + 1 );

      _transform_types[ child->getIndex() ] = TT_NONE;
      _transform_dirty = true;
      return;
    }
    else if(    parent->getParent() == _node
             && parent->getNameString() == NAME_TRANSFORM )
    {
      assert(parent->getIndex() < static_cast<int>(_transform_types.size()));

      const std::string& name = child->getNameString();

      TransformType& type = _transform_types[parent->getIndex()];

      if(      name == "m" )
        type = TT_MATRIX;
      else if( name == "t" )
        type = TT_TRANSLATE;
      else if( name == "rot" )
        type = TT_ROTATE;
      else if( name == "s" )
        type = TT_SCALE;

      _transform_dirty = true;
      return;
    }

    childAdded(child);
  }

  //----------------------------------------------------------------------------
  void Element::childRemoved(SGPropertyNode* parent, SGPropertyNode* child)
  {
    if( parent == _node )
    {
      if( child->getNameString() == NAME_TRANSFORM )
      {
        if( !_transform.valid() )
          return;

        if( child->getIndex() >= static_cast<int>(_transform_types.size()) )
        {
          SG_LOG
          (
            SG_GENERAL,
            SG_WARN,
            "Element::childRemoved: unknown transform: " << child->getPath()
          );
          return;
        }

        _transform_types[ child->getIndex() ] = TT_NONE;

        while( !_transform_types.empty() && _transform_types.back() == TT_NONE )
          _transform_types.pop_back();

        _transform_dirty = true;
        return;
      }
      else if( StyleInfo const* style = getStyleInfo(child->getNameString()) )
      {
        if( setStyle(getParentStyle(child), style) )
          return;
      }
    }

    childRemoved(child);
  }

  //----------------------------------------------------------------------------
  void Element::valueChanged(SGPropertyNode* child)
  {
    SGPropertyNode *parent = child->getParent();
    if( parent == _node )
    {
      const std::string& name = child->getNameString();
      if( StyleInfo const* style_info = getStyleInfo(name) )
      {
        SGPropertyNode const* style = child;
        if( isStyleEmpty(child) )
        {
          child->clearValue();
          style = getParentStyle(child);
        }
        setStyle(style, style_info);
        return;
      }
      else if( name == "update" )
        return update(0);
      else if( name == "visible" )
        // TODO check if we need another nodemask
        return _transform->setNodeMask( child->getBoolValue() ? 0xffffffff : 0 );
      else if( boost::starts_with(name, "blend-") )
        return (void)(_attributes_dirty |= BLEND_FUNC);
    }
    else if(   parent->getParent() == _node
            && parent->getNameString() == NAME_TRANSFORM )
    {
      _transform_dirty = true;
      return;
    }

    childChanged(child);
  }

  //----------------------------------------------------------------------------
  bool Element::setStyle( const SGPropertyNode* child,
                          const StyleInfo* style_info )
  {
    return canApplyStyle(child) && setStyleImpl(child, style_info);
  }

  //----------------------------------------------------------------------------
  void Element::setClip(const std::string& clip)
  {
    if( clip.empty() || clip == "auto" )
    {
      getOrCreateStateSet()->removeAttribute(osg::StateAttribute::SCISSOR);
      return;
    }

    // TODO generalize CSS property parsing
    const std::string RECT("rect(");
    if(    !boost::ends_with(clip, ")")
        || !boost::starts_with(clip, RECT) )
    {
      SG_LOG(SG_GENERAL, SG_WARN, "Canvas: invalid clip: " << clip);
      return;
    }

    const std::string sep(", \t\npx");
    int comp = 0;
    float values[4];

    for(size_t pos = RECT.size(); comp < 4; ++comp)
    {
      pos = clip.find_first_not_of(sep, pos);
      if( pos == std::string::npos || pos == clip.size() - 1 )
        break;

      char *end = 0;
      values[comp] = strtod(&clip[pos], &end);
      if( end == &clip[pos] || !end )
        break;

      pos = end - &clip[0];
    }

    if( comp < 4 )
    {
      SG_LOG(SG_GENERAL, SG_WARN, "Canvas: invalid clip: " << clip);
      return;
    }

    float width = values[1] - values[3],
          height = values[2] - values[0];

    if( width < 0 || height < 0 )
    {
      SG_LOG(SG_GENERAL, SG_WARN, "Canvas: negative clip size: " << clip);
      return;
    }

    float scale_x = 1,
          scale_y = 1;

    CanvasPtr canvas = _canvas.lock();
    if( canvas )
    {
      // The scissor rectangle isn't affected by any transformation, so we need
      // to convert to image/canvas coordinates on our selves.
      scale_x = canvas->getSizeX()
              / static_cast<float>(canvas->getViewWidth());
      scale_y = canvas->getSizeY()
              / static_cast<float>(canvas->getViewHeight());
    }

    osg::Scissor* scissor = new osg::Scissor();
    // <top>, <right>, <bottom>, <left>
    scissor->x() = SGMiscf::roundToInt(scale_x * values[3]);
    scissor->y() = SGMiscf::roundToInt(scale_y * values[0]);
    scissor->width() = SGMiscf::roundToInt(scale_x * width);
    scissor->height() = SGMiscf::roundToInt(scale_y * height);

    if( canvas )
      // Canvas has y axis upside down
      scissor->y() = canvas->getSizeY() - scissor->y() - scissor->height();

    getOrCreateStateSet()->setAttributeAndModes(scissor);
  }

  //----------------------------------------------------------------------------
  void Element::setBoundingBox(const osg::BoundingBox& bb)
  {
    if( _bounding_box.empty() )
    {
      SGPropertyNode* bb_node = _node->getChild("bounding-box", 0, true);
      _bounding_box.resize(4);
      _bounding_box[0] = bb_node->getChild("min-x", 0, true);
      _bounding_box[1] = bb_node->getChild("min-y", 0, true);
      _bounding_box[2] = bb_node->getChild("max-x", 0, true);
      _bounding_box[3] = bb_node->getChild("max-y", 0, true);
    }

    _bounding_box[0]->setFloatValue(bb._min.x());
    _bounding_box[1]->setFloatValue(bb._min.y());
    _bounding_box[2]->setFloatValue(bb._max.x());
    _bounding_box[3]->setFloatValue(bb._max.y());
  }

  //----------------------------------------------------------------------------
  osg::BoundingBox Element::getTransformedBounds(const osg::Matrix& m) const
  {
    if( !_drawable )
      return osg::BoundingBox();

    osg::BoundingBox transformed;
    const osg::BoundingBox& bb = _drawable->getBound();
    for(int i = 0; i < 4; ++i)
      transformed.expandBy( bb.corner(i) * m );

    return transformed;
  }

  //----------------------------------------------------------------------------
  Element::StyleSetters Element::_style_setters;

  //----------------------------------------------------------------------------
  Element::Element( const CanvasWeakPtr& canvas,
                    const SGPropertyNode_ptr& node,
                    const Style& parent_style,
                    Element* parent ):
    PropertyBasedElement(node),
    _canvas( canvas ),
    _parent( parent ),
    _attributes_dirty( 0 ),
    _transform_dirty( false ),
    _transform( new osg::MatrixTransform ),
    _style( parent_style ),
    _drawable( 0 )
  {
    staticInit();

    SG_LOG
    (
      SG_GL,
      SG_DEBUG,
      "New canvas element " << node->getPath()
    );

    // Ensure elements are drawn in order they appear in the element tree
    _transform->getOrCreateStateSet()
              ->setRenderBinDetails
              (
                0,
                "PreOrderBin",
                osg::StateSet::OVERRIDE_RENDERBIN_DETAILS
              );
  }

  //----------------------------------------------------------------------------
  void Element::staticInit()
  {
    if( isInit<Element>() )
      return;

    addStyle("clip", "", &Element::setClip, false);
  }

  //----------------------------------------------------------------------------
  bool Element::isStyleEmpty(const SGPropertyNode* child) const
  {
    return !child
        || simgear::strutils::strip(child->getStringValue()).empty();
  }

  //----------------------------------------------------------------------------
  bool Element::canApplyStyle(const SGPropertyNode* child) const
  {
    if( _node == child->getParent() )
      return true;

    // Parent values do not override if element has own value
    return isStyleEmpty( _node->getChild(child->getName()) );
  }

  //----------------------------------------------------------------------------
  bool Element::setStyleImpl( const SGPropertyNode* child,
                              const StyleInfo* style_info )
  {
    const StyleSetter* style_setter = style_info
                                    ? &style_info->setter
                                    : getStyleSetter(child->getNameString());
    while( style_setter )
    {
      if( style_setter->func(*this, child) )
        return true;
      style_setter = style_setter->next;
    }
    return false;
  }

  //----------------------------------------------------------------------------
  const Element::StyleInfo*
  Element::getStyleInfo(const std::string& name) const
  {
    StyleSetters::const_iterator setter = _style_setters.find(name);
    if( setter == _style_setters.end() )
      return 0;

    return &setter->second;
  }

  //----------------------------------------------------------------------------
  const Element::StyleSetter*
  Element::getStyleSetter(const std::string& name) const
  {
    const StyleInfo* info = getStyleInfo(name);
    return info ? &info->setter : 0;
  }

  //----------------------------------------------------------------------------
  const SGPropertyNode*
  Element::getParentStyle(const SGPropertyNode* child) const
  {
    // Try to get value from parent...
    if( _parent )
    {
      Style::const_iterator style =
        _parent->_style.find(child->getNameString());
      if( style != _parent->_style.end() )
        return style->second;
    }

    // ...or reset to default if none is available
    return child; // TODO somehow get default value for each style?
  }

  //----------------------------------------------------------------------------
  void Element::setDrawable( osg::Drawable* drawable )
  {
    _drawable = drawable;
    assert( _drawable );

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(_drawable);
    _transform->addChild(geode);
  }

  //----------------------------------------------------------------------------
  osg::StateSet* Element::getOrCreateStateSet()
  {
    return _drawable ? _drawable->getOrCreateStateSet()
                     : _transform->getOrCreateStateSet();
  }

  //----------------------------------------------------------------------------
  void Element::setupStyle()
  {
    BOOST_FOREACH( Style::value_type style, _style )
      setStyle(style.second);
  }

} // namespace canvas
} // namespace simgear
