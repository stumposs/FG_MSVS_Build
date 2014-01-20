// Copyright (C) 2008  Timothy Moore timoore@redhat.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef SIMGEAR_EFFECT_GEODE_HXX
#define SIMGEAR_EFFECT_GEODE_HXX 1

#include <osg/Geode>

#include "Effect.hxx"

namespace simgear
{
class EffectGeode : public osg::Geode
{
public:
    EffectGeode();
    EffectGeode(const EffectGeode& rhs,
                const osg::CopyOp& copyop = osg::CopyOp::SHALLOW_COPY);
    META_Node(simgear,EffectGeode);
    Effect* getEffect() const { return _effect.get(); }
    void setEffect(Effect* effect);
    virtual void resizeGLObjectBuffers(unsigned int maxSize);
    virtual void releaseGLObjects(osg::State* = 0) const;
    typedef DrawableList::iterator DrawablesIterator;
    DrawablesIterator drawablesBegin() { return _drawables.begin(); }
    DrawablesIterator drawablesEnd() { return _drawables.end(); }
    void runGenerators(osg::Geometry *geometry);
private:
    osg::ref_ptr<Effect> _effect;
};
}
#endif
