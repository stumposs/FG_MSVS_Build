///@file
/// Call context for Nasal extension functions
///
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

#ifndef SG_NASAL_CALL_CONTEXT_HXX_
#define SG_NASAL_CALL_CONTEXT_HXX_

#include "from_nasal.hxx"
#include "to_nasal.hxx"

namespace nasal
{

  /**
   * Context passed to a function/method being called from Nasal
   */
  class CallContext
  {
    public:
      CallContext(naContext c, size_t argc, naRef* args):
        c(c),
        argc(argc),
        args(args)
      {}

      bool isNumeric(size_t index) const
      {
        return (index < argc && naIsNum(args[index]));
      }

      bool isString(size_t index) const
      {
        return (index < argc && naIsString(args[index]));
      }

      bool isHash(size_t index) const
      {
        return (index < argc && naIsHash(args[index]));
      }

      bool isVector(size_t index) const
      {
        return (index < argc && naIsVector(args[index]));
      }

      bool isGhost(size_t index) const
      {
        return (index < argc && naIsGhost(args[index]));
      }

      void popFront(size_t num = 1)
      {
        if( argc < num )
          return;

        args += num;
        argc -= num;
      }

      void popBack(size_t num = 1)
      {
        if( argc < num )
          return;

        argc -= num;
      }

      /**
       * Get the argument with given index if it exists. Otherwise returns the
       * passed default value.
       *
       * @tparam T    Type of argument (converted using ::from_nasal)
       * @param index Index of requested argument
       * @param def   Default value returned if too few arguments available
       */
      template<class T>
      typename from_nasal_ptr<T>::return_type
      getArg(size_t index, const T& def = T()) const
      {
        if( index >= argc )
          return def;

        return from_nasal<T>(args[index]);
      }

      /**
       * Get the argument with given index. Raises a Nasal runtime error if
       * there are to few arguments available.
       */
      template<class T>
      typename from_nasal_ptr<T>::return_type
      requireArg(size_t index) const
      {
        if( index >= argc )
          naRuntimeError(c, "Missing required arg #%d", index);

        return from_nasal<T>(args[index]);
      }

      template<class T>
      naRef to_nasal(T arg) const
      {
        return nasal::to_nasal(c, arg);
      }

      template<class T>
      typename from_nasal_ptr<T>::return_type
      from_nasal(naRef ref) const
      {
        return (*from_nasal_ptr<T>::get())(c, ref);
      }

      naContext   c;
      size_t      argc;
      naRef      *args;
  };

} // namespace nasal


#endif /* SG_NASAL_CALL_CONTEXT_HXX_ */
