
// queue.cxx -- Audio sample encapsulation class
// 
// Written by Curtis Olson, started April 2004.
// Modified to match the new SoundSystem by Erik Hofman, October 2009
//
// Copyright (C) 2004  Curtis L. Olson - http://www.flightgear.org/~curt
// Copyright (C) 2009 Erik Hofman <erik@ehofman.com>
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
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
// $Id$

#ifdef HAVE_CONFIG_H
#  include <simgear_config.h>
#endif

#include <cstdlib>	// rand()
#include <cstring>

#include <simgear/debug/logstream.hxx>
#include <simgear/structure/exception.hxx>
#include <simgear/misc/sg_path.hxx>

#include "soundmgr_openal.hxx"
#include "sample_queue.hxx"
#include "soundmgr_openal_private.hxx"

using std::string;

//
// SGSampleQueue
//

// empty constructor
SGSampleQueue::SGSampleQueue( int freq, int format ) :
    _refname(random_string()),
    _playing(false)
{
    _freq = freq;
    _format = format;
    _buffers.clear();
}

SGSampleQueue::~SGSampleQueue() {
    stop();
}

void SGSampleQueue::stop()
{
#ifdef ENABLE_SOUND
    ALint num;
    alGetSourcei(_source, AL_BUFFERS_PROCESSED, &num);
    for (int i=0; i<num; i++) {
        ALuint buffer;
        alSourceUnqueueBuffers(_source, 1, &buffer);
        alDeleteBuffers(1, &buffer);
    }
    _buffers.clear();
#endif
    _playing = false;
    _changed = true;
}

void SGSampleQueue::add( const void* smp_data, size_t len )
{
#ifdef ENABLE_SOUND
    const ALvoid *data = (const ALvoid *)smp_data;
    ALuint buffer;
    ALint num;

    if ( _valid_source )
    {
       alGetSourcei(_source, AL_BUFFERS_PROCESSED, &num);
       if (num > 1) {
           alSourceUnqueueBuffers(_source, 1, &buffer);
       } else {
           alGenBuffers(1, &buffer);
       }
       alBufferData(buffer, _format, data, len, _freq);
    }
    else
    {
        alGenBuffers(1, &buffer);
        alBufferData(buffer, _format, data, len, _freq);
        _buffers.push_back(buffer);
    }
#endif
}

void SGSampleQueue::set_source( unsigned int sid )
{
    SGSoundSample::set_source(sid);
#ifdef ENABLE_SOUND
    ALuint num = _buffers.size();
    for (unsigned int i=0; i < num; i++)
    {
        ALuint buffer = _buffers[i];
        alSourceQueueBuffers(_source, 1, &buffer);
    }
    _buffers.clear();
#endif
}

string SGSampleQueue::random_string() {
      static const char *r = "0123456789abcdefghijklmnopqrstuvwxyz"
                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
      string rstr = "Queued sample: ";
      for (int i=0; i<10; i++) {
          rstr.push_back( r[rand() % strlen(r)] );
      }

      return rstr;
}
