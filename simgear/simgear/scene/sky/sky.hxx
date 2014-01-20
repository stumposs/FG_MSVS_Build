/**
 * \file sky.hxx
 * Provides a class to model a realistic (time/date/position) based sky.
 */

// Written by Curtis Olson, started December 1997.
// SSG-ified by Curtis Olson, February 2000.
//
// Copyright (C) 1997-2000  Curtis L. Olson  - http://www.flightgear.org/~curt
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$


#ifndef _SG_SKY_HXX
#define _SG_SKY_HXX


#ifndef __cplusplus
# error This library requires C++
#endif

#include <simgear/compiler.h>
#include <simgear/math/sg_random.h>
#include <simgear/misc/sg_path.hxx>
#include <simgear/props/props.hxx>

#include <vector>

#include <osg/ref_ptr>
#include <osg/MatrixTransform>
#include <osg/Node>
#include <osg/Switch>

#include <simgear/ephemeris/ephemeris.hxx>
#include <simgear/math/SGMath.hxx>

#include <simgear/scene/sky/cloud.hxx>
#include <simgear/scene/sky/dome.hxx>
#include <simgear/scene/sky/moon.hxx>
#include <simgear/scene/sky/oursun.hxx>
#include <simgear/scene/sky/stars.hxx>

namespace simgear {
class SGReaderWriterOptions;
}

typedef struct {
    SGVec3d pos;
    SGGeod pos_geod;
    SGQuatd ori;
    double spin;
	double gst;
	double sun_dist;
	double moon_dist;
	double sun_angle;
} SGSkyState;

typedef struct {
	SGVec3f sky_color;
    SGVec3f adj_sky_color;
    SGVec3f fog_color;
	SGVec3f cloud_color;
	double sun_angle, moon_angle;
} SGSkyColor;

/**
 * A class to model a realistic (time/date/position) based sky.
 *
 * Introduction 
 *
 * The SGSky class models a blended sky dome, a haloed sun, a textured
 * moon with phase that properly matches the date, stars and planets,
 * and cloud layers. SGSky is designed to be dropped into existing
 * plib based applications and depends heavily on plib's scene graph
 * library, ssg. The sky implements various time of day lighting
 * effects, it plays well with fog and visibility effects, and
 * implements scudded cloud fly-through effects. Additionally, you can
 * wire in the output of the SGEphemeris class to accurately position
 * all the objects in the sky.
 *
 * Building the sky 
 *

 * Once you have created an instance of SGSky you must call the
 * build() method.  Building the sky requires several textures. So,
 * you must specify the path/directory where these textures reside
 * before building the sky.  You do this first by calling the
 * texture_path() method.

 * The arguments you pass to the build() method allow you to specify
 * the horizontal and vertical radiuses of the sky dome, the size of
 * your sun sphere and moon sphere, a number of planets, and a
 * multitude of stars.  For the planets and stars you pass in an array
 * of right ascensions, declinations, and magnitudes.

 * Cloud Layers 

 * Cloud layers can be added, changed, or removed individually. To add
 * a cloud layer use the add_cloud_layer() method.  The arguments
 * allow you to specify base height above sea level, layer thickness,
 * a transition zone for entering/leaving the cloud layer, the size of
 * the cloud object, and the type of cloud texture. All distances are
 * in meters. There are additional forms of this method that allow you
 * to specify your own ssgSimpleState or texture name for drawing the
 * cloud layer.

 * Repainting the Sky 

 * As the sun circles the globe, you can call the repaint() method to
 * recolor the sky objects to simulate sunrise and sunset effects,
 * visibility, and other lighting changes.  The arguments allow you to
 * specify a base sky color (for the top of the dome), a fog color
 * (for the horizon), the sun angle with the horizon (for
 * sunrise/sunset effects), the moon angle (so we can make it more
 * yellow at the horizon), and new star and planet data so that we can
 * optionally change the magnitude of these (for day / night
 * transitions.)

 * Positioning Sky Objects 

 * As time progresses and as you move across the surface of the earth,
 * the apparent position of the objects and the various lighting
 * effects can change. the reposition() method allows you to specify
 * the positions of all the sky objects as well as your view position.
 * The arguments allow you to specify your view position in world
 * Cartesian coordinates, the zero elevation position in world
 * Cartesian coordinates (your longitude, your latitude, sea level),
 * the ``up'' vector in world Cartesian coordinates, current
 * longitude, latitude, and altitude. A ``spin'' angle can be
 * specified for orienting the sky with the sun position so sunset and
 * sunrise effects look correct. You must specify GMT side real time,
 * the sun right ascension, sun declination, and sun distance from
 * view point (to keep it inside your view volume.) You also must
 * specify moon right ascension, moon declination, and moon distance
 * from view point.

 * Rendering the Sky 

 * The sky is designed to be rendered in three stages. The first stage
 * renders the parts that form your back drop - the sky dome, the
 * stars and planets, the sun, and the moon.  These should be rendered
 * before the rest of your scene by calling the preDraw() method. The
 * second stage renders the clouds that are above the viewer. This stage 
 * is done before translucent objects in the main scene are drawn. It 
 * is seperated from the preDraw routine to enable to implement a 
 * multi passes technique and is located in the drawUpperClouds() method.
 * The third stage renders the clouds that are below the viewer an which 
 * are likely to be translucent (depending on type) and should be drawn 
 * after your scene has been rendered.  Use the drawLowerClouds() method 
 * to draw the second stage of the sky.

 * A typical application might do the following: 

 * <li> thesky->preDraw( my_altitude );
 * <li> thesky->drawUpperClouds();
 * <li> ssgCullAndDraw ( myscene ) ;
 * <li> thesky->drawLowerClouds();

 * The current altitude in meters is passed to the preDraw() method
 * so the clouds layers can be rendered correction from most distant
 * to closest.

 * Visibility Effects 

 * Visibility and fog is important for correctly rendering the
 * sky. You can inform SGSky of the current visibility by calling the
 * set_visibility() method.

 * When transitioning through clouds, it is nice to pull in the fog as
 * you get close to the cloud layer to hide the fact that the clouds
 * are drawn as a flat polygon. As you get nearer to the cloud layer
 * it is also nice to temporarily pull in the visibility to simulate
 * the effects of flying in and out of the puffy edge of the
 * cloud. These effects can all be accomplished by calling the
 * modify_vis() method.  The arguments allow you to specify your
 * current altitude (which is then compared to the altitudes of the
 * various cloud layers.) You can also specify a time factor which
 * should be the length in seconds since the last time you called
 * modify_vis(). The time_factor value allows the puffy cloud effect
 * to be calculated correctly.

 * The modify_vis() method alters the SGSky's internal idea of
 * visibility, so you should subsequently call get_visibility() to get
 * the actual modified visibility. You should then make the
 * appropriate glFog() calls to setup fog properly for your scene.

 * Accessor Methods 

 * Once an instance of SGSky has been successfully initialized, there
 * are a couple accessor methods you can use such as get_num_layers()
 * to return the number of cloud layers, get_cloud_layer(i) to return
 * cloud layer number i, get_visibility() to return the actual
 * visibility as modified by the sky/cloud model.

 */

class SGSky {

private:
    typedef std::vector<SGSharedPtr<SGCloudLayer> > layer_list_type;
    typedef layer_list_type::iterator layer_list_iterator;
    typedef layer_list_type::const_iterator layer_list_const_iterator;

    // components of the sky
    SGSharedPtr<SGSkyDome> dome;
    SGSharedPtr<SGSun> oursun;
    SGSharedPtr<SGMoon> moon;
    SGSharedPtr<SGStars> planets;
    SGSharedPtr<SGStars> stars;
    layer_list_type cloud_layers;

    osg::ref_ptr<osg::Group> pre_root, cloud_root;
    osg::ref_ptr<osg::Switch> pre_selector;
    osg::ref_ptr<osg::Group> pre_transform;

    osg::ref_ptr<osg::MatrixTransform> _ephTransform;

    SGPath tex_path;

    // visibility
    float visibility;
    float effective_visibility;
    float minimum_sky_visibility;

    int in_cloud;
    int cur_layer_pos;

    // near cloud visibility state variables
    bool in_puff;
    double puff_length;		// in seconds
    double puff_progression;	// in seconds
    double ramp_up;		// in seconds
    double ramp_down;		// in seconds

    // 3D clouds enabled
    bool clouds_3d_enabled;

    // 3D cloud density
    double clouds_3d_density;
    
    // RNG seed
    mt seed;

public:

    /** Constructor */
    SGSky( void );

    /** Destructor */
    ~SGSky( void );

    /**
     * Initialize the sky and connect the components to the scene
     * graph at the provided branch.  See discussion in detailed class
     * description.
     * @param h_radius_m horizontal radius of sky dome
     * @param v_radius_m vertical radius of sky dome
     * @param sun_size size of sun
     * @param moon_size size of moon
     * @param nplanets number of planets
     * @param planet_data an array of planet right ascensions, declinations,
     *        and magnitudes
     * @param nstars number of stars
     * @param star_data an array of star right ascensions, declinations,
     *        and magnitudes
     */
    void build( double h_radius_m, double v_radius_m,
                double sun_size, double moon_size,
                const SGEphemeris& eph, SGPropertyNode *property_tree_node,
                simgear::SGReaderWriterOptions* options);

    /**
     * Repaint the sky components based on current value of sun_angle,
     * sky, and fog colors.  You can also specify new star and planet
     * data so that we can optionally change the magnitude of these
     * (for day/night transitions.)  See discussion in detailed
     * class description.
     *
     * Sun and moon angles are specified in degrees relative to local up
     * <li> 0 degrees = high noon
     * <li> 90 degrees = sun rise/set
     * <li> 180 degrees = darkest midnight
     * @param sky_color the base sky color (for the top of the dome)
     * @param fog_color the fog color (for the horizon)
     * @param sun_angle the sun angle with the horizon (for sunrise/sunset
     *        effects)
     * @param moon_angle the moon angle (so we can make it more yellow
     *        at the horizon)
     * @param nplanets number of planets
     * @param planet_data an array of planet right ascensions, declinations,
     *        and magnitudes
     * @param nstars number of stars
     * @param star_data an array of star right ascensions, declinations,
     *        and magnitudes
     */
    bool repaint( const SGSkyColor &sc, const SGEphemeris& eph );

    /**
     * Reposition the sky at the specified origin and orientation
     *
     * lon specifies a rotation about the Z axis
     * lat specifies a rotation about the new Y axis
     * spin specifies a rotation about the new Z axis (this allows
     * additional orientation for the sunrise/set effects and is used
     * by the skydome and perhaps clouds.  See discussion in detailed
     * class description.
     * @param view_pos specify your view position in world Cartesian
     *        coordinates
     * @param zero_elev the zero elevation position in world Cartesian
     *        coordinates
     * @param view_up the up vector in world Cartesian coordinates
     * @param lon current longitude
     * @param lat current latitude
     * @param alt current altitude
     * @param spin an offset angle for orienting the sky effects with the
     *        sun position so sunset and sunrise effects look correct.
     * @param gst GMT side real time
     * @param sun_ra the sun's current right ascension
     * @param sun_dec the sun's current declination
     * @param sun_dist the sun's distance from the current view point
     *        (to keep it inside your view volume.)
     * @param moon_ra the moon's current right ascension
     * @param moon_dec the moon's current declination
     * @param moon_dist the moon's distance from the current view point. 
     */
    bool reposition( const SGSkyState &st, const SGEphemeris& eph, double dt = 0.0 );

    /**
     * Modify the given visibility based on cloud layers, thickness,
     * transition range, and simulated "puffs".  See discussion in detailed
     * class description.
     * @param alt current altitude
     * @param time_factor amount of time since modify_vis() last called so
     *        we can scale effect rates properly despite variable frame rates.
     */
    void modify_vis( float alt, float time_factor );

    osg::Node* getPreRoot() { return pre_root.get(); }
    osg::Node* getCloudRoot() { return cloud_root.get(); }

    /** 
     * Specify the texture path (optional, defaults to current directory)
     * @param path base path to texture locations
     */
    void texture_path( const string& path );

    /** Enable drawing of the sky. */
    inline void enable() {
        pre_selector->setValue(0, 1);
    }

    /**
     * Disable drawing of the sky in the scene graph.  The leaf node is still
     * there, how ever it won't be traversed on by ssgCullandRender()
     */
    inline void disable() {
        pre_selector->setValue(0, 0);
    }

    /**
     * Get the current sun color
     */
    inline SGVec4f get_sun_color() { return oursun->get_color(); }

    /**
     * Get the current scene color
     */
    inline SGVec4f get_scene_color() { return oursun->get_scene_color(); }

    /**
     * Add a cloud layer.
     *
     * Transfer pointer ownership to this object.
     *
     * @param layer The new cloud layer to add.
     */
    void add_cloud_layer (SGCloudLayer * layer);


    /**
     * Get a cloud layer (const).
     *
     * Pointer ownership remains with this object.
     *
     * @param i The index of the cloud layer, zero-based.
     * @return A const pointer to the cloud layer.
     */
    const SGCloudLayer * get_cloud_layer (int i) const;


    /**
     * Get a cloud layer (non-const).
     *
     * Pointer ownership remains with this object.
     *
     * @param i The index of the cloud layer, zero-based.
     * @return A non-const pointer to the cloud layer.
     */
    SGCloudLayer * get_cloud_layer (int i);


    /**
     * Return the number of cloud layers currently available.
     *
     * @return The cloud layer count.
     */
    int get_cloud_layer_count () const;


    /** @return current effective visibility */
    inline float get_visibility() const { return effective_visibility; }

    /** Set desired clear air visibility.
     * @param v visibility in meters
     */
    inline void set_visibility( float v ) {
	effective_visibility = visibility = (v <= 25.0) ? 25.0 : v;
    }

    /** Get 3D cloud density */
    double get_3dCloudDensity() const;

    /** Set 3D cloud density 
     * @param density 3D cloud density
     */
    void set_3dCloudDensity(double density);

    /** Get 3D cloud visibility range*/
    float get_3dCloudVisRange() const;

    /** Set 3D cloud visibility range
     * @param density 3D cloud visibility range
     */
    void set_3dCloudVisRange(float vis);

    /** Get 3D cloud impostor distance*/
    float get_3dCloudImpostorDistance() const;

    /** Set 3D cloud impostor distance
     * @param density 3D cloud impostor distance
     */
    void set_3dCloudImpostorDistance(float vis);

    /** Get 3D cloud LoD1 Range*/
    float get_3dCloudLoD1Range() const;

    /** Set 3D cloud LoD1 Range
     * @param vis LoD1 Range
     */
    void set_3dCloudLoD1Range(float vis);

    /** Get 3D cloud LoD2 Range*/
    float get_3dCloudLoD2Range() const;

    /** Set 3D cloud LoD2 Range
     * @param vis LoD2 Range
     */
    void set_3dCloudLoD2Range(float vis);

    /** Get 3D cloud impostor usage */
    bool get_3dCloudUseImpostors() const;

    /** Set 3D cloud impostor usage
     * @param wrap whether use impostors for 3D clouds
     */
    void set_3dCloudUseImpostors(bool imp);

    /** Get 3D cloud wrapping */
    bool get_3dCloudWrap() const;

    /** Set 3D cloud wrapping
     * @param wrap whether to wrap 3D clouds
     */
    void set_3dCloudWrap(bool wrap);


    /** Get minimum sky visibility */
    float get_minimum_sky_visibility() const;

    /** Set minimum sky visibility */
    void set_minimum_sky_visibility( float value );
};
#endif // _SG_SKY_HXX
