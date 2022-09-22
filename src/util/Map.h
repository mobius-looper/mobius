/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An approximation of java.util.Map because I can't stand STL.
 * 
 */

#ifndef MAP_UTIL_H
#define MAP_UTIL_H

/**
 * The default map has string keys and object values.
 * The values are not owned and won't be freed.
 */
class Map {
  public:

    Map();
    Map(int initialSize);
    virtual ~Map();

    void put(const char* key, void* value);
    void* get(const char* key);

  private:

    void init(int size);

    void* mMap;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
